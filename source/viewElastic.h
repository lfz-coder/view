/**
 * @file viewElastic.h
 * @brief Elasticsearch DSL 构建器与客户端模块
 * @author Your Name
 * @date 2026
 *
 * 该模块提供了类型安全的 Elasticsearch 查询 DSL 构建功能，以及基于 elasticlient
 * 的 HTTP 客户端封装。支持索引创建、文档增删改查、复合查询等操作。
 *
 * 参数传递规范：
 * - 输入型参数：使用常量引用（const T&）
 * - 输出型参数：使用指针（T*）
 *
 * @note 本模块当前为 WIP（开发中），部分模板方法仅有声明，待后续实现。
 */

#pragma once
#include "viewUtil.h"
#include <cpr/response.h>
#include <elasticlient/client.h>

#include <unordered_map>
#include <vector>

namespace viewElastic {

/**
 * @class Base
 * @brief DSL 节点的基类
 *
 * 所有 DSL 构建节点的基类，提供键值对存储和 JSON 序列化能力。
 * 每个 Base 节点包含一个 key（节点名称）和一个 JSON value（节点数据）。
 */
class Base {
public:
    using Ptr = std::shared_ptr<Base>; ///< Base 智能指针类型别名

    /**
     * @brief 构造 Base 节点
     * @param key 节点名称（作为父级 JSON 对象的键名）
     */
    Base(const std::string& key);

    /**
     * @brief 向当前节点的 JSON value 中添加键值对
     * @tparam T 值的类型（需支持 Json::Value 赋值）
     * @param key 键名
     * @param val 值
     */
    template<typename T>
    void Add(const std::string& key, const T& val) {
        _val[key] = val;
    }

    /**
     * @brief 向当前节点的 JSON value 中追加数组元素
     * @tparam T 值的类型（需支持 Json::Value 赋值）
     * @param key 数组字段名
     * @param val 待追加的值
     */
    template<typename T>
    void Append(const std::string& key, const T& val) {
        _val[key].append(val);
    }

    /**
     * @brief 获取节点名称
     * @return 节点名称的常量引用
     */
    const std::string& Key() const;

    /**
     * @brief 获取节点的 JSON value
     * @return JSON 对象
     */
    virtual const Json::Value Val() const;

    /**
     * @brief 将节点序列化为 JSON 字符串
     * @return JSON 字符串
     */
    virtual std::string ToString() const;

protected:
    std::string _key;   ///< 节点名称
    Json::Value _val;   ///< 节点数据（JSON 对象）
};

/**
 * @class Object
 * @brief DSL 对象节点
 *
 * 表示一个 JSON 对象节点，可包含多个子节点（Base::Ptr）。
 * 子节点通过 unordered_map 管理，key 为子节点名称。
 */
class Object : public Base {
public:
    using Ptr = std::shared_ptr<Object>; ///< Object 智能指针类型别名

    /**
     * @brief 构造 Object 节点
     * @param key 对象名称
     */
    Object(const std::string& key);

    /**
     * @brief 按类型获取子节点
     * @tparam R 目标子节点类型
     * @param key 子节点名称
     * @return 成功返回目标类型的 shared_ptr，未找到返回 nullptr
     */
    template<typename R>
    std::shared_ptr<R> GetElement(const std::string& key) {
        auto it = _subs.find(key);
        if (it == _subs.end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<R>(it->second);
    }

    /**
     * @brief 创建并添加一个 Object 子节点
     * @param key 子节点名称
     * @return 新创建的 Object 子节点 shared_ptr
     */
    std::shared_ptr<Object> NewObject(const std::string& key);

    /**
     * @brief 创建并添加一个 Array 子节点
     * @param key 子节点名称
     * @return 新创建的 Array 子节点 shared_ptr
     */
    std::shared_ptr<class Array> NewArray(const std::string& key);

    /**
     * @brief 添加一个已构造的子节点
     * @param sub 子节点指针
     */
    void AddElement(const Base::Ptr& sub);

    /**
     * @brief 获取当前对象的 JSON value（递归合并所有子节点）
     * @return JSON 对象
     */
    virtual const Json::Value Val() const;

    /**
     * @brief 将对象及其子节点序列化为 JSON 字符串
     * @return JSON 字符串
     */
    virtual std::string ToString() const;

private:
    std::unordered_map<std::string, Base::Ptr> _subs; ///< 子节点映射表
};

/**
 * @class Array
 * @brief DSL 数组节点
 *
 * 表示一个 JSON 数组节点，包含有序的子节点列表。
 */
class Array : public Base {
public:
    using Ptr = std::shared_ptr<Array>; ///< Array 智能指针类型别名

