#include <iostream>
#include <string>
#include "mms_monitor/mms_monitor.hpp"
#include "mms_monitor/serial_interface.hpp"
#include "mms_monitor/at_controller.hpp"

using namespace mms_monitor;

namespace {

SerialInterface* g_serial_interface = nullptr;
AtController* g_at_controller = nullptr;

void ForwardSend(const std::string& message) {
  std::cout << "Sent message: " << message << '\n';
  if (g_serial_interface != nullptr) {
    g_serial_interface->SendMessage(message);
  }
}

void ForwardReceive(const std::string& message) {
  std::cout << "Received message: " << message << '\n';
  if (g_at_controller != nullptr) {
    g_at_controller->ReceiveMessage(message);
  }
}

}  // namespace

int main() {
  std::cout << mms_monitor::build_banner() << '\n';
  AtController at_controller;
  at_controller.SetOnSmsReceived([](const std::string &sender, const std::string &body) {
    std::cout << "Received SMS from " << sender << " with body: " << body << '\n';
    return false;
  });
  SerialInterface serial_interface("/dev/ttyS0", 115200);
  g_serial_interface = &serial_interface;
  g_at_controller = &at_controller;
  at_controller.SetOnSend(ForwardSend);
  serial_interface.Start();
  serial_interface.SetOnReceive(ForwardReceive);
  std::string line;
  do{
    std::getline(std::cin, line);
    if(line.compare("list unread") == 0) {
      at_controller.ListSms(AtController::SmsStatus::kRecUnread);
    } else if(line.compare("list all") == 0) {
      at_controller.ListSms(AtController::SmsStatus::kAll);
    }
  }while(line.compare("@exit") != 0);
  return 0;
}
