// 防止头文件重复包含
#pragma once

#include <string>                                  // 标准字符串类型
#include <boost/date_time/posix_time/posix_time.hpp>  // Boost库中的日期时间类型
#include <odb/nullable.hxx>                       // ODB库中的可空字段支持
#include <odb/core.hxx>                           // ODB核心功能，包含ORM宏定义

/**
 * @brief 学生表实体类
 * 
 * 对应数据库中的tbl_student表，ORM映射需要
 * - 默认构造函数（必须）
 * - friend class odb::access（允许ODB访问私有成员）
 * - #pragma db标记映射信息
 */
#pragma db object table("tbl_student")
class Student {
public:
    // 默认构造函数（ODB要求必须有）
    Student() = default;
    
    // 带参数的构造函数，用于快速创建学生对象
    Student(const std::string& name) : _name(name) {}
    
    // ========== 成员变量的getter/setter方法 ==========
    size_t sn() const { return _sn; }
    void setSn(size_t sn) { _sn = sn; }
    
    size_t classId() const { return _classId; }
    void setClassId(size_t classId) { _classId = classId; }
    
    const std::string& name() const { return _name; }
    void setName(const std::string& name) { _name = name; }
    
    // 可空字段的返回值是 odb::nullable<T> 类型
    const odb::nullable<int>& age() const { return _age; }
    void setAge(const odb::nullable<int>& age) { _age = age; }
    
    const odb::nullable<double>& score() const { return _score; }
    void setScore(const odb::nullable<double>& score) { _score = score; }
    
    const odb::nullable<boost::posix_time::ptime>& birthday() const { return _birthday; }
    void setBirthday(const odb::nullable<boost::posix_time::ptime>& birthday) { _birthday = birthday; }

private:
    // 允许ODB访问私有成员（必须）
    friend class odb::access;
    
    // #pragma db 开头的都是ODB的映射指令
    
    #pragma db id auto      // 主键，auto表示自动增长
    size_t _sn;             // 学生学号（主键）
    
    #pragma db index        // 为classId字段创建索引，提高查询效率
    size_t _classId;        // 班级ID（外键关联tbl_classes表）
    
    std::string _name;      // 学生姓名（普通字段）
    
    odb::nullable<int> _age;                    // 年龄（可为空）
    odb::nullable<double> _score;               // 分数（可为空）
    odb::nullable<boost::posix_time::ptime> _birthday;  // 生日（可为空）
};

/**
 * @brief 班级表实体类
 * 
 * 对应数据库中的tbl_classes表
 */
#pragma db object table("tbl_classes")
class Classes {
public:
    Classes() = default;
    Classes(const std::string& name) : _name(name) {}
    
    // getter/setter方法
    size_t id() const { return _id; }
    void setId(size_t id) { _id = id; }
    
    const std::string& name() const { return _name; }
    void setName(const std::string& name) { _name = name; }
    
private:
    friend class odb::access;
    
    #pragma db id auto
    size_t _id;              // 班级ID（主键，自增）
    
    #pragma db unique type("VARCHAR(32)")  // unique表示唯一约束，type指定数据库字段类型
    std::string _name;       // 班级名称（唯一，最大32字符）
};

/**
 * @brief 视图：班级及其所有学生
 * 
 * 执行左连接查询，返回班级和学生的一对多关系
 * 过滤条件示例：Classes::name == "一年级一班"
 * 
 * 注意：query((?)) 中的 ? 是占位符，在使用时会被实际查询条件替换
 */
#pragma db view object(Classes) \
                object(Student : Student::_classId == Classes::_id) \ 
                query((?))
struct ClassStudent {
    // 使用智能指针自动管理对象生命周期
    std::shared_ptr<Classes> classes;   // 班级信息
    std::shared_ptr<Student> student;   // 学生信息
};

/**
 * @brief 视图：学生及其所属班级
 * 
 * 执行左连接查询，返回学生和班级的多对一关系
 * 过滤条件示例：Student::name == "zhangsan"
 */
#pragma db view object(Student) \
                object(Classes : Student::_classId == Classes::_id) \
                query((?))
struct StudentClass {
    std::shared_ptr<Student> student;   // 学生信息
    std::shared_ptr<Classes> classes;   // 班级信息
};

/* ========================= 使用说明 ========================= */

/**
 * 基础命令：生成ORM代码和数据库schema
 * odb -d mysql --generate-query --generate-schema your_file.hxx
 * 
 * 带boost日期时间支持的完整命令（推荐）：
 * odb -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time your_file.hxx
 * 
 * 参数说明：
 * -d mysql          : 指定使用MySQL数据库
 * --std c++11       : 使用C++11标准
 * --generate-query  : 生成查询支持代码
 * --generate-schema : 生成数据库表创建语句
 * --profile boost/date-time : 启用Boost日期时间支持
 * 
 * 使用流程：
 * 1. 运行odb命令生成对应的.cpp文件和.sql文件
 * 2. 将生成的.cpp文件加入到项目中编译
 * 3. 执行生成的.sql文件创建数据库表
 * 4. 在代码中包含生成的.hxx文件（ODB会根据你的.hxx生成同名的-odb.hxx）
 */
