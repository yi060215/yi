#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/root/tcm-cloud"

sudo apt update
sudo apt install -y python3 python3-venv python3-pip

mkdir -p "$APP_DIR"
cp -r . "$APP_DIR"
cd "$APP_DIR"

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

sudo cp tcm-cloud.service /etc/systemd/system/tcm-cloud.service
sudo systemctl daemon-reload
sudo systemctl enable tcm-cloud
sudo systemctl restart tcm-cloud
sudo systemctl status tcm-cloud --no-pager
