#!/usr/bin/env cmake
# 文件名称: CompileJavaResources.cmake
# 作者: AI Assistant
# 创建日期: 2026-03-14
# 功能: 配置项目使用预编译的测试资源

# 输出目录
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/sanddirt")
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# 直接使用项目中已有的测试资源文件
set(TEST_DEX_JAR "${CMAKE_SOURCE_DIR}/tests/java/unit/TestUnitDex.jar")

if(EXISTS ${TEST_DEX_JAR})
    message(STATUS "Using existing test unit DEX resources from ${TEST_DEX_JAR}")
    
    # 复制到构建目录
    set(SANDDIRT_DEX_JAR "${OUTPUT_DIR}/sanddirt.dex.jar")
    file(COPY ${TEST_DEX_JAR} DESTINATION ${OUTPUT_DIR})
    file(RENAME ${OUTPUT_DIR}/TestUnitDex.jar ${SANDDIRT_DEX_JAR})
    
    # 暴露变量给其他 CMake 文件（我们不嵌入资源，直接告诉项目资源在哪里）
    set(SANDDIRT_RESOURCE_PATH ${SANDDIRT_DEX_JAR} CACHE INTERNAL "Path to sanddirt.dex.jar")
    
    # 对于资源嵌入，我们需要修改源代码来直接访问文件而不是嵌入式资源
    message(STATUS "Java resources configured successfully: ${SANDDIRT_DEX_JAR}")
    
    # 创建一个简单的头文件，供源代码使用
    set(RESOURCE_HEADER "${OUTPUT_DIR}/resource_paths.h")
    file(WRITE ${RESOURCE_HEADER} "#pragma once\n")
    file(APPEND ${RESOURCE_HEADER} "#include <string>\n")
    file(APPEND ${RESOURCE_HEADER} "const std::string& get_sanddirt_dex_jar_path() {\n")
    file(APPEND ${RESOURCE_HEADER} "    static std::string path = \"${SANDDIRT_DEX_JAR}\";\n")
    file(APPEND ${RESOURCE_HEADER} "    return path;\n")
    file(APPEND ${RESOURCE_HEADER} "}\n")
    
    include_directories(${OUTPUT_DIR})
    
else()
    message(FATAL_ERROR "Test DEX resources not found at ${TEST_DEX_JAR}")
endif()