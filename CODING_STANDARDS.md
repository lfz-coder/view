# View 项目编码规范

## 1. 文件组织

### 1.1 目录结构
```
view/
├── source/          # 库源码（头文件 + 实现文件）
│   ├── viewXXX.h    # 模块头文件
│   └── viewXXX.cc   # 模块实现文件
├── example/         # 各模块的使用示例
│   ├── brpcTest/    # brpc RPC 示例
│   ├── etcdTest/    # etcd 服务发现示例
│   ├── FFmpegTest/  # HLS 转码示例
│   ├── jsoncppTest/ # JSON 序列化示例
│   └── spdlogTest/  # 日志示例
└── README.md
```

### 1.2 文件命名
- **头文件**：`view<Module>.h`，如 `viewAVTrans.h`、`viewRpc.h`
- **实现文件**：`view<Module>.cc`，如 `viewAVTrans.cc`、`viewRpc.cc`
- **示例文件**：小驼峰命名，如 `hlsTest.cc`、`brpcServer.cc`

### 1.3 头文件保护
统一使用 `#pragma once`，不使用传统的 `#ifndef` / `#define` 宏保护。

## 2. 命名规范

### 2.1 命名空间
- 命名空间：`view<Module>` 格式，如 `viewAVTrans`、`viewRpc`、`viewEtcd`、`viewLog`
- 命名空间与文件模块一一对应
- 命名空间关闭处必须标注名称：
  ```cpp
  } // namespace viewAVTrans     // ✓ 正确
  } // namespace view            // ✗ 错误（名称不匹配）
  ```

### 2.2 类名
- 大驼峰命名（PascalCase）`HLSTransCoder`、`MQClient`、`ServiceRegister`

### 2.3 成员变量
- 使用 `_` 前缀，小驼峰命名：`_headers`、`_serviceName`、`_evLoop`
- 示例：
  ```cpp
  std::vector<std::string> _headers;     // ✓
  std::vector<std::string> headers;      // ✗ 缺少 _ 前缀
  ```

### 2.4 类型别名
- 智能指针别名统一使用 `Ptr`（大写首字母）：
  ```cpp
  using Ptr = std::shared_ptr<MQClient>;        // ✓
  using ptr = std::shared_ptr<MQClient>;        // ✗ 小写不一致
  ```
- 函数回调类型后置：`using MessageCallback = std::function<...>`

### 2.5 枚举
- 枚举类型：大驼峰：`RandomCharType`
- 枚举值：`k` 前缀 + 大驼峰：`kMix`、`kChar`、`kDigit`
- 始终使用 `enum class`（作用域枚举），避免命名污染：
  ```cpp
  enum class RandomCharType { kMix, kChar, kDigit };   // ✓
  enum class UUidType { MIX, CHAR, DIGIT };              // ✗ 老风格
  ```

### 2.6 函数与方法
- 公开方法：大驼峰：`Parse()`、`Transcode()`、`RegisterService()`
- 私有/保护方法：大驼峰：`MakeKey()`、`WatchHandler()`、`ParseKey()`
- 静态工厂方法：大驼峰：`Create()`

### 2.7 局部变量与参数
- 小驼峰命名：`inputFile`、`outputFile`、`serviceName`
- 避免匈牙利命名法，避免无意义的缩写

### 2.8 常量
- 全局常量：全大写 + 下划线：`HLS_EXTINF`、`RANDOM_STRING_DEFAULT_LENGTH`
- 类内常量：同上

## 3. 注释规范

### 3.1 文件头注释
每个源文件必须以 Doxygen 风格的 `@file` 注释开头：

```cpp
/**
 * @file viewAVTrans.h
 * @brief HLS（HTTP Live Streaming）视频转码器模块
 * @author Your Name
 * @date 2026
 *
 * 该模块提供了将各种格式的视频文件转换为 HLS 流媒体格式的功能。
 */
```

### 3.2 类注释
所有公开类必须有 Doxygen 注释：

```cpp
/**
 * @class M3U8Info
 * @brief M3U8 播放列表文件的解析和生成类
 *
 * 该类提供了解析已有 M3U8 文件和生成新 M3U8 文件的功能。
 */
class M3U8Info {
    ...
};
```

### 3.3 方法注释
所有公开方法必须有 `@brief`、`@param`、`@return` 注释：

```cpp
/**
 * @brief 解析 M3U8 文件
 * @param fileName M3U8 文件的路径
 * @return true 解析成功，false 解析失败
 */
bool Parse(const std::string& fileName);
```

### 3.4 结构体成员注释
```cpp
struct HLSConfig {
    int hls_time = 5;                   ///< 每个 TS 分片的目标时长（秒）
    std::string hls_playlist_type = "vod"; ///< 播放列表类型（vod/event/""）
    std::string hls_base_url;           ///< TS 分片文件的 URL 基础前缀
};
```

