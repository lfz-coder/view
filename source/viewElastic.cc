/**
 * @file viewElastic.cc
 * @brief Elasticsearch DSL 构建器与客户端模块实现
 * @author Your Name
 * @date 2026
 *
 * 本文件实现了 Elasticsearch 查询 DSL 的类型安全构建功能。
 * 整体架构分为四层：
 *   1. DSL 基础节点层（Base → Object / Array）：提供 JSON 树形结构的增删改查
 *   2. 索引配置层（Settings / Mappings / Field / Tokenizer 等）：构建索引 settings 和 mappings
 *   3. 查询构建层（Query / QBool / Term / Match 等）：构建查询 DSL
 *   4. HTTP 客户端层（ESClient / Request）：封装 elasticlient 完成 ES 请求
 *
 * 参数传递规范：
 *   - 输入型参数：使用常量引用（const T&）
 *   - 输出型参数：使用指针（T*）
 */

#include "viewElastic.h"
#include "viewUtil.h"
#include "viewLog.h"
#include <optional>

namespace viewElastic {

// ==================== 第一层：DSL 基础节点 ====================
// Base 是所有 DSL 节点的基类，持有一个 key（节点名）和一个 JSON value（节点数据）。
// Object 是 JSON 对象节点，通过 unordered_map 管理子节点（支持按 key 查找）。
// Array  是 JSON 数组节点，通过 vector 管理子节点（有序、可重复）。
//
// 关键设计：
//   - 每个节点都有独立的 _val（自身数据）和 _subs（子节点集合）
//   - Val() / ToString() 会递归合并所有子节点的数据
//   - Add() / Append() 为模板方法，定义在头文件中，用于操作 _val

// ==================== Base —— DSL 节点基类 ====================

Base::Base(const std::string &key): _key(key) {}

const std::string& Base::Key() const { return _key; }

const Json::Value Base::Val() const { return _val; }

std::string Base::ToString() const {
    // 将 { key: val } 序列化为 JSON 字符串
    Json::Value root;
    root[_key] = _val;
    return *viewUtil::JsonUtil::serialize(root);
}

// ==================== Object —— JSON 对象节点 ====================

Object::Object(const std::string &key): Base(key) {}

std::shared_ptr<Object> Object::NewObject(const std::string &key) {
    // 先查找是否已存在同名子对象，存在则复用，不存在则创建
    Object::Ptr sub;
    sub = this->GetElement<Object>(key);
    if (sub) {
        return sub;
    }
    sub = std::make_shared<Object>(key);
    this->AddElement(sub);
    return sub;
}

std::shared_ptr<Array> Object::NewArray(const std::string &key) {
    // 先查找是否已存在同名子数组，存在则复用，不存在则创建
    Array::Ptr array;
    array = this->GetElement<Array>(key);
    if (array) {
        return array;
    }
    array = std::make_shared<Array>(key);
    this->AddElement(array);
    return array;
}

void Object::AddElement(const Base::Ptr &sub) {
    // 以子节点的 key 为键存入 map，同 key 会覆盖旧值
    _subs[sub->Key()] = sub;
}

const Json::Value Object::Val() const {
    // 递归合并：先取自身 _val，再逐个合并每个子节点的 Val()
    Json::Value root = _val;
    for (auto &i: _subs) {
        root[i.first] = i.second->Val();
    }
    return root;
}

std::string Object::ToString() const {
    // 与 Val() 逻辑一致，但最终序列化为字符串
    Json::Value root = _val;
    for (auto &i: _subs) {
        root[i.first] = i.second->Val();
    }
    return *viewUtil::JsonUtil::serialize(root);
}

// ==================== Array —— JSON 数组节点 ====================

Array::Array(const std::string &key): Base(key) {}

std::shared_ptr<Object> Array::NewObject(const std::string &key) {
    // 数组中的 Object 每次都新建，因为数组元素不可按 key 去重
    auto obj = std::make_shared<Object>(key);
    this->AddElement(obj);
    return obj;
}

std::shared_ptr<Array> Array::NewArray(const std::string &key) {
    auto arr = std::make_shared<Array>(key);
    this->AddElement(arr);
    return arr;
}

void Array::AddElement(const Base::Ptr &sub) {
    // 追加到数组末尾，子节点按照 push_back 的顺序排列
    _subs.push_back(sub);
}

const Json::Value Array::Val() const {
    // 数组节点：自身 _val 作为基础，再逐个 append 每个子节点
    Json::Value root = _val;
    for (auto &i: _subs) {
        root.append(i->Val());
    }
    return root;
}

std::string Array::ToString() const {
    Json::Value root = _val;
    for (auto &i: _subs) {
        root.append(i->Val());
    }
    return *viewUtil::JsonUtil::serialize(root);
}

// ==================== 第二层：索引配置节点 ====================
// 对应 ES 创建索引时的 settings 和 mappings 字段。
// JSON 结构示例：
// {
//   "settings": {
//     "analysis": {
//       "analyzer": {
//         "ik_max_word": { "tokenizer": "ik_max_word", "type": "custom" }
//       }
//     }
//   },
//   "mappings": {
//     "properties": {
//       "title": { "type": "text", "boost": 2.0, "index": true, "analyzer": "ik_max_word" }
//     }
//   }
// }

// ==================== Tokenizer —— 分词器 ====================

Tokenizer::Tokenizer(const std::string &str): Object(str) {
    // 默认使用 ik_max_word 分词算法
    this->SetAlgorithm("ik_max_word");
    this->SetType("custom");
}

void Tokenizer::SetAlgorithm(const std::string &t) {
    // 设置分词算法，如 "standard"、"whitespace"、"ik_smart"、"ik_max_word"
    this->Add("tokenizer", t);
}

void Tokenizer::SetType(const std::string &type) {
    this->Add("type", type);
}

// ==================== Analyzer —— 分析器 ====================

Analyzer::Analyzer(): Object("analyzer") {}

Tokenizer::Ptr Analyzer::NewTokenizer(const std::string &tokenizer_name) {
    // 获取或创建一个分词器子节点，如 NewTokenizer("ik_max_word")
    Tokenizer::Ptr tokenizer;
    tokenizer = this->GetElement<Tokenizer>(tokenizer_name);
    if (tokenizer) {
        return tokenizer;
    }
    tokenizer = std::make_shared<Tokenizer>(tokenizer_name);
    this->AddElement(tokenizer);
    return tokenizer;
}

// ==================== Analysis —— 分析配置 ====================

Analysis::Analysis(): Object("analysis") {}

Analyzer::Ptr Analysis::NewAnalyzer() {
    // 获取或创建默认分析器（key 固定为 "analyzer"）
    Analyzer::Ptr analyzer;
    analyzer = this->GetElement<Analyzer>("analyzer");
    if (analyzer) {
        return analyzer;
    }
    analyzer = std::make_shared<Analyzer>();
    this->AddElement(analyzer);
    return analyzer;
}

// ==================== Settings —— 索引设置 ====================

Settings::Settings(): Object("settings") {}

Analysis::Ptr Settings::NewAnalysis() {
    // 获取或创建 analysis 子节点
    Analysis::Ptr analysis;
    analysis = this->GetElement<Analysis>("analysis");
    if (analysis) {
        return analysis;
    }
    analysis = std::make_shared<Analysis>();
    this->AddElement(analysis);
    return analysis;
}

// ==================== Field —— 单个字段映射定义 ====================

Field::Field(const std::string &key): Object(key) {
    // 字段默认类型为 "text"
    this->SetType("text");
}

void Field::SetType(const std::string &type) {
    // 设置字段类型：text / keyword / integer / long / double / boolean / date 等
    this->Add("type", type);
}

void Field::SetBoost(double boost) {
    // 设置字段权重，影响相关性评分（仅 text 类型字段有效）
    this->Add("boost", boost);
}

void Field::SetIndexed(bool flag) {
    // 设置是否索引该字段，false 则该字段不可搜索
    this->Add("index", flag);
}

void Field::SetAnalyzer(const std::string &analizer_name) {
    // 指定该字段使用的分析器名称
    this->Add("analyzer", analizer_name);
}

// ==================== Properties —— 映射属性集合 ====================

Properties::Properties(): Object("properties") {}

Field::Ptr Properties::NewField(const std::string &field_name) {
    // 获取或创建一个字段定义，如 NewField("title")
    Field::Ptr field;
    field = this->GetElement<Field>(field_name);
    if (field) {
        return field;
    }
    field = std::make_shared<Field>(field_name);
    this->AddElement(field);
    return field;
}

// ==================== Mappings —— 索引映射 ====================

Mappings::Mappings(): Object("mapping") {
    // 默认关闭动态映射，字段必须显式定义
    this->SetDynamic(false);
}

void Mappings::SetDynamic(bool flag) {
    // 是否允许 ES 自动推断新字段的类型（生产环境建议关闭）
    this->Add("dynamic", flag);
}

Properties::Ptr Mappings::NewProperties() {
    // 获取或创建 properties 子节点
    Properties::Ptr properties;
    properties = this->GetElement<Properties>("properties");
    if (properties) {
        return properties;
    }
    properties = std::make_shared<Properties>();
    this->AddElement(properties);
    return properties;
}

// ==================== 第三层：查询叶子节点 ====================
// 这些类对应 ES 查询 DSL 中的具体查询类型。
// JSON 结构示例：
//   Term:       { "term":  { "status": "active" } }
//   Terms:      { "terms": { "tags": ["a","b"] } }
//   Match:      { "match": { "title": "hello world" } }
//   MultiMatch: { "multi_match": { "query": "hello", "fields": ["t1","t2"] } }
//   Range:      { "range":  { "price": { "gt": 100, "lt": 500 } } }
//
// 注意：Term/Terms/Match 构造时接收 field 参数，值通过头文件中的模板方法 SetValue() 设定。

// ==================== Term —— 精确匹配查询 ====================

Term::Term(const std::string &field): Object("term"), _field(field) {}
// 使用方式：term->SetValue("hello") → {"term": {"field_name": "hello"}}

// ==================== Terms —— 多值精确匹配查询 ====================

Terms::Terms(const std::string &field): Object("terms"), _field(field) {}
// 使用方式：terms->SetValue("a"); terms->SetValue("b"); → {"terms": {"field_name": ["a","b"]}}

// ==================== Match —— 全文匹配查询 ====================

Match::Match(const std::string &field): Object("match"), _field(field) {}
// 使用方式：match->SetValue("hello world") → {"match": {"field_name": "hello world"}}

// ==================== MultiMatch —— 多字段全文匹配查询 ====================

MultiMatch::MultiMatch(): Object("multi_match") {}

void MultiMatch::AppendField(const std::string &field) {
    // 向 fields 数组追加一个搜索字段
    this->Append("fields", field);
}
// 使用方式：
//   mm->AppendField("title"); mm->AppendField("content");
//   mm->SetQuery("hello") → {"multi_match": {"query": "hello", "fields": ["title","content"]}}

// ==================== Range —— 范围查询 ====================

Range::Range(const std::string &field): Object("range") {
    // _sub 是 field 名称对应的子对象，用于存放 gt/lt/gte/lte 等条件
    _sub = std::make_shared<Object>(field);
    this->AddElement(_sub);
}
// 使用方式：range->SetRange(100, 500) → {"range": {"price": {"gt": 100, "lt": 500}}}

// ==================== 第四层：查询组合节点 ====================
// 将查询叶子节点组合成完整的查询 DSL。
//
// 类继承关系：
//   QObject : Object — 单值查询容器（如 bool 查询中只能有一个 must）
//   QArray  : Array  — 数组查询容器（如 must 内部可以放多个条件）
//   Must / Should / MustNot — 对应 bool 查询的三个子句，继承 QArray
//   QBool   — bool 复合查询，包含 must / should / must_not
//   Query   — 查询根节点，包含 bool 或 match_all
//
// JSON 结构示例：
// {
//   "query": {
//     "bool": {
//       "must": [
//         { "match": { "title": "hello" } },
//         { "range": { "price": { "gt": 100 } } }
//       ],
//       "should": [
//         { "term": { "status": "hot" } }
//       ],
//       "must_not": [
//         { "term": { "deleted": true } }
//       ],
//       "minimum_should_match": 1
//     }
//   }
// }

// ==================== QObject —— 查询对象容器 ====================

QObject::QObject(const std::string &key): Object(key) {}

Term::Ptr QObject::NewTerm(const std::string &field) {
    // 在 QObject 中 term 是唯一的（按 "term" 这个固定 key 查找复用）
    Term::Ptr term;
    term = this->GetElement<Term>("term");
    if (term) {
        return term;
    }
    term = std::make_shared<Term>(field);
    this->AddElement(term);
    return term;
}

Terms::Ptr QObject::NewTerms(const std::string &field) {
    Terms::Ptr terms;
    terms = this->GetElement<Terms>("terms");
    if (terms) {
        return terms;
    }
    terms = std::make_shared<Terms>(field);
    this->AddElement(terms);
    return terms;
}

Match::Ptr QObject::NewMatch(const std::string &field) {
    Match::Ptr match;
    match = this->GetElement<Match>("match");
    if (match) {
        return match;
    }
    match = std::make_shared<Match>(field);
    this->AddElement(match);
    return match;
}

MultiMatch::Ptr QObject::NewMultiMatch() {
    MultiMatch::Ptr multi_match;
    multi_match = this->GetElement<MultiMatch>("multi_match");
    if (multi_match) {
        return multi_match;
    }
    multi_match = std::make_shared<MultiMatch>();
    this->AddElement(multi_match);
    return multi_match;
}

Range::Ptr QObject::NewRange(const std::string &field) {
    Range::Ptr range;
    range = this->GetElement<Range>("range");
    if (range) {
        return range;
    }
    range = std::make_shared<Range>(field);
    this->AddElement(range);
    return range;
}

// ==================== QArray —— 查询数组容器 ====================

QArray::QArray(const std::string &key): Array(key) {}
// QArray 中的每个查询条件被包装在一个匿名 Object("") 中再追加到数组，
// 这样数组中的每个元素就是一个独立的 JSON 对象。

Term::Ptr QArray::NewTerm(const std::string &field) {
    // 创建一个 Term，用匿名对象包裹后放入数组
    auto tmp = std::make_shared<Term>(field);
    auto obj = this->NewObject("");
    obj->AddElement(tmp);
    return tmp;
}

Terms::Ptr QArray::NewTerms(const std::string &field) {
    auto tmp = std::make_shared<Terms>(field);
    auto obj = this->NewObject("");
    obj->AddElement(tmp);
    return tmp;
}

Match::Ptr QArray::NewMatch(const std::string &field) {
    auto tmp = std::make_shared<Match>(field);
    auto obj = this->NewObject("");
    obj->AddElement(tmp);
    return tmp;
}

MultiMatch::Ptr QArray::NewMultiMatch() {
    auto tmp = std::make_shared<MultiMatch>();
    auto obj = this->NewObject("");
    obj->AddElement(tmp);
    return tmp;
}

Range::Ptr QArray::NewRange(const std::string &field) {
    auto tmp = std::make_shared<Range>(field);
    auto obj = this->NewObject("");
    obj->AddElement(tmp);
    return tmp;
}

// ==================== Must / Should / MustNot —— bool 子句 ====================

Must::Must(): QArray("must") {}
// must 子句：所有条件必须满足，参与评分
Should::Should(): QArray("should") {}
// should 子句：至少满足 minimum_should_match 个条件，参与评分
MustNot::MustNot(): QArray("must_not") {}
// must_not 子句：所有条件必须不满足，不参与评分

// ==================== QBool —— bool 复合查询 ====================

QBool::QBool(): QObject("bool") {}

void QBool::SetMinimumShouldMatch(size_t count) {
    // 设置 should 子句最少需要满足的条件数（常用于实现"或"逻辑）
    this->Add("minimum_should_match", count);
}

Must::Ptr QBool::NewMust() {
    // 获取或创建 must 子句
    Must::Ptr tmp;
    tmp = this->GetElement<Must>("must");
    if (tmp) {
        return tmp;
    }
    tmp = std::make_shared<Must>();
    this->AddElement(tmp);
    return tmp;
}

Should::Ptr QBool::NewShould() {
    // 获取或创建 should 子句
    Should::Ptr tmp;
    tmp = this->GetElement<Should>("should");
    if (tmp) {
        return tmp;
    }
    tmp = std::make_shared<Should>();
    this->AddElement(tmp);
    return tmp;
}

MustNot::Ptr QBool::NewMustNot() {
    // 获取或创建 must_not 子句
    MustNot::Ptr tmp;
    tmp = this->GetElement<MustNot>("must_not");
    if (tmp) {
        return tmp;
    }
    tmp = std::make_shared<MustNot>();
    this->AddElement(tmp);
    return tmp;
}

// ==================== Query —— 查询根节点 ====================

Query::Query(): QObject("query") {}

void Query::MatchAll() {
    // 设置 match_all 查询，匹配所有文档（空对象作为值）
    this->Add("match_all", Json::Value(Json::ValueType::objectValue));
}

QBool::Ptr Query::NewBool() {
    // 获取或创建 bool 复合查询
    QBool::Ptr tmp;
    tmp = this->GetElement<QBool>("bool");
    if (tmp) {
        return tmp;
    }
    tmp = std::make_shared<QBool>();
    this->AddElement(tmp);
    return tmp;
}

Must::Ptr Query::NewMust() {
    // 快捷方法：直接拿到 bool → must 子句
    return this->NewBool()->NewMust();
}

Should::Ptr Query::NewShould() {
    // 快捷方法：直接拿到 bool → should 子句
    return this->NewBool()->NewShould();
}

MustNot::Ptr Query::NewMustNot() {
    // 快捷方法：直接拿到 bool → must_not 子句
    return this->NewBool()->NewMustNot();
}

// ==================== 第五层：HTTP 请求层 ====================
// Request   —— 封装单次 ES 请求的参数（index / type / op / id）
// Indexer   —— 创建索引请求构建器（继承 Object + Request）
// Inserter  —— 插入文档请求构建器
// Updater   —— 更新文档请求构建器，通过 doc() 子节点存储部分更新字段
// Deleter   —— 删除文档请求构建器
// Searcher  —— 搜索请求构建器，支持 query / size / from
// ESClient  —— HTTP 客户端实现，基于 elasticlient

// ==================== Request —— 请求参数封装 ====================

Request::Request(const std::string &index, const std::string &type,
                 const std::string &op, const std::string &id)
    : _index(index), _type(type), _op(op), _id(id) {}

void Request::SetIndex(const std::string &index) { _index = index; }
void Request::SetType(const std::string &type)   { _type = type; }
void Request::SetOp(const std::string &op)       { _op = op; }
void Request::SetId(const std::string &id)       { _id = id; }

const std::string& Request::Index() const { return _index; }
const std::string& Request::Type()  const { return _type; }
const std::string& Request::Op()    const { return _op; }
const std::string& Request::Id()    const { return _id; }

// ==================== Indexer —— 创建索引请求 ====================

Indexer::Indexer(const std::string &index)
    : Object(""), Request(index, "_doc", "index", index) {}
// Indexer 继承 Object（提供 settings/mappings 的 JSON body）
// 同时继承 Request（提供 index/type/id 等 HTTP 参数）

Settings::Ptr Indexer::NewSettings() {
    // 获取或创建 settings 配置
    Settings::Ptr settings;
    settings = this->GetElement<Settings>("settings");
    if (settings) {
        return settings;
    }
    settings = std::make_shared<Settings>();
    this->AddElement(settings);
    return settings;
}

Tokenizer::Ptr Indexer::NewTokenizer(const std::string &tokenizer_name) {
    // 快捷链式调用：settings → analysis → analyzer → tokenizer
    return this->NewSettings()->NewAnalysis()->NewAnalyzer()->NewTokenizer(tokenizer_name);
}

Mappings::Ptr Indexer::NewMappings() {
    // 获取或创建 mappings 配置
    Mappings::Ptr mappings;
    mappings = this->GetElement<Mappings>("mapping");
    if (mappings) {
        return mappings;
    }
    mappings = std::make_shared<Mappings>();
    this->AddElement(mappings);
    return mappings;
}

Field::Ptr Indexer::NewField(const std::string &field_name) {
    // 快捷链式调用：mappings → properties → field
    return this->NewMappings()->NewProperties()->NewField(field_name);
}

// ==================== Inserter —— 插入文档请求 ====================

Inserter::Inserter(const std::string &index, const std::string &id)
    : Object(""), Request(index, "_doc", "_insert", id) {}
// Inserter 将用户通过 Add() 设置的字段直接作为 JSON body 发送

// ==================== Updater —— 更新文档请求 ====================

Updater::Updater(const std::string &index, const std::string &id)
    : Object(""), Request(index, "_doc", "_update", id) {}
// 更新请求体格式：{ "doc": { "field1": "new_value", ... } }
// 通过 Doc() 获取 doc 子对象，再用 Add() 设置要更新的字段

Object::Ptr Updater::Doc() {
    // 获取或创建 "doc" 子节点，用于存放部分更新字段
    Object::Ptr doc;
    doc = this->GetElement<Object>("doc");
    if (doc) {
        return doc;
    }
    doc = std::make_shared<Object>("doc");
    this->AddElement(doc);
    return doc;
}

// ==================== Deleter —— 删除文档请求 ====================

Deleter::Deleter(const std::string &index, const std::string &id)
    : Object(""), Request(index, "_doc", "_delete", id) {}
// 删除操作不需要 body，直接通过 HTTP DELETE 发送

// ==================== Searcher —— 搜索请求 ====================

Searcher::Searcher(const std::string &index)
    : Object(""), Request(index, "_doc", "_search", "") {}

Query::Ptr Searcher::NewQuery() {
    // 获取或创建 query 查询子节点
    Query::Ptr query;
    query = this->GetElement<Query>("query");
    if (query) {
        return query;
    }
    query = std::make_shared<Query>();
    this->AddElement(query);
    return query;
}

void Searcher::SetSize(size_t count) {
    // 设置返回结果数量上限（ES 默认 10）
    this->Add("size", count);
}

void Searcher::SetFrom(size_t offset) {
    // 设置分页偏移量（从第几条开始返回）
    this->Add("from", offset);
}

// ==================== ESClient —— ES HTTP 客户端 ====================
// 基于 elasticlient::Client，封装了索引和文档的 CRUD 操作。
// 所有方法遵循统一模式：
//   1. 从请求构建器中提取关键参数（index / type / id / body）
//   2. 发起 HTTP 请求
//   3. 检查响应状态码，非 2xx 则记录错误日志并返回失败

ESClient::ESClient(const std::vector<std::string> &hosts)
    : _client(std::make_shared<elasticlient::Client>(hosts)) {}

// ==================== ESClient::Create —— 创建索引 ====================

bool ESClient::Create(const Indexer &idx) {
    // 步骤1：从 Indexer 中提取关键信息
    std::string index_name = idx.Index();
    std::string index_type = idx.Type();
    std::string index_id   = idx.Id();
    std::string index_body = idx.ToString();

    // 步骤2：发起创建索引的 HTTP 请求
    auto resp = _client->index(index_name, index_type, index_id, index_body);

    // 步骤3：检查响应状态码，非 2xx 视为失败
    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("创建索引失败: {}/{}/{}/{} - {}",
                        index_name, index_type, index_id, index_body, resp.text);
        return false;
    }
    return true;
}