    /**
     * @brief 构造 Array 节点
     * @param key 数组名称
     */
    Array(const std::string& key);

    /**
     * @brief 创建并添加一个 Object 子节点到数组
     * @param key 子节点名称
     * @return 新创建的 Object 子节点 shared_ptr
     */
    std::shared_ptr<Object> NewObject(const std::string& key);

    /**
     * @brief 创建并添加一个 Array 子节点到数组
     * @param key 子节点名称
     * @return 新创建的 Array 子节点 shared_ptr
     */
    std::shared_ptr<Array> NewArray(const std::string& key);

    /**
     * @brief 向数组中追加一个已构造的子节点
     * @param sub 子节点指针
     */
    void AddElement(const Base::Ptr& sub);

    /**
     * @brief 获取数组的 JSON value（递归合并所有子节点）
     * @return JSON 数组对象
     */
    virtual const Json::Value Val() const;

    /**
     * @brief 将数组序列化为 JSON 字符串
     * @return JSON 字符串
     */
    virtual std::string ToString() const;

private:
    std::vector<Base::Ptr> _subs; ///< 子节点列表
};

/**
 * @class Tokenizer
 * @brief Elasticsearch 分词器节点
 *
 * 用于定义索引的分词器配置，如 standard、whitespace、ik_smart 等。
 */
class Tokenizer : public Object {
public:
    using Ptr = std::shared_ptr<Tokenizer>; ///< Tokenizer 智能指针类型别名

    /**
     * @brief 构造分词器节点
     * @param str 分词器名称
     */
    Tokenizer(const std::string& str);

    /**
     * @brief 设置分词算法
     * @param t 分词算法名称（如 "standard"、"whitespace"）
     */
    void SetAlgorithm(const std::string& t);

    /**
     * @brief 设置分词器类型
     * @param type 分词器类型标识
     */
    void SetType(const std::string& type);
};

/**
 * @class Analyzer
 * @brief Elasticsearch 分析器节点
 *
 * 管理索引的分析器配置，可包含一个分词器子节点。
 */
class Analyzer : public Object {
public:
    using Ptr = std::shared_ptr<Analyzer>; ///< Analyzer 智能指针类型别名

    /**
     * @brief 默认构造分析器节点
     */
    Analyzer();

    /**
     * @brief 创建并添加一个分词器子节点
     * @param tokenizerName 分词器名称（如 "standard"）
     * @return 新创建的 Tokenizer 子节点 shared_ptr
     */
    Tokenizer::Ptr NewTokenizer(const std::string& tokenizerName);
};

/**
 * @class Analysis
 * @brief Elasticsearch 分析配置节点
 *
 * 索引的 analysis 配置根节点，可包含一个分析器子节点。
 */
class Analysis : public Object {
public:
    using Ptr = std::shared_ptr<Analysis>; ///< Analysis 智能指针类型别名

    /**
     * @brief 默认构造分析配置节点
     */
    Analysis();

    /**
     * @brief 获取或创建分析器子节点
     * @return Analyzer 子节点 shared_ptr
     */
    Analyzer::Ptr NewAnalyzer();
};

/**
 * @class Settings
 * @brief Elasticsearch 索引设置节点
 *
 * 索引的 settings 配置根节点。
 */
class Settings : public Object {
public:
    using Ptr = std::shared_ptr<Settings>; ///< Settings 智能指针类型别名

    /**
     * @brief 默认构造索引设置节点
     */
    Settings();

    /**
     * @brief 获取或创建分析配置子节点
     * @return Analysis 子节点 shared_ptr
     */
    Analysis::Ptr NewAnalysis();
};

/**
 * @class Field
 * @brief Elasticsearch 字段映射节点
 *
 * 用于定义索引中单个字段的类型、权重、索引策略和分析器等映射属性。
 */
class Field : public Object {
public:
    using Ptr = std::shared_ptr<Field>; ///< Field 智能指针类型别名

    /**
     * @brief 构造字段映射节点
     * @param key 字段名称
     */
    Field(const std::string& key);

    /**
     * @brief 设置字段数据类型
     * @param type 字段类型（如 "text"、"keyword"、"integer" 等）
     */
    void SetType(const std::string& type);

    /**
     * @brief 设置字段权重（影响相关性评分）
     * @param boost 权重值
     */
    void SetBoost(double boost);

