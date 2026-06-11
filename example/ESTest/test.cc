#include "../../source/viewElastic.h"
#include "../../source/viewUtil.h"
#include "../../source/viewLog.h"

const std::string indexName = "student";
const std::string url = "http://elastic:123456@192.168.10.129:9200/";

void Create(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Indexer indexer(indexName);
    indexer.NewTokenizer("ikmax");
    
    auto name = indexer.NewField("name");
    name->SetAnalyzer("ikmax");
    name->SetBoost(2.0);

    indexer.NewField("phone")->SetType("keyword");
    indexer.NewField("age")->SetType("integer");
    indexer.NewField("skills")->SetType("text");
    
    // 修正1: data → date
    auto birthField = indexer.NewField("birth");
    birthField->SetType("date");
    // birthField->SetIndexed(false); // 如需按日期查询，注释此行

    auto res = client->Create(indexer);
    if (res == false) {
        return;
    }
}

// 新增数据
void Insert(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Inserter inserter(indexName, "1");
    inserter.Add("name", "张三");
    inserter.Add("phone", "15566667777");
    inserter.Add("age", 18);
    // 修正2: brith → birth
    inserter.Add("birth", "2018-08-19 22:22:33");
    inserter.Append("skills", "C++");
    inserter.Append("skills", "Java");
    inserter.Append("skills", "Python");

    auto res = client->Insert(inserter);
    if (res == false) {
        return; 
    }
}

// 更新数据
void Update1(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Updater updater(indexName, "1");
    updater.Add("name", "李四");
    updater.Add("phone", "15566668888");
    updater.Add("age", 20);
    updater.Append("skills", "Go");
    updater.Append("skills", "Rust");
    auto res = client->Update(updater);
    if (res == false) {
        return;
    }
}

void Update2(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Updater updater(indexName, "1");
    updater.Add("name", "王五");
    updater.Add("phone", "15566669999");
    updater.Add("age", 18);
    // 修正2: brith → birth
    updater.Add("birth", "2018-08-19 22:22:33");
    updater.Append("skills", "C++");
    updater.Append("skills", "Java");
    updater.Append("skills", "Python");
    auto res = client->Update(updater);
    if (res == false) {
        return;
    }
}

void Select1(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Searcher searcher(indexName);
    auto query = searcher.NewQuery();
    query->NewMatch("name")->SetValue("李四");
    searcher.SetFrom(0);
    searcher.SetSize(1);
    auto res = client->Search(searcher);
    if (!res) {
        return;
    }
    viewLog::INFO("序列化结果:{}", *viewUtil::JsonUtil::serialize(*res));
}

void Select2(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Searcher searcher(indexName);
    auto query = searcher.NewQuery();
    auto should = query->NewShould();
    should->NewTerms("phone")->SetValue("15566669999");
    should->NewTerms("phone")->SetValue("15533333333");

    auto res = client->Search(searcher);
    if(!res) {
        return;
    }
    viewLog::INFO("序列化结果:{}", *viewUtil::JsonUtil::serialize(*res));
}

void Remove1(const viewElastic::ESClient::Ptr& client) {
    viewElastic::Deleter deleter(indexName, "1");
    auto res = client->Remove(deleter);
    if(res == false) {
        return;
    }
}

void Remove2(const viewElastic::ESClient::Ptr& client) {
    auto res = client->Remove(indexName);
    if(res == false) {
        return;
    }
}

int main() {
    viewLog::init_logger();
    std::vector<std::string> urls = {url};
    auto client = std::make_shared<viewElastic::ESClient>(urls);
    
    Create(client);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Insert(client);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Update1(client);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Select1(client);
    Update2(client);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Select2(client);
    Remove1(client);
    Remove2(client);

    return 0;
}