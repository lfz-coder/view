# 编译器和选项
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
ODB = odb

# 依赖库
LIBS = -lpthread -lgflags -lodb-mysql -lodb -lodb-boost -lspdlog -lfmt
INCLUDE = ../../source

# 目标可执行文件
TARGET = odb_test

# 添加头文件路径
CPPFLAGS = -I$(INCLUDE) -I.

# 默认目标
all: student-odb.cxx $(TARGET)

# 直接链接所有 .cc 文件（不生成 .o 文件）
$(TARGET): student-odb.cxx test.cc ../../source/viewLog.cc ../../source/viewODB.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

# 自动用 ODB 生成 ORM 代码
student-odb.cxx student-odb.hxx student-odb.ixx: student.hxx
	$(ODB) --std c++17 --database mysql --generate-query --generate-schema --profile boost/date-time student.hxx

# 清理
clean:
	rm -f $(TARGET) student.sql student-odb.cxx student-odb.hxx student-odb.ixx

# 仅清理可执行文件
clean-exec:
	rm -f $(TARGET)

# 运行
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean clean-exec run