CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = -lfdfsclient -lfastcommon -lspdlog -lfmt -ljsoncpp -lpthread -ldl

# 可执行文件名
TARGET = fdfsTest

# 源文件
SRCS = test.cc ../../source/viewFdfs.cc ../../source/viewLog.cc ../../source/viewUtil.cc

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -I../../source/ $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
