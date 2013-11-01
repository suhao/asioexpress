//               Copyright Ross MacGregor 2013
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "AsioExpress/pch.hpp"

#include "AsioExpressConfig/config.hpp"

#include <boost/random.hpp>

#include "AsioExpress/Platform/DebugMessage.hpp"

#include "AsioExpress/MessagePort/Ipc/IpcErrorCodes.hpp"
#include "AsioExpress/MessagePort/Ipc/private/IpcSysMessage.hpp"

#include "AsioExpress/MessagePort/SyncIpc/SyncIpcEndPoint.hpp"
#include "AsioExpress/MessagePort/SyncIpc/private/SyncIpcCommandConnect.hpp"
#include "AsioExpress/MessagePort/SyncIpc/private/SyncIpcCommandReceive.hpp"

namespace
{
    boost::mt19937 rng(static_cast<boost::uint32_t>(std::time(0)));
}

namespace AsioExpress {
namespace MessagePort {
namespace SyncIpc {

//TODO: This code is copied from IPC source and should be refactored.

inline std::string IntToString(int n)   
{ 
    std::ostringstream ss; 
    ss << n; 
    return ss.str(); 
}

void SyncIpcCommandConnect(
        SyncIpcEndPoint const & endPoint, 
        SyncIpcMessagePort & messagePort)
{
    static int const LowConnectionId = 1;
    static int const HighConnectionId = 99;
    
    using namespace AsioExpress::MessagePort::Ipc;
    
    AsioExpress::MessagePort::DataBufferPointer dataBuffer(
        new AsioExpress::MessagePort::DataBuffer);

#ifdef DEBUG_IPC
    DebugMessage("MessagePortCommandConnect: Finding new message queue ID.\n");
#endif

    messagePort.Disconnect();

    //
    // Step 1 - Find a valid message queue name that is not in use
    //

    std::string clientQueueName;
    std::string serverQueueName;
    bool finished = false;

    while (!finished)
    {
        boost::uniform_int<> connectionIdRange(LowConnectionId, HighConnectionId);
        boost::variate_generator<boost::mt19937&, boost::uniform_int<> > randomNumber(rng, connectionIdRange);
        std::string id = IntToString(randomNumber());

        clientQueueName = endPoint.GetEndPoint() + "#Client#" + id;
        serverQueueName = endPoint.GetEndPoint() + "#Server#" + id;

        // Check that the queue doesn't already exist

        try
        {
            boost::interprocess::message_queue mq1(boost::interprocess::open_only, clientQueueName.c_str());
            // No exception thrown - this queue already exists. Try again.
            continue;
        }
        catch (boost::interprocess::interprocess_exception&)
        {
            // Couldn't open queue, so it doesn't exist. We can proceed.
        }

        try
        {
            boost::interprocess::message_queue mq1(boost::interprocess::open_only, serverQueueName.c_str());
            // No exception thrown - this queue already exists. Try again.
            continue;
        }
        catch (boost::interprocess::interprocess_exception&)
        {
            // Couldn't open queue, so it doesn't exist. We can proceed.
            finished = true;
        }
    }

    //
    // Step 2 - Create the message queues for client & server
    //
#ifdef DEBUG_IPC
    DebugMessage("MessagePortCommandConnect: Create new message queues.\n");
#endif

    try
    {
        messagePort.m_recvMessageQueueName = clientQueueName;
        messagePort.m_sendMessageQueueName = serverQueueName;
        messagePort.m_recvMessageQueue.reset(new boost::interprocess::message_queue(boost::interprocess::create_only, messagePort.m_recvMessageQueueName.c_str(), endPoint.GetMaxNumMsg(), endPoint.GetMaxMsgSize()));
        messagePort.m_sendMessageQueue.reset(new boost::interprocess::message_queue(boost::interprocess::create_only, messagePort.m_sendMessageQueueName.c_str(), endPoint.GetMaxNumMsg(), endPoint.GetMaxMsgSize()));
    }
    catch (boost::interprocess::interprocess_exception& ex)
    {
#ifdef DEBUG_IPC
        DebugMessage("MessagePortCommandConnect: Unable to create client/server message queues!\n");
#endif
        messagePort.Disconnect();
        throw CommonException(Error(
            boost::system::error_code(ex.get_native_error(), boost::system::get_system_category()),
            "MessagePort::Connect(): Unable to create client/server message queues."));
    }

    //
    // Step 3 - Try to connect to the server's acceptor message queue to
    //          notify it we want to connect & pass through the queue names.
    //

    try
    {
        boost::interprocess::message_queue acceptor(boost::interprocess::open_only, endPoint.GetEndPoint().c_str());

        IpcSysMessage msg(IpcSysMessage::MSG_CONNECT);
        msg.AddParam(messagePort.m_recvMessageQueueName);
        msg.AddParam(messagePort.m_sendMessageQueueName);

        char buf[1024];
        int len = msg.Encode(buf);

#ifdef DEBUG_IPC
        DebugMessage("MessagePortCommandConnect: Sending connect message.\n");
#endif
        if (!acceptor.try_send(buf, len, IpcSysMessage::SYS_MSG_PRIORITY))
        {
#ifdef DEBUG_IPC
            DebugMessage("MessagePortCommandConnect: Error sending connect message!\n");
#endif
            messagePort.Disconnect();
            
            throw CommonException(Error(
                    ErrorCode::CommunicationFailure,
                    "MessagePort::AsyncConnect(): Server's acceptor queue is full."));
        }
    }
    catch (boost::interprocess::interprocess_exception &ex)
    {
        messagePort.Disconnect();
        throw CommonException(Error(
                boost::system::error_code(ex.get_native_error(), boost::system::get_system_category()),
                "MessagePort::AsyncConnect(): Unable to open server acceptor queue."));
    }

    //
    // Step 4 - Asynchronously wait for a connection acknowledgement system message
    //          from the server to our receive message queue
    //

#ifdef DEBUG_IPC
    DebugMessage("MessagePortCommandConnect: Waiting for ACK message.\n");
#endif

    SyncIpcCommandReceive(
            messagePort.m_recvMessageQueue,
            dataBuffer,
            8000);

    //
    // Step 5 - Validate the connection acknowledgement
    //

    IpcSysMessage msg2;
    msg2.Decode(dataBuffer->Get());

    if (msg2.GetMessageType() != IpcSysMessage::MSG_CONNECT_ACK)
    {
#ifdef DEBUG_IPC
        DebugMessage("MessagePortCommandConnect: Invalid CONNECT-ACK response from server!\n");
#endif
        messagePort.Disconnect();
        throw CommonException(Error(
                ErrorCode::CommunicationFailure,
                "MessagePort::AsyncConnect(): Invalid CONNECT-ACK response from server."));
    }

    // Success
#ifdef DEBUG_IPC
    DebugMessage("MessagePortCommandConnect: Connected.\n");
#endif
}

} // namespace Ipc
} // namespace MessagePort
} // namespace AsioExpress