CXX = g++
CXXFLAGS = -std=c++17 -I../../source/
LIBS = -lleveldb -lprotobuf -lpthread -ldl -lssl -lcrypto -lspdlog -lfmt -lgflags \
       -lavformat -lavcodec -lavutil -lswscale \
	   -ljsoncpp

TARGET = hlsTest
SOURCES = hlsTest.cc ../../source/viewAVTrans.cc ../../source/viewLog.cc ../../source/viewUtil.cc

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(TARGET)