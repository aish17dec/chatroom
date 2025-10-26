# ============================================================
#  Makefile — Server build script (C++17)
#  Part of: DC Assignment 2 – Chat Room (Ricart–Agrawala DME)
# ============================================================

# Compiler and flags
CXX       ?= g++
CXXFLAGS  ?= -O2 -std=c++17 -Wall -Wextra -pedantic 
LDFLAGS   ?=

# Paths
BIN_DIR   := ../bin
COMMON_DIR := ../common

# Include paths
INCLUDES  := -I$(COMMON_DIR)

# Object files
OBJS      := ServerMain.o \
             $(COMMON_DIR)/NetUtils.o 

# Target
TARGET    := $(BIN_DIR)/server

# Default rule
all: $(TARGET)

# Link target executable
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Built: $@"

# Compile source files
ServerMain.o: ServerMain.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# Cleanup rule
clean:
	rm -f *.o $(COMMON_DIR)/*.o $(TARGET)
	@echo "Cleaned server build artifacts"

# Convenience run rule (optional)
run: all
	@echo "Running server..."
	@$(TARGET) --bind 0.0.0.0:7000 --file ./chat.txt

.PHONY: all clean run