### 3.5 注释风格选择
- 公开接口：Doxygen 风格（`/** ... */` 或 `///` 单行）
- 复杂实现内部：中文注释解释步骤（"步骤1：xxx"）
- 简单的自解释代码：不加注释

## 4. 代码风格

### 4.1 缩进与空格
- 使用 **4 个空格**缩进，不使用 Tab
- 操作符两侧加空格：`a = b + c`
- 逗号后加空格：`func(a, b, c)`

### 4.2 大括号
- K&R 风格（左大括号不换行）：
  ```cpp
  if (condition) {
      // ...
  } else {
      // ...
  }
  ```
- 所有 if/else/for/while 必须使用大括号，即使只有一行

### 4.3 行宽
- 推荐不超过 **120 字符**，参数过长时换行对齐：
  ```cpp
  static std::string RandomString(size_t length = RANDOM_STRING_DEFAULT_LENGTH,
                                   RandomCharType type = RandomCharType::kMix);
  ```

### 4.4 空行与组织
- 函数之间用空行分隔
- 逻辑相关的代码组之间使用注释分隔线：
  ```cpp
  // ==================== 步骤1：打开输入文件 ====================
  ```
- 类的 public / protected / private 按此顺序排列

## 5. 模块设计规范

### 5.1 参数传递规范

- **输入型参数**：使用常量引用（`const T&`）
  ```cpp
  static bool Read(const std::string& path, std::string* content);  // ✓ path 为输入，用 const&
  ```
- **输出型参数**：使用指针（`T*`），调用处以 `&` 取地址传入
  ```cpp
  std::string content;
  FileUtil::Read("/path/to/file", &content);  // ✓ 输出参数传指针
  ```
- **返回值**：
  - 操作类函数（读、写、注册等）：返回 `bool` 指示成功/失败
  - 获取值类函数（序列化、反序列化、上传等）：返回 `std::optional<T>`
- 输出型参数必须在函数内部进行空指针检查

### 5.2 模块单一职责
- 每个模块（`viewXXX`）只封装一个外部依赖或一个功能领域
- 例如：`viewAVTrans` 只负责 FFmpeg/HLS，`viewRpc` 只负责 brpc

### 5.3 头文件依赖最小化
- 能用前向声明的不用 `#include`
- 头文件中只包含必要的头文件，实现细节放到 `.cc` 中

### 5.4 错误处理
- 函数返回值指示成功/失败：`bool` 用于操作类，`std::optional` 用于获取值类
- 错误信息通过 `viewLog::ERROR()` 输出
- 资源清理使用 RAII 模式

### 5.5 RAII 原则
- 资源（文件、连接、锁）必须在构造函数中获取、析构函数中释放
- 示例：
  ```cpp
  struct NetworkGuard {
      NetworkGuard()  { avformat_network_init(); }
      ~NetworkGuard() { avformat_network_deinit(); }
  };
  ```

### 5.6 智能指针
- 优先使用 `std::shared_ptr` 和 `std::unique_ptr`
- 禁止裸 `new` / `delete`（除非与 C API 交互且在 RAII 包装内）

## 6. 日志规范

### 6.1 日志宏
- 统一使用项目封装的宏：`DEBUG`、`INFO`、`WARN`、`ERROR`
- 不要直接使用 `spdlog::info()` 或 `std::cout`

### 6.2 日志格式
- 使用 fmt 风格的格式化字符串：
  ```cpp
  viewLog::INFO("处理文件: {}", filename);
  viewLog::ERROR("打开文件失败: {} 错误: {}", path, errmsg);
  ```

### 6.3 日志级别
| 级别 | 用途 |
|------|------|
| DEBUG | 调试信息，发布版本中关闭 |
| INFO  | 正常运行信息（打开文件、完成步骤） |
| WARN  | 警告（可恢复的异常情况） |
| ERROR | 错误（操作失败，需要关注） |

## 7. 禁止事项

- ❌ 在头文件中定义宏名过于通用的宏（如 `FMT_PREFIX` 可能被其他库冲突）
- ❌ 在代码中使用硬编码的 IP 地址 / 端口作为默认值
- ❌ 提交编译产物（`.o`、二进制文件）到 Git
- ❌ 提交自动生成的文件（`*.pb.h`、`*.pb.cc`）——应通过构建脚本生成
- ❌ 在 `.cc` 文件为空的情况下提交（`viewMQ.cc` 为空——应实现或用 `#error` 标记未完成）
- ❌ 无限递归或无限循环（必须有最大重试次数或退出条件）
- ❌ 裸 `new` 在非 RAII 上下文中使用
