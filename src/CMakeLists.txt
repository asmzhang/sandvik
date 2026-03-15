#=============================================================================
# Sandvik核心源代码构建
# 构建目标: libsandvik (共享库) 和 sandvik-vm (可执行文件)
#=============================================================================

# 生成version.in.hpp文件
if(NOT EXISTS ${CMAKE_BINARY_DIR}/src/version.in.hpp)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
        OUTPUT_VARIABLE VERSION_GEN_OUTPUT
        ERROR_VARIABLE VERSION_GEN_ERROR
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    if(VERSION_GEN_ERROR)
        message(WARNING "Failed to generate version.in.hpp: ${VERSION_GEN_ERROR}")
        # 生成一个默认的版本文件
        file(WRITE ${CMAKE_BINARY_DIR}/src/version.in.hpp
            "#pragma once\n"
            "constexpr std::string_view __GitHash = \"unknown\";\n"
            "constexpr std::string_view __Version = \"1.0.0\";\n"
        )
    else()
        message(STATUS "Generated version.in.hpp successfully")
    endif()
endif()

# 添加源文件到编译路径
include_directories(${CMAKE_BINARY_DIR}/src)

# 包含依赖管理和编译选项
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Dependencies.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompileOptions.cmake)

# 收集所有源文件
file(GLOB_RECURSE SANDVIK_SOURCES
    "*.cpp"
    "*.c"
)

# 排除main.cpp（用于可执行文件）
list(FILTER SANDVIK_SOURCES EXCLUDE REGEX ".*main\.cpp$")

# 不再添加资源对象文件，我们使用直接访问方式

# 收集所有头文件
file(GLOB_RECURSE SANDVIK_HEADERS
    "*.hpp"
    "*.h"
)

# 创建共享库目标
if(SANDVIK_BUILD_SHARED)
    add_library(sandvik SHARED ${SANDVIK_SOURCES})
    
    # 库属性
    set_target_properties(sandvik PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        OUTPUT_NAME "sandvik"
    )
    
    # 包含目录 - 添加对 fmt 头文件的访问，但不链接 libfmt.a
    target_include_directories(sandvik
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/system
            ${CMAKE_CURRENT_SOURCE_DIR}/jni
            ${CMAKE_SOURCE_DIR}/ext/fmt/include
    )
    
    # 不再有Java资源编译目标依赖，我们直接使用预编译的测试资源
    
    # 链接依赖 - 从 SANDVIK_DEPENDENCIES 中移除了 fmt 库，因为 libLIEF.a 已经包含了完整的 fmt 实现
    # 如果需要单独使用 fmt，应该使用 LIEF_USE_SYSTEM_FMT 选项重新编译 LIEF
    target_link_libraries(sandvik
        PUBLIC
            ${SANDVIK_DEPENDENCIES}
    )
    
    # 配置编译和链接选项
    sandvik_configure_target(sandvik)
    
    # Windows需要导出符号
    if(SANDVIK_PLATFORM_WINDOWS)
        target_compile_definitions(sandvik
            PRIVATE
                SANDVIK_EXPORTS
        )
    endif()
    
    # 安装规则
    install(TARGETS sandvik
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
    
    # 安装头文件
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION include
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.h"
            PATTERN "*.in.hpp"
        PATTERN "system/miniz.c" EXCLUDE
        PATTERN "system/miniz.h" EXCLUDE
    )
    
    message(STATUS "Built shared library: sandvik")
endif()

# 创建可执行文件目标
if(SANDVIK_BUILD_EXECUTABLE)
    # 可执行文件源文件
    set(VM_SOURCES main.cpp)
    
    add_executable(sandvik-vm ${VM_SOURCES})
    
    # 可执行文件属性
    set_target_properties(sandvik-vm PROPERTIES
        OUTPUT_NAME "sandvik"
    )
    
    # 包含目录 - 为可执行目标添加对 fmt 头文件的访问
    target_include_directories(sandvik-vm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/ext/fmt/include
    )
    
    # 链接依赖
    target_link_libraries(sandvik-vm
        PRIVATE
            sandvik
            ${SANDVIK_DEPENDENCIES}
    )
    
    # 配置编译和链接选项
    sandvik_configure_target(sandvik-vm)
    
    # 安装规则
    install(TARGETS sandvik-vm
        RUNTIME DESTINATION bin
    )
    
    message(STATUS "Built executable: sandvik-vm")
endif()

# 创建平台抽象头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/platform_abstraction.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/platform_abstraction.h
)

# 添加到包含路径
target_include_directories(sandvik
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

if(SANDVIK_BUILD_EXECUTABLE)
    target_include_directories(sandvik-vm
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()