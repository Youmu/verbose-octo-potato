#include <iostream>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>
#include "mms_monitor/mms_monitor.hpp"
#include "mms_monitor/serial_interface.hpp"
#include "mms_monitor/at_controller.hpp"
#include "mms_monitor/potato_client.hpp"

using namespace mms_monitor;

namespace {

SerialInterface* g_serial_interface = nullptr;
AtController* g_at_controller = nullptr;
PotatoClient* g_potato_client = nullptr;
std::atomic<bool> g_running{true};

std::string CurrentUtcIso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  const auto sec = std::chrono::duration_cast<std::chrono::seconds>(epoch_ms);
  const auto ms = epoch_ms - sec;

  const std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
  gmtime_r(&tt, &utc_tm);

  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S")
      << '.' << std::setw(3) << std::setfill('0') << ms.count()
      << 'Z';
  return oss.str();
}

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

void HandleSignal(int signal_number) {
  std::cout << "Received signal " << signal_number << ", shutting down\n";
  g_running = false;
}

int ParseBaudrate(const char* value) {
  if (value == nullptr || std::string(value).empty()) {
    return 115200;
  }
  const int parsed = std::stoi(value);
  if (parsed <= 0) {
    throw std::invalid_argument("SERIAL_BAUDRATE must be a positive integer");
  }
  return parsed;
}

}  // namespace

int main() {
  try {
    // Force line-by-line flush when running as a non-interactive service.
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    std::cout << mms_monitor::build_banner() << '\n';
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    const char* potato_ep = std::getenv("POTATO_EP");
    const char* potato_key = std::getenv("POTATO_KEY");
    if (potato_ep == nullptr || potato_key == nullptr || std::string(potato_ep).empty() ||
        std::string(potato_key).empty()) {
      std::cerr << "POTATO_EP and POTATO_KEY must be set\n";
      return 1;
    }

    const char* serial_device = std::getenv("SERIAL_DEVICE");
    const std::string serial_path =
        (serial_device == nullptr || std::string(serial_device).empty()) ? "/dev/ttyS0" : serial_device;
    const int serial_baudrate = ParseBaudrate(std::getenv("SERIAL_BAUDRATE"));

    SerialInterface serial_interface(serial_path, serial_baudrate);
    PotatoClient potato_client(potato_ep, potato_key);
    
    const char* potato_auth_token = std::getenv("POTATO_AUTH_TOKEN");
    if (potato_auth_token != nullptr && !std::string(potato_auth_token).empty()) {
      potato_client.SetAuthToken(potato_auth_token);
    }

    AtController at_controller;

    g_potato_client = &potato_client;
    g_serial_interface = &serial_interface;
    g_at_controller = &at_controller;

    at_controller.SetOnSmsReceived([](const std::string& sender, const std::string& body) {
      std::cout << "Received SMS from " << sender << " with body: " << body << '\n';
      if (g_potato_client != nullptr) {
        g_potato_client->PushMessage(CurrentUtcIso8601(), sender, body);
        return true;
      }
      return false;
    });

    at_controller.SetOnSmsNewMsg([&at_controller]() {
      std::cout << "New message arrived\n";
      at_controller.ListSms(AtController::SmsStatus::kRecUnread);
    });
    at_controller.SetOnSend(ForwardSend);
    serial_interface.SetOnReceive(ForwardReceive);
    serial_interface.Start();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    at_controller.Init();
    at_controller.ListSms(AtController::SmsStatus::kRecUnread);
    std::cerr << "potato_client service started\n";

    while (g_running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    serial_interface.Stop();
    std::cout << "Service stopped\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << '\n';
    return 1;
  }
}
