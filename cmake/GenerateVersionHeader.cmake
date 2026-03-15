#!/usr/bin/cmake -P
# 生成version.in.hpp文件的CMake脚本

# 尝试使用Git获取版本信息
find_package(Git QUIET)
if(GIT_FOUND)
    # 获取Git提交哈希
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # 检查是否有未提交的更改
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(GIT_STATUS)
        set(GIT_HASH "${GIT_HASH}-dirty")
    endif()
else()
    # 如果没有Git，使用默认值
    set(GIT_HASH "unknown")
endif()

# 设置版本号
set(SANDVIK_VERSION "1.0.0")

# 生成内容
set(VERSION_CONTENT "#pragma once\n")
set(VERSION_CONTENT "${VERSION_CONTENT}constexpr std::string_view __GitHash = \"${GIT_HASH}\";\n")
set(VERSION_CONTENT "${VERSION_CONTENT}constexpr std::string_view __Version = \"${SANDVIK_VERSION}\";\n")

# 写入文件
set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/src/version.in.hpp")
file(WRITE ${OUTPUT_FILE} "${VERSION_CONTENT}")

message(STATUS "Generated version.in.hpp at: ${OUTPUT_FILE}")