// ==================== ESClient::Insert —— 插入文档 ====================

bool ESClient::Insert(const Inserter &ins) {
    // 步骤1：从 Inserter 中提取关键信息
    std::string index_name = ins.Index();
    std::string index_type = ins.Type();
    std::string doc_id     = ins.Id();
    std::string index_body = ins.ToString();

    // 步骤2：发起插入文档的 HTTP 请求
    auto resp = _client->index(index_name, index_type, doc_id, index_body);

    // 步骤3：检查响应
    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("新增数据失败: {}/{}/{}/{} - {}",
                        index_name, index_type, doc_id, index_body, resp.text);
        return false;
    }
    return true;
}

// ==================== ESClient::Update —— 更新文档 ====================

bool ESClient::Update(const Updater &upd) {
    // 步骤1：提取关键信息，拼接 _update API 的 URL
    std::string index_name = upd.Index();
    std::string index_type = upd.Type();
    std::string doc_id     = upd.Id();
    std::string index_body = upd.ToString();
    std::string url = index_name + "/_update/" + doc_id;

    // 步骤2：发起 POST 请求到 /_update 端点
    auto resp = _client->performRequest(elasticlient::Client::HTTPMethod::POST, url, index_body);

    // 步骤3：检查响应
    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("更新数据失败: {}/{}/{} - {}",
                        index_name, url, index_body, resp.text);
        return false;
    }
    return true;
}

