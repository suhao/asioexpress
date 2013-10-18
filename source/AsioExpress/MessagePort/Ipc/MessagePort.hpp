//               Copyright Ross MacGregor 2013
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include "AsioExpressError/EcToErrorAdapter.hpp"
#include "AsioExpress/MessagePort/DataBuffer.hpp"
#include "AsioExpress/CompletionHandler.hpp"
#include "AsioExpress/MessagePort/Ipc/EndPoint.hpp"
#include "AsioExpress/MessagePort/Ipc/private/MessageQueuePointer.hpp"
#include "AsioExpress/MessagePort/Ipc/private/ReceiveThread.hpp"
#include "AsioExpress/MessagePort/Ipc/private/SendThread.hpp"

namespace AsioExpress {
namespace MessagePort {
namespace Ipc {

class MessagePort
{
private:
  friend class MessagePortCommandConnect;
  friend class MessagePortCommandAccept;

public:
  typedef EndPoint EndPointType;

public:
  MessagePort(boost::asio::io_service & ioService);
  ~MessagePort();

  void AsyncConnect(
      EndPoint endPoint, 
      AsioExpress::CompletionHandler completionHandler);

  void AsyncSend(
      AsioExpress::MessagePort::DataBufferPointer buffer, 
      AsioExpress::CompletionHandler completionHandler);

  void AsyncReceive(
      AsioExpress::MessagePort::DataBufferPointer buffer, 
      AsioExpress::CompletionHandler completionHandler);

  void Disconnect();

  void SetMessagePortOptions();

  inline bool IsConnected() const               { return m_sendMessageQueue != 0; }
  inline const std::string& GetLocalID() const  { return m_recvMessageQueueName; }
  inline const std::string& GetRemoteID() const { return m_sendMessageQueueName; }

private:
  MessagePort & operator=(MessagePort const &);
  AsioExpress::Error SetupWithMessageQueues(const std::string& sendQueue, const std::string& recvQueue);

private:
  boost::asio::io_service &               m_ioService;
  MessageQueuePointer                     m_sendMessageQueue;
  std::string                             m_sendMessageQueueName;
  MessageQueuePointer                     m_recvMessageQueue;
  std::string                             m_recvMessageQueueName;
  ReceiveThreadPointer                    m_receiveThread;
  SendThreadPointer                       m_sendThread;
};


} // namespace Ipc
} // namespace MessagePort
} // namespace AsioExpress
