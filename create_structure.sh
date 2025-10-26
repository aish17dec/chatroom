#!/bin/bash
# create_structure.sh
# This script creates an empty directory and file structure for the project.

# Exit immediately if a command fails
set -e

echo "Creating project structure..."

# Create directories
mkdir -p common server client

# Create files
touch README.md Makefile \
      common/logger.hpp common/logger.cpp common/net.hpp common/net.cpp \
      server/Makefile server/main.cpp \
      client/Makefile client/dme.hpp client/dme.cpp client/main.cpp

echo "Project structure created successfully:"
tree -a -I '.git'

