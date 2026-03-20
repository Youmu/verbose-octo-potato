#include "mms_monitor/serial_interface.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace mms_monitor {
namespace {

speed_t ToPosixBaud(int boudrate) {
  switch (boudrate) {
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
    default:
      throw std::invalid_argument("Unsupported boudrate: " + std::to_string(boudrate));
  }
}

void ConfigureSerialPort(int fd, int boudrate) {
  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    throw std::system_error(errno, std::generic_category(), "tcgetattr failed");
  }

  const speed_t speed = ToPosixBaud(boudrate);
  if (cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
    throw std::system_error(errno, std::generic_category(), "cfset speed failed");
  }

  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag |= CREAD | CLOCAL;
  tty.c_cflag &= ~CRTSCTS;

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_oflag &= ~OPOST;

  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    throw std::system_error(errno, std::generic_category(), "tcsetattr failed");
  }
}

}  // namespace

SerialInterface::SerialInterface(const std::string& device, int boudrate)
    : device_(device), boudrate_(boudrate) {
  fd_ = open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(), "open failed for " + device_);
  }
  try {
    ConfigureSerialPort(fd_, boudrate_);
  } catch (...) {
    close(fd_);
    fd_ = -1;
    throw;
  }
}

SerialInterface::~SerialInterface() {
  running_ = false;
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

void SerialInterface::Start() {
  bool expected = false;
  if (!started_.compare_exchange_strong(expected, true)) {
    return;
  }
  running_ = true;
  reader_thread_ = std::thread(&SerialInterface::ReadLoop, this);
}

void SerialInterface::SetOnReceive(SerialReceiveCb onReceive) { on_receive_.store(onReceive); }

void SerialInterface::SendMessage(const std::string& msg) const {
  if (fd_ < 0) {
    throw std::runtime_error("Serial device is not open");
  }
  std::lock_guard<std::mutex> lock(write_mutex_);

  std::string payload = msg;
  payload.push_back('\r');
  size_t total = 0;
  while (total < payload.size()) {
    const ssize_t written = write(fd_, payload.c_str() + total, payload.size() - total);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(), "write failed");
    }
    total += static_cast<size_t>(written);
  }
}

void SerialInterface::ReadLoop() {
  std::string line;
  line.reserve(256);

  while (running_) {
    char ch = '\0';
    const ssize_t count = read(fd_, &ch, 1);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (count == 0) {
      continue;
    }

    if (ch == '\r' || ch == '\n') {
      const SerialReceiveCb cb = on_receive_.load();
      if (!line.empty() && cb != nullptr) {
        cb(line);
      }
      line.clear();
      continue;
    }
    line.push_back(ch);
  }
}

}  // namespace mms_monitor
