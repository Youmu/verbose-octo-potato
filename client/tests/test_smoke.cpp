#include <iostream>
#include <string>

#include "mms_monitor/mms_monitor.hpp"

int main() {
  const std::string msg = mms_monitor::build_banner();
  if (msg.empty()) {
    std::cerr << "build_banner() returned empty message\n";
    return 1;
  }
  return 0;
}
