CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = -lleveldb -lprotobuf -lpthread -ldl -lcrypto -lspdlog -lfmt -lgflags \
	    -ljsoncpp \
	    -letcd-cpp-api -lcurl -lssl

all: registry discovery

registry: registry.cc ../../source/viewEtcd.cc ../../source/viewLog.cc ../../source/viewUtil.cc
	$(CXX) $(CXXFLAGS) -I../../source/ $^ -o $@ $(LDFLAGS)

discovery: discovery.cc ../../source/viewEtcd.cc ../../source/viewLog.cc ../../source/viewUtil.cc
	$(CXX) $(CXXFLAGS) -I../../source/ $^ -o $@ $(LDFLAGS)

clean:
	rm -f registry discovery

.PHONY: all clean