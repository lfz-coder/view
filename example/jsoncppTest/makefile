CFLAGS = -I../../source/ -std=c++17
jsonTest : jsonTest.cc ../../source/viewUtil.cc ../../source/viewLog.cc
	g++ $(CFLAGS) $^ -o $@  -lspdlog -lfmt -lpthread -lgflags -ljsoncpp