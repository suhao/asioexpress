//               Copyright Ross MacGregor 2013
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "AsioExpress/pch.hpp"

#include "AsioExpressConfig/config.hpp"

#include "AsioExpress/MessagePort/Ipc/private/MessagePortCommandAccept.hpp"
#include "AsioExpress/MessagePort/Ipc/private/MessagePortSysMessage.hpp"
#include "AsioExpress/MessagePort/Ipc/private/MessagePortCommandReceive.hpp"
#include "AsioExpress/MessagePort/Ipc/IpcErrorCodes.hpp"
#include "AsioExpress/Yield.hpp" // Enable the pseudo-keywords REENTER, YIELD and FORK.

namespace AsioExpress {
namespace MessagePort {
namespace Ipc {

void MessagePortCommandAccept::operator() (AsioExpress::Error e)
{
  if (e)
  {
    m_acceptor.Close();
    CallCompletionHandler(e);
    return;
  }

  REENTER(this)
  {
    // 
    // Step 1 - Wait for a valid CONNECT message to arrive
    //

    YIELD 
    {
#ifdef DEBUG_IPC
      OutputDebugString("MessagePortCommandAccept: Waiting for connect message.\n");
#endif

      MessagePortCommandReceive(m_acceptor.m_ioService,
                                m_acceptor.m_receiveThread,
                                m_acceptor.m_messageQueue,
                                m_tempBuffer,
                                *this)();
    }

    // Need to start a block here to avoid errors like:
    // C2360: initialization of 'serverQueueName' is skipped by 'case' label
    {
      // 
      // Step 2 - Process the message and check it's the right type
      //

      MessagePortSysMessage msg;
      msg.Decode(m_tempBuffer->Get());
      
      if ( msg.GetMessageType() != MessagePortSysMessage::MSG_CONNECT || msg.GetNumParams() != 2 )
      {
#ifdef DEBUG_IPC
        OutputDebugString("MessagePortCommandAccept: Invalid CONNECT command recieved!\n");
#endif

        CallCompletionHandler(
          ErrorCode::CommunicationFailure,
          "MessagePort::AsyncAccept(): Invalid CONNECT command recieved.");
        return;
      }

      //
      // Step 3 - Try to open the two message queues indicated by the message
      //

#ifdef DEBUG_IPC
      OutputDebugString("MessagePortCommandAccept: Connect message received. Setting up message queues...\n");
#endif

      AsioExpress::Error err = m_messagePort.SetupWithMessageQueues(msg.GetParam(0), msg.GetParam(1));
      if ( err )
      {
#ifdef DEBUG_IPC
        OutputDebugString("MessagePortCommandAccept: Error setting up message queues!\n");
#endif
        CallCompletionHandler(err);
        return;
      }

      //
      // Step 4 - Send a connection ACK and return success
      //

      MessagePortSysMessage msgack(MessagePortSysMessage::MSG_CONNECT_ACK);
      int len = msgack.Encode(m_tempBuffer->Get());
      
      try 
      {
#ifdef DEBUG_IPC
        OutputDebugString("MessagePortCommandAccept: Sending connection ACK.\n");
#endif
        if ( !m_messagePort.m_sendMessageQueue->try_send(m_tempBuffer->Get(), len, MessagePortSysMessage::SYS_MSG_PRIORITY) )
        {
#ifdef DEBUG_IPC
        OutputDebugString("MessagePortCommandAccept: Error sending connection ACK!\n");
#endif
          m_messagePort.Disconnect();
          CallCompletionHandler(
            ErrorCode::CommunicationFailure,
            "MessagePort::AsyncAccept(): Can't send connection ACK, client's recieve queue is full.");
          return;
        }
      }
      catch(boost::interprocess::interprocess_exception &ex) {    
        m_messagePort.Disconnect();
        CallCompletionHandler(
          boost::system::error_code(ex.get_native_error(), boost::system::system_category()),
          "MessagePort::AsyncAccept(): Unable to send to client's receive queue.");
        return;
      }
    }

    CallCompletionHandler(AsioExpress::Error());
  }
}

void MessagePortCommandAccept::CallCompletionHandler(
    boost::system::error_code errorCode,
    std::string message)
{
  CallCompletionHandler(AsioExpress::Error(errorCode, message));
}

void MessagePortCommandAccept::CallCompletionHandler(
    AsioExpress::Error err)
{
  m_acceptor.m_ioService.post(boost::asio::detail::bind_handler(m_completionHandler, err));
}

} // namespace Ipc
} // namespace MessagePort
} // namespace AsioExpress
