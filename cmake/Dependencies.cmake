#=============================================================================
# 依赖管理模块
# 使用项目ext目录中的依赖
#=============================================================================

include(CMakeFindDependencyMacro)

# 依赖配置选项
option(SANDVIK_USE_SYSTEM_FMT "Use system fmt library" ${SANDVIK_USE_SYSTEM_DEPS})
option(SANDVIK_USE_SYSTEM_LIEF "Use system LIEF library" ${SANDVIK_USE_SYSTEM_DEPS})
option(SANDVIK_USE_SYSTEM_FFI "Use system libffi library" ${SANDVIK_USE_SYSTEM_DEPS})
option(SANDVIK_USE_SYSTEM_XXHASH "Use system xxhash library" ${SANDVIK_USE_SYSTEM_DEPS})

# 检查ext目录是否存在
set(EXT_DIR ${CMAKE_SOURCE_DIR}/ext)
if(NOT EXISTS ${EXT_DIR})
    message(FATAL_ERROR "ext directory not found at ${EXT_DIR}")
endif()

# 使用项目ext目录中的fmt库
if(SANDVIK_USE_SYSTEM_FMT)
    find_package(fmt REQUIRED)
    message(STATUS "Using system fmt library")
else()
    set(fmt_dir ${EXT_DIR}/fmt)
    if(EXISTS ${fmt_dir} AND EXISTS ${fmt_dir}/CMakeLists.txt)
        message(STATUS "Using ext/fmt library")
        
        # 配置fmt
        set(FMT_TEST OFF CACHE BOOL "Disable fmt tests")
        set(FMT_DOC OFF CACHE BOOL "Disable fmt documentation")
        set(FMT_INSTALL OFF CACHE BOOL "Disable fmt installation")
        
        # 直接添加fmt子目录，使用统一的UTF-8设置
        add_subdirectory(${fmt_dir} ${CMAKE_BINARY_DIR}/ext/fmt EXCLUDE_FROM_ALL)
        set(fmt_LIBRARIES fmt::fmt)
    else()
        message(FATAL_ERROR "ext/fmt not found or invalid")
    endif()
endif()

# 使用项目ext目录中的args库
set(args_dir ${EXT_DIR}/args)
if(EXISTS ${args_dir} AND EXISTS ${args_dir}/args.hxx)
    message(STATUS "Using ext/args library")
    
    add_library(args INTERFACE)
    target_include_directories(args INTERFACE ${args_dir})
    add_library(args::args ALIAS args)
else()
    message(FATAL_ERROR "ext/args not found or invalid")
endif()

# 使用项目ext目录中的LIEF库
if(SANDVIK_USE_SYSTEM_LIEF)
    find_package(LIEF REQUIRED)
    message(STATUS "Using system LIEF library")