    /**
     * @brief 设置字段是否索引
     * @param flag true 索引，false 不索引
     */
    void SetIndexed(bool flag);

    /**
     * @brief 设置字段的分析器
     * @param analyzerName 分析器名称
     */
    void SetAnalyzer(const std::string& analyzerName);
};

/**
 * @class Properties
 * @brief Elasticsearch 映射属性集合节点
 *
 * 管理索引 mapping 中的所有字段定义。
 */
class Properties : public Object {
public:
    using Ptr = std::shared_ptr<Properties>; ///< Properties 智能指针类型别名

    /**
     * @brief 默认构造属性集合节点
     */
    Properties();

    /**
     * @brief 创建并添加一个字段子节点
     * @param fieldName 字段名称
     * @return 新创建的 Field 子节点 shared_ptr
     */
    Field::Ptr NewField(const std::string& fieldName);
};

/**
 * @class Mappings
 * @brief Elasticsearch 索引映射节点
 *
 * 定义索引的 mapping 结构，包含字段属性和动态映射策略。
 */
class Mappings : public Object {
public:
    using Ptr = std::shared_ptr<Mappings>; ///< Mappings 智能指针类型别名

    /**
     * @brief 默认构造映射节点
     */
    Mappings();

    /**
     * @brief 获取或创建属性集合子节点
     * @return Properties 子节点 shared_ptr
     */
    Properties::Ptr NewProperties();

    /**
     * @brief 设置是否启用动态映射
     * @param flag true 启用动态映射，false 禁用
     */
    void SetDynamic(bool flag);
};

/**
 * @class Term
 * @brief Elasticsearch term 查询节点
 *
 * 精确匹配查询，不对查询文本进行分词。
 */
class Term : public Object {
public:
    using Ptr = std::shared_ptr<Term>; ///< Term 智能指针类型别名

    /**
     * @brief 构造 term 查询节点
     * @param field 目标字段名
     */
    Term(const std::string& field);

    /**
     * @brief 设置查询值
     * @tparam T 值类型
     * @param value 精确匹配的目标值
     */
    template<typename T>
    void SetValue(const T& value) {
        this->Add(_field, value);
    }

private:
    std::string _field; ///< 目标字段名
};

/**
 * @class Terms
 * @brief Elasticsearch terms 查询节点
 *
 * 多值精确匹配查询，匹配字段值等于数组中任意一个值的文档。
 */
class Terms : public Object {
public:
    using Ptr = std::shared_ptr<Terms>; ///< Terms 智能指针类型别名

    /**
     * @brief 构造 terms 查询节点
     * @param field 目标字段名
     */
    Terms(const std::string& field);

    /**
     * @brief 设置查询值列表
     * @tparam T 值类型
     * @param value 待匹配的值（追加到值列表）
     */
    template<typename T>
    void SetValue(const T& value) {
        this->Append(_field, value);
    }

private:
    std::string _field; ///< 目标字段名
};

/**
 * @class Match
 * @brief Elasticsearch match 查询节点
 *
 * 全文匹配查询，会对查询文本进行分词后再匹配。
 */
class Match : public Object {
public:
    using Ptr = std::shared_ptr<Match>; ///< Match 智能指针类型别名

    /**
     * @brief 构造 match 查询节点
     * @param field 目标字段名
     */
    Match(const std::string& field);

    /**
     * @brief 设置查询文本
     * @tparam T 文本类型
     * @param value 查询文本
     */
    template<typename T>
    void SetValue(const T& value) {
        this->Add(_field, value);
    }

private:
    std::string _field; ///< 目标字段名
};

/**
 * @class MultiMatch
 * @brief Elasticsearch multi_match 查询节点
 *
 * 多字段全文匹配查询，在多个字段中同时搜索。
 */
class MultiMatch : public Object {
public:
    using Ptr = std::shared_ptr<MultiMatch>; ///< MultiMatch 智能指针类型别名

    /**
     * @brief 默认构造 multi_match 查询节点
     */
    MultiMatch();

    /**
     * @brief 添加一个搜索字段
     * @param field 字段名
     */
    void AppendField(const std::string& field);

    /**
     * @brief 设置查询文本
     * @tparam T 文本类型
     * @param query 查询文本
     */
    template<typename T>
    void SetQuery(const T& query) {
        this->Add("query", query);
    }
};

/**
 * @class Range
 * @brief Elasticsearch range 查询节点
 *
 * 范围查询，支持 gt/lt/gte/lte 等边界条件。
 */
class Range : public Object {
public:
    using Ptr = std::shared_ptr<Range>; ///< Range 智能指针类型别名

