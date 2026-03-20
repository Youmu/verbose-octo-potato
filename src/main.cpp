#include <iostream>
#include <string>
#include "mms_monitor/mms_monitor.hpp"
#include "mms_monitor/serial_interface.hpp"

using namespace mms_monitor;

int main() {
  std::cout << mms_monitor::build_banner() << '\n';
  SerialInterface serial_interface("/dev/ttyS0", 115200);
  serial_interface.Start();
  serial_interface.SetOnReceive([](const std::string &message) {
    std::cout << "Received message: " << message << '\n';
  });
  std::string line;
  do{
    std::getline(std::cin, line);
    serial_interface.SendMessage(line);
  }while(line.compare("@exit") != 0);
  return 0;
}
