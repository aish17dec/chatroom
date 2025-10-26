CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra 
LDFLAGS := 

.PHONY: all clean pack server client

all: server client

server:
	$(MAKE) -C server CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)"

client:
	$(MAKE) -C client CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)"

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean
	rm -rf bin evidence

pack:
	@mkdir -p evidence
	@cp -v server/*.log client/*.log chat.txt evidence/ 2>/dev/null || true
	@echo "Evidence packed under ./evidence/"