    /**
     * @brief 构造 range 查询节点
     * @param field 目标字段名
     */
    Range(const std::string& field);

    /**
     * @brief 设置范围查询的上下界
     * @tparam T 边界值类型
     * @param gt 大于（下界）
     * @param lt 小于（上界）
     */
    template<typename T>
    void SetRange(const T& gt, const T& lt) {
        _sub->Add("gt", gt);
        _sub->Add("lt", lt);
    }

private:
    Object::Ptr _sub; ///< 范围参数子节点
};

/**
 * @class QObject
 * @brief 查询对象节点
 *
 * 查询 DSL 的中间节点，支持创建 term、terms、match、multi_match、range 等子查询。
 */
class QObject : public Object {
public:
    using Ptr = std::shared_ptr<QObject>; ///< QObject 智能指针类型别名

    /**
     * @brief 构造查询对象节点
     * @param key 节点名称
     */
    QObject(const std::string& key);

    /**
     * @brief 创建一个 term 子查询
     * @param field 目标字段名
     * @return Term 子节点 shared_ptr
     */
    Term::Ptr NewTerm(const std::string& field);

    /**
     * @brief 创建一个 terms 子查询
     * @param field 目标字段名
     * @return Terms 子节点 shared_ptr
     */
    Terms::Ptr NewTerms(const std::string& field);

    /**
     * @brief 创建一个 match 子查询
     * @param field 目标字段名
     * @return Match 子节点 shared_ptr
     */
    Match::Ptr NewMatch(const std::string& field);

    /**
     * @brief 创建一个 multi_match 子查询
     * @return MultiMatch 子节点 shared_ptr
     */
    MultiMatch::Ptr NewMultiMatch();

    /**
     * @brief 创建一个 range 子查询
     * @param field 目标字段名
     * @return Range 子节点 shared_ptr
     */
    Range::Ptr NewRange(const std::string& field);
};

/**
 * @class QArray
 * @brief 查询数组节点
 *
 * 查询 DSL 的数组节点，支持在数组中创建各类子查询。
 */
class QArray : public Array {
public:
    using Ptr = std::shared_ptr<QArray>; ///< QArray 智能指针类型别名

    /**
     * @brief 构造查询数组节点
     * @param key 节点名称
     */
    QArray(const std::string& key);

    Term::Ptr NewTerm(const std::string& field);
    Terms::Ptr NewTerms(const std::string& field);
    Match::Ptr NewMatch(const std::string& field);
    MultiMatch::Ptr NewMultiMatch();
    Range::Ptr NewRange(const std::string& field);
};

/**
 * @class Must
 * @brief Elasticsearch bool 查询的 must 子句
 *
 * must 子句中的条件必须全部满足，参与评分计算。
 */
class Must : public QArray {
public:
    using Ptr = std::shared_ptr<Must>; ///< Must 智能指针类型别名

    /**
     * @brief 默认构造 must 子句
     */
    Must();
};

/**
 * @class Should
 * @brief Elasticsearch bool 查询的 should 子句
 *
 * should 子句中的条件至少满足一部分（由 minimum_should_match 控制），参与评分计算。
 */
class Should : public QArray {
public:
    using Ptr = std::shared_ptr<Should>; ///< Should 智能指针类型别名

    /**
     * @brief 默认构造 should 子句
     */
    Should();
};

/**
 * @class MustNot
 * @brief Elasticsearch bool 查询的 must_not 子句
 *
 * must_not 子句中的条件必须全部不满足，不参与评分计算。
 */
class MustNot : public QArray {
public:
    using Ptr = std::shared_ptr<MustNot>; ///< MustNot 智能指针类型别名

    /**
     * @brief 默认构造 must_not 子句
     */
    MustNot();
};

/**
 * @class QBool
 * @brief Elasticsearch bool 查询节点
 *
 * 复合查询节点，组合 must、should、must_not 等子句。
 */
class QBool : public QObject {
public:
    using Ptr = std::shared_ptr<QBool>; ///< QBool 智能指针类型别名

    /**
     * @brief 默认构造 bool 查询节点
     */
    QBool();

    /**
     * @brief 创建 must 子句
     * @return Must 子节点 shared_ptr
     */
    Must::Ptr NewMust();