// ==================== ESClient::Remove —— 删除文档 ====================

bool ESClient::Remove(const Deleter &del) {
    std::string index_name = del.Index();
    std::string index_type = del.Type();
    std::string doc_id     = del.Id();

    auto resp = _client->remove(index_name, index_type, doc_id);

    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("删除数据失败: {}/{}/{} - {}",
                        index_name, index_type, doc_id, resp.text);
        return false;
    }
    return true;
}

// ==================== ESClient::Remove —— 删除索引 ====================

bool ESClient::Remove(const std::string &index_name) {
    // 直接对索引名发起 DELETE 请求
    auto resp = _client->performRequest(elasticlient::Client::HTTPMethod::DELETE, index_name, "");

    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("删除索引失败: {} - {}", index_name, resp.text);
        return false;
    }
    return true;
}

// ==================== ESClient::Search —— 搜索文档 ====================

std::optional<Json::Value> ESClient::Search(const Searcher &sea) {
    // 步骤1：从 Searcher 中提取关键信息
    std::string index_name = sea.Index();
    std::string index_type = sea.Type();
    std::string index_body = sea.ToString();

    // 步骤2：发起搜索请求
    auto resp = _client->search(index_name, index_type, index_body);
    if (resp.status_code < 200 || resp.status_code >= 300) {
        viewLog::ERROR("搜索数据失败: {}/{}/{} - {}",
                        index_name, index_type, index_body, resp.text);
        return std::optional<Json::Value>();
    }

    // 步骤3：反序列化响应 JSON
    auto json_resp = viewUtil::JsonUtil::deserialize(resp.text);
    if (!json_resp) {
        viewLog::ERROR("搜索数据,对响应进行反序列化失败: {}/{}/{} - {}",
                        index_name, index_type, index_body, resp.text);
        return std::optional<Json::Value>();
    }

    // 步骤4：校验 ES 响应格式是否完整（必须包含 hits.hits）
    if ((*json_resp).isNull() ||
        (*json_resp)["hits"].isNull() ||
        (*json_resp)["hits"]["hits"].isNull()) {
        viewLog::ERROR("搜索数据,响应格式错误: {}/{}/{} - {}",
                        index_name, index_type, index_body, resp.text);
        return std::optional<Json::Value>();
    }

    // 步骤5：提取每个命中文档的 _source 字段，组装结果数组
    Json::Value result;
    int sz = (*json_resp)["hits"]["hits"].size();
    for (int i = 0; i < sz; ++i) {
        result.append((*json_resp)["hits"]["hits"][i]["_source"]);
    }
    return result;
}

} // namespace viewElastic