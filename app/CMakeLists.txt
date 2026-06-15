cmake_minimum_required(VERSION 3.16)
project(ai-bookreader-sim C)

# PC 仿真模式
add_definitions(-DPC_SIMULATION)

# 查找 SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# 包含路径
include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 源文件
set(SOURCES
    sim_main.c
    renderer_sdl.c
)

# 创建可执行文件
add_executable(reader_sim ${SOURCES})

# 链接 SDL2
target_link_libraries(reader_sim ${SDL2_LIBRARIES})

# 编译选项
target_compile_options(reader_sim PRIVATE -Wall -Wextra -O2)