    /**
     * @brief 创建 should 子句
     * @return Should 子节点 shared_ptr
     */
    Should::Ptr NewShould();

    /**
     * @brief 创建 must_not 子句
     * @return MustNot 子节点 shared_ptr
     */
    MustNot::Ptr NewMustNot();

    /**
     * @brief 设置 should 子句的最小满足数量
     * @param count 最小满足数
     */
    void SetMinimumShouldMatch(size_t count);
};

/**
 * @class Query
 * @brief Elasticsearch 查询根节点
 *
 * 搜索请求的 query 部分，支持 match_all、bool 复合查询等。
 */
class Query : public QObject {
public:
    using Ptr = std::shared_ptr<Query>; ///< Query 智能指针类型别名

    /**
     * @brief 默认构造查询根节点
     */
    Query();

    /**
     * @brief 设置查询为 match_all（匹配所有文档）
     */
    void MatchAll();

    /**
     * @brief 创建 bool 复合查询子节点
     * @return QBool 子节点 shared_ptr
     */
    QBool::Ptr NewBool();

    /**
     * @brief 创建 must 子句（快捷方法）
     * @return Must 子节点 shared_ptr
     */
    Must::Ptr NewMust();

    /**
     * @brief 创建 should 子句（快捷方法）
     * @return Should 子节点 shared_ptr
     */
    Should::Ptr NewShould();

    /**
     * @brief 创建 must_not 子句（快捷方法）
     * @return MustNot 子节点 shared_ptr
     */
    MustNot::Ptr NewMustNot();
};

/**
 * @class Request
 * @brief Elasticsearch HTTP 请求参数封装
 *
 * 封装一次 ES 请求的核心参数：索引名、类型、操作和文档 ID。
 */
class Request {
public:
    /**
     * @brief 构造请求对象
     * @param index 索引名称
     * @param type  文档类型
     * @param op    操作类型（如 "_search"、"_doc"）
     * @param id    文档 ID
     */
    Request(const std::string& index, const std::string& type,
            const std::string& op, const std::string& id);

    void SetIndex(const std::string& index);
    void SetType(const std::string& type);
    void SetOp(const std::string& op);
    void SetId(const std::string& id);

    const std::string& Index() const;
    const std::string& Type() const;
    const std::string& Op() const;
    const std::string& Id() const;

protected:
    std::string _index; ///< 索引名称
    std::string _type;  ///< 文档类型
    std::string _op;    ///< 操作类型
    std::string _id;    ///< 文档 ID
};

/**
 * @class Indexer
 * @brief 索引创建请求构建器
 *
 * 用于构建创建索引的完整请求体，包含 settings 和 mappings。
 */
class Indexer : public Object, public Request {
public:
    using Ptr = std::shared_ptr<Indexer>; ///< Indexer 智能指针类型别名

    /**
     * @brief 构造索引创建器
     * @param index 索引名称
     */
    Indexer(const std::string& index);

    /**
     * @brief 获取或创建设置子节点
     * @return Settings 子节点 shared_ptr
     */
    Settings::Ptr NewSettings();

    /**
     * @brief 创建分词器子节点
     * @param tokenizerName 分词器名称
     * @return Tokenizer 子节点 shared_ptr
     */
    Tokenizer::Ptr NewTokenizer(const std::string& tokenizerName);

    /**
     * @brief 获取或创建映射子节点
     * @return Mappings 子节点 shared_ptr
     */
    Mappings::Ptr NewMappings();

    /**
     * @brief 创建字段子节点
     * @param fieldName 字段名称
     * @return Field 子节点 shared_ptr
     */
    Field::Ptr NewField(const std::string& fieldName);
};

/**
 * @class Inserter
 * @brief 文档插入请求构建器
 *
 * 用于构建向指定索引插入文档的请求体。
 */
class Inserter : public Object, public Request {
public:
    using Ptr = std::shared_ptr<Inserter>; ///< Inserter 智能指针类型别名

    /**
     * @brief 构造文档插入器
     * @param index 索引名称
     * @param id    文档 ID
     */
    Inserter(const std::string& index, const std::string& id);
};

/**
 * @class Updater
 * @brief 文档更新请求构建器
 *
 * 用于构建更新指定文档的请求体，支持部分字段更新。
 */
class Updater : public Object, public Request {
public:
    using Ptr = std::shared_ptr<Updater>; ///< Updater 智能指针类型别名

    /**
     * @brief 构造文档更新器
     * @param index 索引名称
     * @param id    文档 ID
     */
    Updater(const std::string& index, const std::string& id);

