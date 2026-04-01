#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="potato_client"
INSTALL_BIN="/usr/local/bin/${SERVICE_NAME}"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
ENV_FILE="/etc/default/${SERVICE_NAME}"
LOG_DIR="/var/log/${SERVICE_NAME}"
LOG_FILE="${LOG_DIR}/${SERVICE_NAME}.log"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
DEFAULT_SOURCE_BIN="${PROJECT_ROOT}/build/release/mms_monitor_app"

usage() {
  echo "Usage: $0 -e <POTATO_EP> -k <POTATO_KEY> [-d <SERIAL_DEVICE>] [-b <SERIAL_BAUDRATE>] [-t <POTATO_AUTH_TOKEN>] [-s <SOURCE_BIN>]"
  echo "Example: $0 -e http://127.0.0.1:8000/potato/msg -k BASE64_AES256_KEY -d /dev/ttyAMA0 -b 115200 -t TOKEN"
  echo "Build first from project root:"
  echo "  cmake -S client -B build/release -DCMAKE_BUILD_TYPE=Release"
  echo "  cmake --build build/release -j"
}

POTATO_EP=""
POTATO_KEY=""
SERIAL_DEVICE="/dev/ttyAMA0"
SERIAL_BAUDRATE="115200"
POTATO_AUTH_TOKEN=""
SOURCE_BIN="$DEFAULT_SOURCE_BIN"

while getopts ":e:k:d:b:t:s:h" opt; do
  case "${opt}" in
    e) POTATO_EP="${OPTARG}" ;;
    k) POTATO_KEY="${OPTARG}" ;;
    d) SERIAL_DEVICE="${OPTARG}" ;;
    b) SERIAL_BAUDRATE="${OPTARG}" ;;
    t) POTATO_AUTH_TOKEN="${OPTARG}" ;;
    s) SOURCE_BIN="${OPTARG}" ;;
    h)
      usage
      exit 0
      ;;
    :)
      echo "Option -${OPTARG} requires an argument"
      usage
      exit 1
      ;;
    \?)
      echo "Unknown option: -${OPTARG}"
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${POTATO_EP}" || -z "${POTATO_KEY}" ]]; then
  echo "Both -e <POTATO_EP> and -k <POTATO_KEY> are required"
  usage
  exit 1
fi

if [[ ! "${SERIAL_BAUDRATE}" =~ ^[0-9]+$ ]] || [[ "${SERIAL_BAUDRATE}" -le 0 ]]; then
  echo "Invalid baudrate: ${SERIAL_BAUDRATE}"
  echo "SERIAL_BAUDRATE must be a positive integer"
  exit 1
fi

if [[ ! -f "$SOURCE_BIN" ]]; then
  echo "Binary not found: $SOURCE_BIN"
  echo "Build first from project root:"
  echo "  cmake -S client -B build/release -DCMAKE_BUILD_TYPE=Release"
  echo "  cmake --build build/release -j"
  exit 1
fi

echo "Installing binary to $INSTALL_BIN"
sudo install -m 0755 "$SOURCE_BIN" "$INSTALL_BIN"

echo "Writing $ENV_FILE from arguments"
sudo tee "$ENV_FILE" >/dev/null <<EOF
POTATO_EP=${POTATO_EP}
POTATO_KEY=${POTATO_KEY}
SERIAL_DEVICE=${SERIAL_DEVICE}
SERIAL_BAUDRATE=${SERIAL_BAUDRATE}
POTATO_AUTH_TOKEN=${POTATO_AUTH_TOKEN}
EOF

echo "Preparing log file $LOG_FILE"
sudo install -d -m 0755 "$LOG_DIR"
sudo touch "$LOG_FILE"
sudo chmod 0644 "$LOG_FILE"

echo "Installing systemd unit $SERVICE_FILE"
sudo tee "$SERVICE_FILE" >/dev/null <<EOF
[Unit]
Description=Potato Client Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
EnvironmentFile=-$ENV_FILE
ExecStart=$INSTALL_BIN
Restart=always
RestartSec=3
User=root
StandardOutput=append:$LOG_FILE
StandardError=append:$LOG_FILE

[Install]
WantedBy=multi-user.target
EOF

echo "Reloading systemd and enabling service"
sudo systemctl daemon-reload
sudo systemctl enable --now "$SERVICE_NAME"
sudo systemctl status "$SERVICE_NAME" --no-pager

echo
echo "Service installed."
echo "Configured from arguments:"
echo "  POTATO_EP=${POTATO_EP}"
echo "  POTATO_KEY=<hidden>"
echo "  SERIAL_DEVICE=${SERIAL_DEVICE}"
echo "  SERIAL_BAUDRATE=${SERIAL_BAUDRATE}"
if [[ -n "${POTATO_AUTH_TOKEN}" ]]; then
  echo "  POTATO_AUTH_TOKEN=<hidden>"
else
  echo "  POTATO_AUTH_TOKEN=<not set>"
fi
echo "Logs:"
echo "  $LOG_FILE"
