#=============================================================================
# Sandvik - Dalvik JVM Implementation
# CMake跨平台构建系统
#
# 支持平台: Windows, Linux, macOS, Android
# 编译器: C++17标准
# 构建目标: 共享库(sandvik)和可执行文件(sandvik-vm)
#=============================================================================

cmake_minimum_required(VERSION 3.15)
project(sandvik
    VERSION 1.0.0
    DESCRIPTION "Minimal Dalvik JVM written in C++"
    LANGUAGES C CXX
)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 解决 MinGW 标准库与 Clang C++20 的兼容性问题
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(_GLIBCXX_USE_C99_MATH_TR1)
    add_compile_options(-Wno-error=consteval)
endif()

# 优先使用LLVM/Clang工具链
if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    # 强制使用LLVM/Clang工具链
    # 对于Windows，尝试查找LLVM/Clang
    if(WIN32)
        set(LLVM_PATHS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{LLVM_PATH}/bin"
        )
        find_program(CLANG_EXECUTABLE clang PATHS ${LLVM_PATHS} NO_DEFAULT_PATH)
        find_program(CLANGXX_EXECUTABLE clang++ PATHS ${LLVM_PATHS} NO_DEFAULT_PATH)
        
        # 如果未找到，尝试系统默认
        if(NOT CLANG_EXECUTABLE)
            find_program(CLANG_EXECUTABLE clang)
        endif()
        if(NOT CLANGXX_EXECUTABLE)
            find_program(CLANGXX_EXECUTABLE clang++)
        endif()
    else()
        # 对于Unix系统
        find_program(CLANG_EXECUTABLE clang)
        find_program(CLANGXX_EXECUTABLE clang++)
    endif()
    
    if(CLANG_EXECUTABLE AND CLANGXX_EXECUTABLE)
        set(CMAKE_C_COMPILER ${CLANG_EXECUTABLE} CACHE STRING "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER ${CLANGXX_EXECUTABLE} CACHE STRING "C++ compiler" FORCE)
        message(STATUS "Using LLVM/Clang toolchain: ${CLANGXX_EXECUTABLE}")
    else()
        message(WARNING "LLVM/Clang not found, using system default compiler")
    endif()
else()
    message(STATUS "Using specified compiler: C=${CMAKE_C_COMPILER}, C++=${CMAKE_CXX_COMPILER}")
endif()

# 全局编译选项
if(MSVC)
    # 统一使用 /utf-8 而不是分开的选项
    add_compile_options(/utf-8)
    add_compile_options(/wd4200 /wd4295 /wd4819 /wd4189)  # 禁用特定警告
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
    
    # 禁用警告视为错误
    string(REPLACE "/WX" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/WX" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/WX" "" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang/LLVM编译器选项
    add_compile_options(-Wall -Wextra)
    add_compile_options(-Wno-unused-parameter -Wno-unused-variable)
    add_compile_options(-Wno-missing-field-initializers)
    add_compile_options(-fcolor-diagnostics)
    
    # 统一使用UTF-8编码
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    
    # 禁用警告视为错误（如果启用）
    if(CMAKE_CXX_FLAGS MATCHES "-Werror")
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC编译器选项
    add_compile_options(-Wall -Wextra)
    add_compile_options(-Wno-unused-parameter -Wno-unused-variable)
    add_compile_options(-Wno-missing-field-initializers)
    
    # 统一使用UTF-8编码
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    
    # 禁用警告视为错误
    if(CMAKE_CXX_FLAGS MATCHES "-Werror")
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
        string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    endif()
endif()

# 查找 Java
find_package(Java REQUIRED)

# 项目选项
option(SANDVIK_BUILD_SHARED "Build shared library" ON)
option(SANDVIK_BUILD_EXECUTABLE "Build executable VM" ON)
option(SANDVIK_BUILD_TESTS "Build tests" OFF)
option(SANDVIK_BUILD_ANDROID "Build for Android" OFF)
option(SANDVIK_ENABLE_JNI "Enable JNI support" ON)
option(SANDVIK_USE_SYSTEM_DEPS "Use system dependencies" OFF)

# 编译Java资源
include(cmake/CompileJavaResources.cmake)

# 平台检测和配置
include(cmake/PlatformConfig.cmake)

# Android特定配置
if(SANDVIK_PLATFORM_ANDROID)
    include(cmake/AndroidConfig.cmake)
endif()

# 添加子目录
add_subdirectory(src)

# 如果启用测试，添加测试目录
if(SANDVIK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 安装配置
include(cmake/InstallConfig.cmake)

# 构建验证（可选）
option(SANDVIK_VERIFY_BUILD "Run build verification" OFF)
if(SANDVIK_VERIFY_BUILD)
    include(cmake/BuildVerification.cmake)
endif()