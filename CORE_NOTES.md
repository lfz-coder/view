# View 项目核心注意事项

## 一、严重问题与待办事项

### 1.1 viewMQ.cc ✅ 已实现

[viewMQ.cc](source/viewMQ.cc) 已完整实现，包括：
- `MQClient`：基于 AMQP-CPP + libev 的完整客户端（声明、发布、消费）
- `PublishClient`：组合模式（已去除继承），预设交换机快捷发布
- `SubscribeClient`：组合模式，预设队列快捷消费
- `DeclareSetting`：DLX 死信队列支持

### 1.2 PublishClient / SubscribeClient 设计 ✅ 已修正

`PublishClient` 已去除对 `MQClient` 的继承，改为纯组合模式。`SubscribeClient` 本身已是组合模式，同时统一了 `ptr` → `Ptr` 命名。

### 1.3 编码规范 ✅ 已修正

所有已识别的命名和代码风格问题已修正（`RandomCharType`、`kMix/kChar/kDigit`、`RandomString()`、`Ptr`、namespace 注释、`HLS_PLAYLIST_TYPE`、`hls_base_url` 等）。

## 二、运行时风险

### 2.1 etcd 连接重试 ✅ 已修正

`WaitForConnection()` 已添加最大重试次数限制（30 次），超时后输出错误日志并返回。

### 2.2 RegisterService 递归重新注册 ✅ 已修正

保活失败时的重新注册已改为：
- 使用 `shared_ptr<int>` 共享重试计数，限制最大 5 次重试
- 移除 `std::async`，改为直接同步重试（`sleep_for + RegisterService()`），消除 future 泄漏

### 2.3 Transcode() 资源管理 ✅ 已重构

[viewAVTrans.cc](source/viewAVTrans.cc) 的 `Transcode()` 已重构为统一的 `goto cleanup` 模式：
- 所有变量在函数开头定义，消除跨变量声明跳转问题
- 所有错误路径跳转到 `cleanup:` 标签统一清理
- 使用布尔标记 `header_written` / `avio_opened` 跟踪需要清理的资源状态
- 函数从 ~250 行缩减到 ~160 行，清理代码从 5+ 处重复变为 1 处

## 三、线程安全

### 3.1 Channels 轮询索引

`Channels::Select()` 中的 `_index` 在 `_mtx` 锁内递增，当前实现是安全的。但如果未来 `Select()` 被改为高频调用，轮询性能可能受影响。

### 3.2 ClosureFactory 的 thread_local

`HLSTransCoder::AvError()` 使用 `thread_local char[]` 作为错误缓冲区，保证线程安全。同一线程下次调用时缓冲区内容会被覆盖，调用者需注意立即使用返回值。

### 3.3 log_settings 非线程安全

`init_logger()` 不应该被多个线程同时调用。`g_logger` 的赋值不是原子的。

## 四、内存与资源管理

### 4.1 ServerFactory 中的裸 new ✅ 已修正

`brpcServer.cc` 示例已改用 `std::make_shared<CalServiceImpl>()` + `service.get()` 模式，并增加了 Create 失败时的错误处理。

### 4.2 brpcClient 中的原始指针 ✅ 已修正

`brpcClient.cc` 示例已改用 `std::make_unique<>` 管理资源，先取裸指针（`.get()`）传给 brpc，再将 `unique_ptr` 移动到 lambda 中完成生命周期管理。

## 五、兼容性注意事项

### 5.1 `##__VA_ARGS__` 的 GCC 扩展 ✅ 已改进

[viewLog.h](source/viewLog.h) 中的 `FMT_PREFIX` 宏已用 `#ifndef VIEWLOG_FMT_PREFIX` 守卫保护，避免与其他库冲突。`##__VA_ARGS__` 仍然保留以供 GCC/Clang 用户使用；C++20 项目可自行改用 `__VA_OPT__(,) __VA_ARGS__`。

### 5.2 FFmpeg C 库的 extern "C"

[viewAVTrans.h](source/viewAVTrans.h) 正确使用了 `extern "C"` 包裹 FFmpeg 头文件。如果新增其他 C 库依赖，也需要同样处理。

## 六、构建系统

### 6.1 无统一构建系统

当前每个 example 有独立的手写 Makefile，编译参数重复定义。source 目录下的代码无独立的库构建目标。

- **建议**：引入 CMake，为 `source/` 构建静态库 `libview.a`，example 链接该库即可

### 6.2 .gitignore ✅ 已更新

已添加对二进制文件、`.o` 文件、`*.pb.h` / `*.pb.cc` 生成文件的忽略规则，移除了不相关的 Flash Builder 条目。

## 七、设计原则提醒

1. **RAII 优先**：资源获取即初始化，用对象生命周期管理资源
2. **YAGNI**：不为将来可能的需求编写代码。`viewMQ` 可以先裁剪到实际需要的最小接口
3. **DRY**：`Transcode()` 中重复的清理代码应抽取为 RAII 包装类
4. **模块独立**：每个 `viewXXX` 应该可以独立编译和测试，不相互依赖
5. **错误即日志**：失败时用 `viewLog::ERROR` 记录详细信息，方便问题定位
6. **不丢 future**：`std::async` 返回的 future 必须存储，否则析构阻塞
