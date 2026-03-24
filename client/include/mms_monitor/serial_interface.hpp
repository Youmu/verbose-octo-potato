#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace mms_monitor {

typedef void (*SerialReceiveCb)(const std::string& message);

class SerialInterface {
 public:
  SerialInterface(const std::string& device, int boudrate);
  ~SerialInterface();

  SerialInterface(const SerialInterface&) = delete;
  SerialInterface& operator=(const SerialInterface&) = delete;

  void Start();
  void Stop();
  void SetOnReceive(SerialReceiveCb onReceive);
  void SendMessage(const std::string& msg) const;

 private:
  void ReadLoop();

  int fd_{-1};
  std::string device_;
  int boudrate_{0};
  std::atomic<SerialReceiveCb> on_receive_{nullptr};
  std::atomic<bool> started_{false};
  std::atomic<bool> running_{false};
  mutable std::mutex write_mutex_;
  std::thread reader_thread_;
};

}  // namespace mms_monitor
