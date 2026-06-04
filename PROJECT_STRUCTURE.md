# View 项目结构信息

## 项目概览

View 是一个 C++ 后端基础设施组件库，将常用的后端中间件封装为统一风格的功能模块。

**当前版本**：开发中  
**语言**：C++17  
**构建**：手写 Makefile（待迁移到 CMake）

## 模块总览

```
view/
├── source/                    # 库源码
│   ├── viewLog.h / .cc        # 日志模块（spdlog 封装）
│   ├── viewUtil.h / .cc       # 通用工具模块
│   ├── viewAVTrans.h / .cc    # 视频转码模块（FFmpeg HLS）
│   ├── viewEtcd.h / .cc       # 服务注册/发现模块（etcd）
│   ├── viewRpc.h / .cc        # RPC 模块（brpc 封装）
│   └── viewMQ.h / .cc         # 消息队列模块（AMQP/RabbitMQ）⚠️ 待实现
│
├── example/                   # 各模块的使用示例
│   ├── spdlogTest/            # 日志模块示例
│   ├── jsoncppTest/           # JSON 工具示例
│   ├── FFmpegTest/            # HLS 转码示例
│   ├── etcdTest/              # etcd 注册/发现示例
│   └── brpcTest/              # brpc RPC 客户端/服务端示例
│
├── CODING_STANDARDS.md        # 编码规范
├── PROJECT_STRUCTURE.md       # 本文件 —— 项目结构信息
├── CORE_NOTES.md              # 核心注意事项
└── README.md                  # 项目说明
```

## 模块详细说明

### 1. viewLog — 日志模块

- **文件**：[source/viewLog.h](source/viewLog.h) · [source/viewLog.cc](source/viewLog.cc)
- **依赖**：spdlog（仅外部依赖）
- **功能**：
  - 全局日志器 `g_logger` 的初始化和配置
  - 支持同步/异步模式、控制台/文件输出
  - 提供 `DEBUG` / `INFO` / `WARN` / `ERROR` 宏，自动注入文件名和行号
- **核心类型**：
  | 类型 | 说明 |
  |------|------|
  | `log_settings` | 日志配置结构体（async, level, format, path） |
  | `g_logger` | 全局 spdlog logger 实例 |
  | `init_logger()` | 日志器初始化函数 |

### 2. viewUtil — 通用工具模块

- **文件**：[source/viewUtil.h](source/viewUtil.h) · [source/viewUtil.cc](source/viewUtil.cc)
- **依赖**：jsoncpp, viewLog
- **功能**：JSON 序列化、文件读写、字符串分割、随机字符串生成
- **核心类型**：
  | 类型 | 说明 |
  |------|------|
  | `JsonUtil` | JSON 序列化/反序列化（返回 `std::optional`） |
  | `FileUtil` | 文件二进制读写 |
  | `StrUtil` | 字符串分割（按分隔符） |
  | `RandomUtil` | 随机字符串生成（支持字母+数字/纯字母/纯数字） |
  | `RandomCharType` | 随机字符类型枚举（kMix / kChar / kDigit） |

### 3. viewAVTrans — 视频转码模块

- **文件**：[source/viewAVTrans.h](source/viewAVTrans.h) · [source/viewAVTrans.cc](source/viewAVTrans.cc)
- **依赖**：FFmpeg (libavformat, libavcodec, libavutil, libswscale), viewUtil, viewLog
- **功能**：HLS 视频流转换（remux 模式，不重新编码）
- **核心类型**：
  | 类型 | 说明 |
  |------|------|
  | `M3U8Info` | M3U8 播放列表文件的解析和生成 |
  | `HLSConfig` | HLS 转码配置（分片时长、播放类型、Base URL） |
  | `HLSTransCoder` | 主转码器，支持 input → HLS 转换 |

### 4. viewEtcd — 服务注册/发现模块

- **文件**：[source/viewEtcd.h](source/viewEtcd.h) · [source/viewEtcd.cc](source/viewEtcd.cc)
- **依赖**：etcd-cpp-api, viewUtil, viewLog
- **功能**：基于 etcd 的服务注册与发现（支持心跳保活、自动重新注册）
- **核心类型**：
  | 类型 | 说明 |
  |------|------|
  | `WaitForConnection()` | 阻塞等待 etcd 连接就绪 |
  | `ServiceRegister` | 服务注册（创建租约→PUT→保活→失败重新注册） |
  | `ServiceDiscovery` | 服务发现（监听目录变化→回调通知上线/下线） |

### 5. viewRpc — RPC 模块

- **文件**：[source/viewRpc.h](source/viewRpc.h) · [source/viewRpc.cc](source/viewRpc.cc)
- **依赖**：brpc, viewLog
- **功能**：brpc 的 Channel/Server 封装，连接池管理，支持 lambda 回调
- **核心类型**：
  | 类型 | 说明 |
  |------|------|
  | `Channels` | 单服务 Channel 连接池（轮询策略） |
  | `RpcManager` | 多服务 Channel 管理器（按服务名索引） |
  | `ClosureFactory` | Closure 工厂（std::function → brpc Closure） |
  | `ServerFactory` | 服务端工厂（创建+启动+接管 Service 生命周期） |

### 6. viewMQ — 消息队列模块 ⚠️ 待实现

- **文件**：[source/viewMQ.h](source/viewMQ.h) · [source/viewMQ.cc](source/viewMQ.cc)（.cc 为空）
- **依赖**：AMQP-CPP, libev
- **功能**：RabbitMQ 客户端封装（交换机/队列声明、死信队列、发布/消费）
- **核心类型**（已声明）：
  | 类型 | 说明 |
  |------|------|
  | `DeclareSetting` | AMQP 交换机+队列声明配置 |
  | `MQClient` | 核心客户端（libev 事件循环） |
  | `PublishClient` | 发布客户端 |
  | `SubscribeClient` | 订阅客户端 |
  | `MQFactory` | 模板工厂类 |

## 模块依赖关系

```
viewAVTrans ──────┐
viewEtcd ─────────┼──→ viewUtil ──→ viewLog ──→ spdlog
viewRpc ──────────┘

viewMQ ──→ AMQP-CPP + libev（独立，待实现）
```

`viewLog` 是所有模块的基础依赖，`viewUtil` 提供了 JSON、文件、字符串等通用能力。

## 外部依赖

| 库 | 用途 | 使用模块 |
|----|------|----------|
| spdlog | 日志 | 全部模块 |
| jsoncpp | JSON 处理 | viewUtil |
| FFmpeg | 视频编解码/封装 | viewAVTrans |
| etcd-cpp-api | etcd 客户端 | viewEtcd |
| brpc | RPC 框架 | viewRpc |
| AMQP-CPP + libev | 消息队列 | viewMQ（待实现） |
| gflags | 命令行参数 | spdlogTest 示例 |

## 编译方式

以 example 目录为例，每个示例有自己的 `makefile`：
- 直接引用 `../../source/viewXXX.cc` 编译
- 链接对应的外部库
- 无统一的顶级 CMake/Makefile（待改进）
