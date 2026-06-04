CXX = g++
CXXFLAGS = -I../../source/ -std=c++17 -Wall -Wextra
LDFLAGS = -lamqpcpp -lev -lpthread -lspdlog -lfmt -lgflags

mqTest: mqTest.cc ../../source/viewMQ.cc ../../source/viewMQ.h ../../source/viewLog.cc ../../source/viewLog.h
	$(CXX) $(CXXFLAGS) mqTest.cc ../../source/viewMQ.cc ../../source/viewLog.cc -o mqTest $(LDFLAGS)

clean:
	rm -f mqTest

test: mqTest
	./mqTest

.PHONY: clean test
