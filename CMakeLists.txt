# view — C++ 后端基础设施组件库
# 将常用中间件封装为统一风格的功能模块
cmake_minimum_required(VERSION 3.16)
project(view
    VERSION  0.1.0
    LANGUAGES CXX
)

# ============================================================================
# 全局设置
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认构建静态库；-DBUILD_SHARED_LIBS=ON 切换为动态库
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

# ============================================================================
# CMake 模块路径
# ============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(viewMacros)

# ============================================================================
# 源码
# ============================================================================
add_subdirectory(source)

# ============================================================================
# 安装规则
# ============================================================================
include(GNUInstallDirs)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/
        DESTINATION include/view
        FILES_MATCHING
        PATTERN "*.h"
)

# 导出目标
install(EXPORT viewTargets
        NAMESPACE view::
        DESTINATION lib/cmake/view
        FILE viewTargets.cmake
)

# 生成包配置文件
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/viewConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/viewConfig.cmake"
    INSTALL_DESTINATION lib/cmake/view
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/viewConfig.cmake"
        DESTINATION lib/cmake/view
)
