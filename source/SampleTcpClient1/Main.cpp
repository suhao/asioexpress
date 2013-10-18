//               Copyright Ross MacGregor 2013
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "SampleTcpClient1/pch.hpp"

#include <iostream>

#include "SampleTcpClient1/App.hpp"

boost::function0<void> console_ctrl_function;

BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
  switch (ctrl_type)
  {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    console_ctrl_function();
    return TRUE;
  default:
    return FALSE;
  }
}

class ShutDownFunc
{
public:
  ShutDownFunc(
      boost::asio::io_service & ioService,
      App & app) :
    ioService(ioService),
    app(app)
  {
  }

  void operator()()
  {
    // The App::Stop is posted to the io_service to make sure it is 
    // it is performed in the correct thread.
    ioService.post(
       boost::bind(
          &App::Stop, 
          &app));
  }

private:
  boost::asio::io_service &   ioService;
  App &                       app;
};


int main(int argc, char* argv[])
{
  if (argc != 4)
  {
    std::cout << "Usage: SampleClient1 <clientName> <server ip> <server port>" << std::endl
              << "Example: SampleClient1 test001 128.10.10.104 18000";

    return 1;
  }

  AsioExpress::MessagePort::Tcp::EndPoint testEndPoint(argv[2], argv[3]);

  boost::asio::io_service ioService;  

  App app(ioService, testEndPoint, argv[1]);

  // Set console control handler to allow client to be stopped.
  console_ctrl_function = ShutDownFunc(ioService, app);
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

  app.Start();

  ioService.run();

	return 0;
}
