#!/bin/bash
# ============================================================
# setup.sh — Environment bootstrap script for Chatroom Project
# Installs compiler, make, and utility tools on Ubuntu 22.04/24.04
# ============================================================

set -e  # Exit immediately if a command fails

echo "=== Updating package lists ==="
sudo apt update -y

echo "=== Installing make and build tools ==="
sudo apt install -y make make-guile build-essential

echo "=== Installing snap and tree utility ==="
sudo snap install tree

echo "=== Setup complete ==="
echo "You can now run: make clean && make"

