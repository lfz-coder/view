all: brpcClient brpcServer
CFLAGS = -I../../source/ -std=c++17
brpcClient: brpcClient.cc ../../source/viewRpc.cc ../../source/viewLog.cc cal.pb.cc
	g++ $(CFLAGS) $^ -o $@ -lbrpc -lleveldb -lprotobuf -lpthread -ldl -lssl -lcrypto -lspdlog -lfmt -lgflags

brpcServer: brpcServer.cc ../../source/viewRpc.cc ../../source/viewLog.cc cal.pb.cc
	g++ $(CFLAGS) $^ -o $@ -lbrpc -lleveldb -lprotobuf -lpthread -ldl -lssl -lcrypto -lspdlog -lfmt -lgflags

%.pb.cc: %.proto
	protoc --cpp_out=./ $^

clean:
	rm -f brpcClient brpcServer cal.pb.cc cal.pb.h