else()
    set(lief_dir ${EXT_DIR}/LIEF)
    if(EXISTS ${lief_dir} AND EXISTS ${lief_dir}/CMakeLists.txt)
        message(STATUS "Using ext/LIEF library")
        
        # 配置LIEF - 使用内部的 spdlog，但使用外部的 fmt 库
    set(LIEF_BUILD_SHARED OFF CACHE BOOL "Build static LIEF")
    set(LIEF_EXAMPLES OFF CACHE BOOL "Disable LIEF examples")
    set(LIEF_TESTS OFF CACHE BOOL "Disable LIEF tests")
    set(LIEF_DOC OFF CACHE BOOL "Disable LIEF documentation")
    set(LIEF_PYTHON_API OFF CACHE BOOL "Disable LIEF Python API")
    set(LIEF_INSTALL OFF CACHE BOOL "Disable LIEF installation")
    set(LIEF_LOGGING ON CACHE BOOL "Enable LIEF logging")
    set(LIEF_EXTERNAL_SPDLOG OFF CACHE BOOL "Use internal spdlog library (we will set SPDLOG_FMT_EXTERNAL externally)")
    set(LIEF_EXTERNAL_EXPECTED OFF CACHE BOOL "Use external expected")
    set(LIEF_OPT_EXTERNAL_FROZEN OFF CACHE BOOL "Use external frozen")
    set(LIEF_OPT_UTFCPP_EXTERNAL OFF CACHE BOOL "Use external utf8cpp")
    set(LIEF_OPT_MBEDTLS_EXTERNAL OFF CACHE BOOL "Use external mbedtls")
    set(LIEF_OPT_NLOHMANN_JSON_EXTERNAL OFF CACHE BOOL "Use external json")
    set(LIEF_OPT_EXTERNAL_SPAN OFF CACHE BOOL "Use external span")
    
    # 为LIEF库设置特殊编译选项（禁用警告）
    set(LIEF_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    if(MSVC)
        # 禁用LIEF在Windows上的特定警告
        set(LIEF_CXX_FLAGS "${LIEF_CXX_FLAGS} /wd4334 /wd4359 /wd4251 /wd4275 /wd4244 /wd4065 /wd4819")
    else()
        # 对于其他平台，使用默认标志
    endif()
    
    # 设置 fmt 库的包含路径，让 LIEF 的 spdlog 能找到外部的 fmt 库
    set(FMT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ext/fmt/include" CACHE PATH "")
    
    # 添加 SPDLOG_FMT_EXTERNAL 定义，让 spdlog 使用外部的 fmt 库
    set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "")
    add_compile_definitions(SPDLOG_FMT_EXTERNAL)
    
    # 添加 fmt 库的包含路径
    include_directories(${FMT_INCLUDE_DIR})
    
    # 保存原始标志
    set(ORIGINAL_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    
    # 设置 C++ 标志
    set(CMAKE_CXX_FLAGS ${LIEF_CXX_FLAGS})
    
    add_subdirectory(${lief_dir} ${CMAKE_BINARY_DIR}/ext/LIEF EXCLUDE_FROM_ALL)
    
    # 恢复原始标志
    set(CMAKE_CXX_FLAGS ${ORIGINAL_CMAKE_CXX_FLAGS})
    
    set(LIEF_LIBRARIES LIEF::LIEF)
    else()
        message(FATAL_ERROR "ext/LIEF not found or invalid")
    endif()
endif()

# 使用项目ext目录中的axml-parser
set(axml_dir ${EXT_DIR}/axml-parser)
if(EXISTS ${axml_dir} AND EXISTS ${axml_dir}/CMakeLists.txt)
    message(STATUS "Using ext/axml-parser library")
    
    add_subdirectory(${axml_dir} ${CMAKE_BINARY_DIR}/ext/axml-parser EXCLUDE_FROM_ALL)
    
    # 检查axml_parser目标是否已创建
    if(TARGET axml_parser)
        set(axml_LIBRARIES axml_parser)
        add_library(axml::axml ALIAS axml_parser)
        # 确保包含目录正确
        target_include_directories(axml_parser INTERFACE ${axml_dir}/include)
    else()
        message(STATUS "Creating minimal AXML parser (fallback)")
        add_library(axml INTERFACE)
        target_include_directories(axml INTERFACE
            ${axml_dir}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/system
        )
        add_library(axml::axml ALIAS axml)
        set(axml_LIBRARIES axml)
    endif()
else()
    message(STATUS "Creating minimal AXML parser")
    
    add_library(axml INTERFACE)
    target_include_directories(axml INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/system
    )
    add_library(axml::axml ALIAS axml)
    set(axml_LIBRARIES axml)
endif()

# 使用项目ext目录中的xxhash库
if(SANDVIK_USE_SYSTEM_XXHASH)
    find_package(xxHash REQUIRED)
    message(STATUS "Using system xxhash library")
else()
    set(xxhash_dir ${EXT_DIR}/xxHash)
    if(EXISTS ${xxhash_dir} AND EXISTS ${xxhash_dir}/cmake_unofficial/CMakeLists.txt)
        message(STATUS "Using ext/xxHash library")
        
        # 配置xxhash
        set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "Disable xxhsum")
        set(XXHASH_BUILD_ENCODER OFF CACHE BOOL "Disable encoder")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static xxhash")
        
        add_subdirectory(${xxhash_dir}/cmake_unofficial ${CMAKE_BINARY_DIR}/ext/xxhash EXCLUDE_FROM_ALL)
        set(xxhash_LIBRARIES xxhash)
    else()
        message(FATAL_ERROR "ext/xxHash not found or invalid")
    endif()
endif()

# 使用项目ext目录中的libffi库（如果存在）
if(SANDVIK_USE_SYSTEM_FFI)
    find_package(LibFFI REQUIRED)
    message(STATUS "Using system libffi library")
else()
    set(libffi_dir ${EXT_DIR}/libffi)
    if(EXISTS ${libffi_dir} AND EXISTS ${libffi_dir}/CMakeLists.txt)
        message(STATUS "Using ext/libffi library")
        
        # 配置libffi
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libffi")
        set(BUILD_TESTING OFF CACHE BOOL "Disable libffi tests")
        set(FFI_BUILD_TESTS OFF CACHE BOOL "Disable libffi tests")
        
        add_subdirectory(${libffi_dir} ${CMAKE_BINARY_DIR}/ext/libffi EXCLUDE_FROM_ALL)
        
        # 创建libffi目标别名
        if(TARGET ffi)
            add_library(LibFFI::LibFFI ALIAS ffi)
            set(LibFFI_LIBRARIES ffi)
            message(STATUS "libffi library configured as 'ffi' target")
        elseif(TARGET libffi)
            add_library(LibFFI::LibFFI ALIAS libffi)
            set(LibFFI_LIBRARIES libffi)
            message(STATUS "libffi library configured as 'libffi' target")
        else()
            message(WARNING "libffi target not found, creating interface library")
            add_library(libffi INTERFACE)
            add_library(LibFFI::LibFFI ALIAS libffi)
            set(LibFFI_LIBRARIES libffi)
            
            target_compile_definitions(libffi INTERFACE
                FFI_BUILDING=1
            )
            
            if(WIN32)
                target_include_directories(libffi INTERFACE
                    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libffi_windows
                )
            endif()
        endif()
    else()
        # 如果ext目录中没有libffi，创建一个简单的接口库
        message(STATUS "Creating minimal libffi interface library")
        
        add_library(libffi INTERFACE)
        add_library(LibFFI::LibFFI ALIAS libffi)
        set(LibFFI_LIBRARIES libffi)
        
        # 添加必要的定义
        target_compile_definitions(libffi INTERFACE
            FFI_BUILDING=1
        )
        
        # 对于Windows，添加必要的头文件
        if(WIN32)
            target_include_directories(libffi INTERFACE
                ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libffi_windows
            )
        endif()
    endif()
endif()

# 依赖汇总 - LIEF 现在已禁用日志功能，我们需要单独链接外部的 fmt 库
set(SANDVIK_DEPENDENCIES
    ${fmt_LIBRARIES}
    args::args
    ${LIEF_LIBRARIES}
    ${LibFFI_LIBRARIES}
    ${xxhash_LIBRARIES}
    axml::axml
    Threads::Threads
)

# Windows 平台需要额外的网络库
if(SANDVIK_PLATFORM_WINDOWS)
    list(APPEND SANDVIK_DEPENDENCIES ws2_32)
endif()

message(STATUS "Dependencies configured successfully")