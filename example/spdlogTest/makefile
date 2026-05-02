CFLAGS = -I../../source/ -std=c++17
logTest : logTest.cc ../../source/viewLog.cc
	g++ $(CFLAGS) $^ -o $@  -lspdlog -lfmt -lpthread -lgflags		