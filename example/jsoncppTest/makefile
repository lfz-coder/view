CFLAGS = -I../../source/ -std=c++17
jsonTest : jsonTest.cc ../../source/util.cc ../../source/viewLog.cc
	g++ $(CFLAGS) $^ -o $@  -lspdlog -lfmt -lpthread -lgflags -ljsoncpp