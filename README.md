# View

C++ 后端基础设施组件库，封装常用中间件为统一风格的功能模块。

## 模块

| 模块 | 文件 | 功能 | 状态 |
|------|------|------|------|
| viewLog | [source/viewLog.h](source/viewLog.h) | spdlog 日志封装 | ✅ 完成 |
| viewUtil | [source/viewUtil.h](source/viewUtil.h) | JSON/文件/字符串/随机数工具 | ✅ 完成 |
| viewAVTrans | [source/viewAVTrans.h](source/viewAVTrans.h) | FFmpeg HLS 视频转码 | ✅ 完成 |
| viewEtcd | [source/viewEtcd.h](source/viewEtcd.h) | etcd 服务注册与发现 | ✅ 完成 |
| viewRpc | [source/viewRpc.h](source/viewRpc.h) | brpc RPC 客户端/服务端封装 | ✅ 完成 |
| viewMQ | [source/viewMQ.h](source/viewMQ.h) | AMQP/RabbitMQ 消息队列 | ⚠️ 待实现 |

## 快速开始

```cpp
#include "source/viewLog.h"
#include "source/viewUtil.h"

int main() {
    // 初始化日志
    viewLog::init_logger();
    
    // 使用 JSON 工具
    auto json = viewUtil::JsonUtil::deserialize(R"({"name": "hello"})");
    
    // 生成随机字符串
    auto id = viewUtil::RandomUtil::RandomString();
    viewLog::INFO("生成 ID: {}", id);
    return 0;
}
```

## 文档

- [编码规范](CODING_STANDARDS.md) —— 命名、注释、代码风格规范
- [项目结构](PROJECT_STRUCTURE.md) —— 模块详情和依赖关系
- [核心注意事项](CORE_NOTES.md) —— 已知问题、运行时风险、设计提醒
