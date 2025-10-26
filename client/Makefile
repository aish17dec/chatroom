# ============================================================
#  Makefile — Client build script (C++17)
#  Part of: DC Assignment 2 – Chat Room (Ricart–Agrawala DME)
# ============================================================

# Compiler and flags
CXX       ?= g++
CXXFLAGS  ?= -O2 -std=c++17 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS   ?=

# Paths
BIN_DIR    := ../bin
COMMON_DIR := ../common

# Include search paths
INCLUDES   := -I$(COMMON_DIR)

# Object files
OBJS       := ClientMain.o \
              DME.o \
              $(COMMON_DIR)/NetUtils.o \
              $(COMMON_DIR)/Logger.o

# Output binary
TARGET     := $(BIN_DIR)/client

# Default build target
all: $(TARGET)

# Link final executable
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Built: $@"

# Compile source files
ClientMain.o: ClientMain.cpp DME.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

DME.o: DME.cpp DME.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# Clean build artefacts
clean:
	rm -f *.o $(COMMON_DIR)/*.o $(TARGET)
	@echo "Cleaned client build artefacts"

# Convenience run target
run: all
	@echo "Running client..."
	@$(TARGET) \
		--user "User1" \
		--self-id 1 --peer-id 2 \
		--peer 127.0.0.1:8002 \
		--server 127.0.0.1:7000 \
		--listen 0.0.0.0:8001

.PHONY: all clean run

