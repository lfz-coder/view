# View 项目核心注意事项

## 一、严重问题与待办事项

### 1.1 viewMQ.cc 为空文件 ⚠️ **最高优先级**

头文件 [viewMQ.h](source/viewMQ.h) 已完整声明了 `MQClient`、`PublishClient`、`SubscribeClient`、`MQFactory` 等类，但 [viewMQ.cc](source/viewMQ.cc) 是一个空文件（0 行实现代码）。

- **影响**：任何引用 `viewMQ` 的代码都无法链接
- **操作**：如需使用消息队列模块，请优先完成实现；如暂不需要，建议在头文件中添加 `#error "viewMQ module not yet implemented"` 防止误用

### 1.2 PublishClient / SubscribeClient 设计问题

两个类同时使用了**继承** `MQClient` 和**组合**（内部持有 `MQClient::Ptr`），这是一种设计冲突：

```cpp
class PublishClient : public MQClient {  // 继承
private:
    MQClient::Ptr _mqClient;             // 组合
};
```

- **影响**：语义不清晰——PublishClient 是否是一个 MQClient？还是仅使用 MQClient？
- **建议**：二选一。推荐使用组合模式（持有 `MQClient::Ptr`），去掉继承关系，因为发布/订阅客户端不应该 *是* 一个 MQClient

### 1.3 编码规范相关

已完成以下修正：
- ✅ `UUidType` → `RandomCharType`（修正拼写错误）
- ✅ 枚举值 `MIX/CHAR/DIGIT` → `kMix/kChar/kDigit`（统一 k 前缀）
- ✅ `RandomUtil::Uuid()` → `RandomUtil::RandomString()`（修正误导性命名——此函数生成随机字符串，而非 UUID）
- ✅ `UUID_LENGTH` → `RANDOM_STRING_DEFAULT_LENGTH`
- ✅ `MQClient::ptr` → `MQClient::Ptr`（统一 Ptr 大写）
- ✅ `// namespace view` → `// namespace viewRpc`（修正 viewRpc.h 中的错误注释）
- ✅ `HLS_PLAYLIST_TYPE` 添加了缺失的冒号 `:`
- ✅ `hls_base_url` 删除硬编码的局域网 IP 默认值

## 二、运行时风险

### 2.1 etcd 无限重试

`WaitForConnection()` 和 `RegisterService()` 中的重试逻辑没有最大次数限制和指数退避：

```cpp
// viewEtcd.cc
void WaitForConnection(etcd::Client& client) {
    while(!client.head().get().is_ok()) {     // ⚠️ 无限循环
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

- **风险**：如果 etcd 服务永久不可达，程序将永远阻塞
- **建议**：添加最大重试次数（如 30 次）和指数退避策略，超时后返回错误

### 2.2 RegisterService 递归重新注册

保活失败时的 `std::async` 会异步调用 `RegisterService()` 自身，形成递归链：

```cpp
// viewEtcd.cc line 97
self->RegisterService();  // ⚠️ 递归，无限制
```

- **风险**：
  - 每次重试都会创建新的 etcd 客户端和租约
  - `std::async` 返回的 future 被丢弃，析构时可能阻塞
  - 如果 etcd 持续不可用，会堆积大量异步任务
- **建议**：添加重试计数、限制最大并发异步重试数

### 2.3 Transcode() 中的手动资源管理

[viewAVTrans.cc](source/viewAVTrans.cc) 的 `HLSTransCoder::Transcode()` 函数约 250 行，手动管理多个 FFmpeg 资源，多处重复清理代码：

```cpp
// 同一个清理模式出现 5+ 次
avformat_close_input(&inputContext);
avformat_free_context(outputContext);
return false;
```

- **风险**：新增错误路径时容易遗漏清理，导致资源泄漏
- **建议**：用 RAII 包装 `AVFormatContext*`、`AVDictionary*`，使用 `std::unique_ptr` + 自定义 deleter

## 三、线程安全

### 3.1 Channels 轮询索引

`Channels::Select()` 中的 `_index` 在 `_mtx` 锁内递增，当前实现是安全的。但如果未来 `Select()` 被改为高频调用，轮询性能可能受影响。

### 3.2 ClosureFactory 的 thread_local

`HLSTransCoder::AvError()` 使用 `thread_local char[]` 作为错误缓冲区，保证线程安全。同一线程下次调用时缓冲区内容会被覆盖，调用者需注意立即使用返回值。

### 3.3 log_settings 非线程安全

`init_logger()` 不应该被多个线程同时调用。`g_logger` 的赋值不是原子的。

## 四、内存与资源管理

### 4.1 ServerFactory 中的裸 new

```cpp
// brpcServer.cc
ServerFactory::Create(9000, new CalServiceImpl())
```

- **风险**：如果 `Create` 失败返回 `nullptr`，`CalServiceImpl` 会泄漏
- **缓解**：`ServerFactory::Create` 使用 `SERVER_OWNS_SERVICE`，成功后 brpc 接管生命周期；但失败路径确实存在泄漏
- **建议**：失败时 delete service，或改用智能指针参数

### 4.2 brpcClient 中的原始指针

```cpp
auto cntl = new brpc::Controller;  // 原始 new
auto req  = new cal::AddReq;       // 原始 new
auto rsp  = new cal::AddRsp;       // 原始 new
```

- **风险**：如果 `stub.Add()` 之前抛出异常，三个对象全部泄漏
- **建议**：用 `std::make_unique` 创建，需要转移所有权时使用 `.release()`

## 五、兼容性注意事项

### 5.1 `##__VA_ARGS__` 的 GCC 扩展

[viewLog.h](source/viewLog.h) 中的日志宏使用了 GNU 扩展的 `##__VA_ARGS__`：

```cpp
#define DEBUG(fmt, ...) g_logger->debug(FMT_PREFIX + fmt, __FILE__, __LINE__, ##__VA_ARGS__)
```

- **影响**：MSVC 上可能编译失败；使用 `-pedantic` 时警告
- **建议**：C++20 项目可以改用 `__VA_OPT__(,) __VA_ARGS__`

### 5.2 FFmpeg C 库的 extern "C"

[viewAVTrans.h](source/viewAVTrans.h) 正确使用了 `extern "C"` 包裹 FFmpeg 头文件。如果新增其他 C 库依赖（如 AMQP-CPP），也需要同样处理。

## 六、构建系统

### 6.1 无统一构建系统

当前每个 example 有独立的手写 Makefile，编译参数重复定义。source 目录下的代码无独立的库构建目标。

- **建议**：引入 CMake，为 `source/` 构建静态库 `libview.a`，example 链接该库即可

### 6.2 .gitignore 不完整

当前 `.gitignore` 不能覆盖：
- 编译产物（二进制文件、`.o` 文件）散落在各 example 目录下
- protobuf 生成的 `*.pb.h` / `*.pb.cc` 文件

## 七、设计原则提醒

1. **RAII 优先**：资源获取即初始化，用对象生命周期管理资源
2. **YAGNI**：不为将来可能的需求编写代码。`viewMQ` 可以先裁剪到实际需要的最小接口
3. **DRY**：`Transcode()` 中重复的清理代码应抽取为 RAII 包装类
4. **模块独立**：每个 `viewXXX` 应该可以独立编译和测试，不相互依赖
5. **错误即日志**：失败时用 `viewLog::ERROR` 记录详细信息，方便问题定位
6. **不丢 future**：`std::async` 返回的 future 必须存储，否则析构阻塞
