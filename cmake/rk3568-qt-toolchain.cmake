# 目标系统和架构
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 使用 Ubuntu 提供的 ARM64 C++ 交叉编译器
set(CMAKE_CXX_COMPILER
    "/usr/bin/aarch64-linux-gnu-g++"
)

# 从开发板整理的目标系统开发文件
set(CMAKE_SYSROOT
    "/home/topeet/embedded-learning/sysroots/rk3568-ubuntu20"
)

# CMake 查找目标依赖时使用的根目录
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

# 构建期间执行的工具在虚拟机环境中查找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# 库、头文件和软件包配置在目标环境中查找
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 链接阶段：
# -B：启动文件搜索路径
# -L：直接依赖库搜索路径
# -rpath-link：动态库的间接依赖搜索路径
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-B${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/ -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -L${CMAKE_SYSROOT}/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/lib/aarch64-linux-gnu"
)