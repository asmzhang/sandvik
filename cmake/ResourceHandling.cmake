#!/usr/bin/env cmake
# 文件名称: ResourceHandling.cmake
# 作者: AI Assistant
# 创建日期: 2026-03-14
# 功能: 提供在 CMake 中嵌入资源文件到二进制目标的功能

# 查找 ld 命令
find_program(LD_COMMAND ld REQUIRED)

# 函数：将资源文件转换为二进制目标文件
# 参数：
#   RESOURCE_FILE - 资源文件路径
#   OUTPUT_OBJECT - 输出的目标文件路径
# 返回：无
function(embed_resource RESOURCE_FILE OUTPUT_OBJECT)
    if(NOT EXISTS ${RESOURCE_FILE})
        message(FATAL_ERROR "Resource file not found: ${RESOURCE_FILE}")
    endif()

    get_filename_component(RESOURCE_NAME ${RESOURCE_FILE} NAME)
    
    set(BINARY_SYMBOL_PREFIX "_binary_")
    set(BINARY_SYMBOL_SUFFIX "_start")
    
    # 使用 ld 命令将资源文件转换为二进制目标文件
    add_custom_command(
        OUTPUT ${OUTPUT_OBJECT}
        COMMAND ${LD_COMMAND} -r -b binary -o ${OUTPUT_OBJECT} ${RESOURCE_FILE}
        COMMENT "Embedding resource ${RESOURCE_NAME} to ${OUTPUT_OBJECT}"
        DEPENDS ${RESOURCE_FILE}
        VERBATIM
    )
    
    message(STATUS "Created resource target for ${RESOURCE_FILE} -> ${OUTPUT_OBJECT}")
endfunction()

# 函数：获取嵌入资源的符号名称
# 参数：
#   RESOURCE_FILE - 资源文件路径
# 返回：资源符号前缀（用于extern声明）
function(get_resource_symbol_prefix RESOURCE_FILE OUTPUT_PREFIX)
    get_filename_component(RESOURCE_NAME ${RESOURCE_FILE} NAME)
    
    # 将文件名转换为C标识符格式（替换非法字符为下划线）
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" SYMBOL_PREFIX "_binary_${RESOURCE_NAME}")
    
    set(${OUTPUT_PREFIX} ${SYMBOL_PREFIX} PARENT_SCOPE)
endfunction()