    /**
     * @brief 获取或创建 doc 子节点（存储更新字段）
     * @return Object 子节点 shared_ptr
     */
    Object::Ptr Doc();

    /**
     * @brief 向 doc 子节点中添加键值对
     * @tparam T 值类型
     * @param key 字段名
     * @param val 字段值
     */
    template<typename T>
    void Add(const std::string& key, const T& val) {
        this->Doc()->Add(key, val);
    }

    /**
     * @brief 向 doc 子节点中追加数组元素
     * @tparam T 值类型
     * @param key 数组字段名
     * @param val 待追加的值
     */
    template<typename T>
    void Append(const std::string& key, const T& val) {
        this->Doc()->Append(key, val);
    }
};

/**
 * @class Deleter
 * @brief 文档删除请求构建器
 *
 * 用于构建删除指定文档的请求体。
 */
class Deleter : public Object, public Request {
public:
    using Ptr = std::shared_ptr<Deleter>; ///< Deleter 智能指针类型别名

    /**
     * @brief 构造文档删除器
     * @param index 索引名称
     * @param id    文档 ID
     */
    Deleter(const std::string& index, const std::string& id);
};

/**
 * @class Searcher
 * @brief 搜索请求构建器
 *
 * 用于构建搜索请求体，包含查询条件、分页参数等。
 */
class Searcher : public Object, public Request {
public:
    /**
     * @brief 构造搜索器
     * @param index 索引名称
     */
    Searcher(const std::string& index);

    /**
     * @brief 获取或创建查询子节点
     * @return Query 子节点 shared_ptr
     */
    Query::Ptr NewQuery();

    /**
     * @brief 设置返回结果数量
     * @param count 返回数量上限
     */
    void SetSize(size_t count);

    /**
     * @brief 设置分页偏移量
     * @param offset 起始偏移量
     */
    void SetFrom(size_t offset);
};

/**
 * @class BaseClient
 * @brief Elasticsearch 客户端抽象基类
 *
 * 定义 ES 客户端的基本操作接口，所有具体实现必须继承此类。
 */
class BaseClient {
public:
    using Ptr = std::shared_ptr<BaseClient>; ///< BaseClient 智能指针类型别名

    BaseClient() = default;
    virtual ~BaseClient() = default;

    /**
     * @brief 创建索引
     * @param idx 索引定义（包含 settings 和 mappings）
     * @return true 创建成功，false 创建失败
     */
    virtual bool Create(const Indexer& idx) = 0;

    /**
     * @brief 插入文档
     * @param ins 文档数据
     * @return true 插入成功，false 插入失败
     */
    virtual bool Insert(const Inserter& ins) = 0;

    /**
     * @brief 更新文档
     * @param upd 更新数据
     * @return true 更新成功，false 更新失败
     */
    virtual bool Update(const Updater& upd) = 0;

    /**
     * @brief 删除文档
     * @param del 删除目标
     * @return true 删除成功，false 删除失败
     */
    virtual bool Remove(const Deleter& del) = 0;

    /**
     * @brief 删除索引
     * @param index 索引名称
     * @return true 删除成功，false 删除失败
     */
    virtual bool Remove(const std::string& index) = 0;

    /**
     * @brief 执行搜索查询
     * @param sea 搜索请求体
     * @return 成功返回查询结果的 JSON 对象，失败返回 std::nullopt
     */
    virtual std::optional<Json::Value> Search(const Searcher& sea) = 0;
};

/**
 * @class ESClient
 * @brief 基于 elasticlient 的 Elasticsearch HTTP 客户端实现
 *
 * 使用 elasticlient 库通过 HTTP 协议与 ES 集群通信，支持多主机自动故障转移。
 */
class ESClient : public BaseClient {
public:
    using Ptr = std::shared_ptr<ESClient>; ///< ESClient 智能指针类型别名

    /**
     * @brief 构造 ES 客户端
     * @param hosts ES 集群节点地址列表（格式：http://host:port）
     */
    ESClient(const std::vector<std::string>& hosts);

    bool Create(const Indexer& idx) override;
    bool Insert(const Inserter& ins) override;
    bool Update(const Updater& upd) override;
    bool Remove(const Deleter& del) override;
    bool Remove(const std::string& index) override;
    std::optional<Json::Value> Search(const Searcher& sea) override;

private:
    std::shared_ptr<elasticlient::Client> _client; ///< elasticlient HTTP 客户端实例
};

} // namespace viewElastic
