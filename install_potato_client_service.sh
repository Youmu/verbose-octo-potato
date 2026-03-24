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

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <POTATO_EP> <POTATO_KEY> [SOURCE_BIN]"
  echo "Example: $0 http://127.0.0.1:8000/potato/msg BASE64_AES256_KEY"
  echo "Build first from project root:"
  echo "  cmake -S client -B build/release -DCMAKE_BUILD_TYPE=Release"
  echo "  cmake --build build/release -j"
  exit 1
fi

POTATO_EP="$1"
POTATO_KEY="$2"
SOURCE_BIN="${3:-$DEFAULT_SOURCE_BIN}"

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
SERIAL_DEVICE=/dev/ttyAMA0
SERIAL_BAUDRATE=115200
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
echo "Logs:"
echo "  $LOG_FILE"
