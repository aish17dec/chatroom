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
      common/Logger.hpp common/Logger.cpp common/NetUtils.hpp common/NetUtils.cpp \
      server/Makefile server/ServerMain.cpp \
      client/Makefile client/DME.hpp client/DME.cpp client/ClientMain.cpp

echo "Project structure created successfully:"
tree -a -I '.git'

