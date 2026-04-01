#pragma once
#include <string>
#include <functional>
#include "driver/uart.h"

namespace espclient {

//typedef void (*SerialReceiveCb)(const std::string& message);

class SerialInterface {
 public:
  SerialInterface(int uart_port, int boudrate);
  ~SerialInterface();

  SerialInterface(const SerialInterface&) = delete;
  SerialInterface& operator=(const SerialInterface&) = delete;

  void Start();
  void Stop();
  void SetOnReceive(std::function<void(const std::string)> onReceive);
  void SendMessage(const std::string& msg) const;

 private:
  uint8_t *buffer;

  void DoReadLoop();

  // UART
  uart_port_t uart_port_{UART_NUM_1};
  int baudrate_{115200};

  // Callback
  std::function<void(const std::string)> on_receive_;

  // Status
  bool started_{false};
  bool running_{false};
  bool stop_requested_{false};
  TaskHandle_t task_handle_;
};

}  // namespace mms_monitor
