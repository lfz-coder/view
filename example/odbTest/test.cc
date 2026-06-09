#include "../../source/viewODB.h"
#include "../../source/viewLog.h"
#include <iostream>
#include "student.hxx"
#include "student-odb.hxx"

int main() {
    viewLog::init_logger();
    viewODB::MysqlSettings settings = {
        .host = "192.168.10.129",
        .user = "root",
        .password = "123456",
        .database = "mytest",
        .connectPoolSize = 5
    };
    auto mysql = viewODB::DatabaseFactory::Mysql(settings);
    {
        try {
            odb::transaction tx(mysql->begin());
            auto& db = tx.database();
            typedef odb::query<ClassStudent> Query;
            typedef odb::result<ClassStudent> Result;
            Result res(db.query<ClassStudent>(Query::Classes::name == "一年级一班"));
            for (auto it = res.begin(); it != res.end(); ++it) {
                if (it->classes) {
                    viewLog::INFO("班级:{}\t", it->classes->name());
                }
                if (it->student) {
                    viewLog::INFO("姓名:{}\t", it->student->name());
                }
            }
            tx.commit();
        } catch (odb::exception& e) {
            viewLog::ERROR("ODB Error:{}", e.what());
        }
    }
}