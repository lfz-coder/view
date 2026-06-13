# source/CMakeLists.txt
# 定义各模块的库目标及内部依赖关系
#
# 每个模块：
#   - 独立的 STATIC 库目标（名称 = 模块名，如 viewLog）
#   - 通过 target_link_libraries 声明内部依赖（如 viewUtil → viewLog）
#   - 外部依赖通过 pkg_check_modules / find_library 查找
#
# 聚合目标：
#   - view  —— INTERFACE 库，依赖所有模块，用户链接 view 即得全部功能

# ============================================================================
# 外部依赖查找
# ============================================================================

# --- spdlog（CMake 包支持完善）---
find_package(spdlog REQUIRED)

# --- 有 .pc 文件的包 → 目标名 PkgConfig::<prefix> ---
# 注意：PkgConfig 目标名 = pkg_check_modules 的第一个参数（prefix），
#       而非 pkg-config 模块名。
view_find_pkgconf(jsoncpp   jsoncpp)
view_find_pkgconf(brpc      brpc)
view_find_pkgconf(avformat  libavformat)
view_find_pkgconf(avcodec   libavcodec)
view_find_pkgconf(avutil    libavutil)
view_find_pkgconf(swscale   libswscale)
view_find_pkgconf(redispp   redis++)
view_find_pkgconf(hiredis   hiredis)
view_find_pkgconf(odb       odb)

# --- 无 .pc 文件的包 → 目标名 third::<name> ---
view_find_library(etcdcppapi   etcd/Client.hpp           etcd-cpp-api)
view_find_library(amqpcpp      amqpcpp.h                 amqpcpp)
view_find_library(ev           ev.h                      ev)
view_find_library(fdfsclient   fastdfs/fdfs_client.h     fdfsclient)
view_find_library(fastcommon   fastcommon/logger.h       fastcommon)
view_find_library(elasticlient elasticlient/client.h     elasticlient /usr/local/include /usr/local/lib)
view_find_library(cpr          cpr/response.h            cpr          /usr/local/include /usr/local/lib)

# ============================================================================
# 辅助函数：创建模块库目标
# ============================================================================
function(view_add_module)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES HEADERS INTERNAL_DEPS EXTERNAL_DEPS)
    cmake_parse_arguments(MOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${MOD_NAME} STATIC ${MOD_SOURCES} ${MOD_HEADERS})
    add_library(view::${MOD_NAME} ALIAS ${MOD_NAME})

    target_include_directories(${MOD_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/view>
    )

    # 内部依赖：同项目其他模块
    if(MOD_INTERNAL_DEPS)
        target_link_libraries(${MOD_NAME} PUBLIC ${MOD_INTERNAL_DEPS})
    endif()

    # 外部依赖
    if(MOD_EXTERNAL_DEPS)
        target_link_libraries(${MOD_NAME} PUBLIC ${MOD_EXTERNAL_DEPS})
    endif()

    # 安装
    install(TARGETS ${MOD_NAME} EXPORT viewTargets
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib)
    install(FILES ${MOD_HEADERS} DESTINATION include/view)
endfunction()

# ============================================================================
# 模块定义
# ============================================================================

# --- viewLog：日志模块（spdlog 封装）---
view_add_module(NAME viewLog
    SOURCES  viewLog.cc
    HEADERS  viewLog.h
    EXTERNAL_DEPS spdlog::spdlog
)

# --- viewUtil：通用工具模块（JSON / 文件 / 字符串 / 随机数）---
view_add_module(NAME viewUtil
    SOURCES  viewUtil.cc
    HEADERS  viewUtil.h
    INTERNAL_DEPS viewLog
    EXTERNAL_DEPS PkgConfig::jsoncpp
)

# --- viewAVTrans：视频转码模块（FFmpeg HLS）---
view_add_module(NAME viewAVTrans
    SOURCES  viewAVTrans.cc
    HEADERS  viewAVTrans.h
    INTERNAL_DEPS viewUtil viewLog
    EXTERNAL_DEPS PkgConfig::avformat PkgConfig::avcodec
                 PkgConfig::avutil PkgConfig::swscale
)

# --- viewEtcd：服务注册 / 发现模块（etcd）---
view_add_module(NAME viewEtcd
    SOURCES  viewEtcd.cc
    HEADERS  viewEtcd.h
    INTERNAL_DEPS viewUtil viewLog
    EXTERNAL_DEPS third::etcdcppapi
)

# --- viewRpc：RPC 模块（brpc 封装）---
view_add_module(NAME viewRpc
    SOURCES  viewRpc.cc
    HEADERS  viewRpc.h
    INTERNAL_DEPS viewLog
    EXTERNAL_DEPS PkgConfig::brpc
)

# --- viewMQ：消息队列模块（AMQP-CPP + libev）---
view_add_module(NAME viewMQ
    SOURCES  viewMQ.cc
    HEADERS  viewMQ.h
    INTERNAL_DEPS viewLog
    EXTERNAL_DEPS third::amqpcpp third::ev
)

# --- viewRedis：Redis 客户端模块 ---
view_add_module(NAME viewRedis
    SOURCES  viewRedis.cc
    HEADERS  viewRedis.h
    INTERNAL_DEPS viewLog
    EXTERNAL_DEPS PkgConfig::redispp PkgConfig::hiredis
)

# --- viewODB：ODB 数据库模块 ---
view_add_module(NAME viewODB
    SOURCES  viewODB.cc
    HEADERS  viewODB.h
    INTERNAL_DEPS viewLog
    EXTERNAL_DEPS PkgConfig::odb
)

# --- viewFdfs：FastDFS 分布式文件系统客户端 ---
view_add_module(NAME viewFdfs
    SOURCES  viewFdfs.cc
    HEADERS  viewFdfs.h
    INTERNAL_DEPS viewUtil viewLog
    EXTERNAL_DEPS third::fdfsclient third::fastcommon
)

# --- viewElastic：Elasticsearch DSL / 客户端模块 ---
view_add_module(NAME viewElastic
    SOURCES  viewElastic.cc
    HEADERS  viewElastic.h
    INTERNAL_DEPS viewUtil viewLog
    EXTERNAL_DEPS third::elasticlient third::cpr
)

# ============================================================================
# 聚合目标 view —— INTERFACE 库，依赖全部模块
# ============================================================================
add_library(view INTERFACE)
add_library(view::view ALIAS view)

target_link_libraries(view INTERFACE
    viewLog viewUtil viewAVTrans viewEtcd viewRpc
    viewMQ viewRedis viewODB viewFdfs viewElastic
)

install(TARGETS view EXPORT viewTargets)
