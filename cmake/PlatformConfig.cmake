#=============================================================================
# 平台配置模块
# 处理Windows/Linux/macOS/Android的差异
#=============================================================================

# 平台检测
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SANDVIK_PLATFORM_WINDOWS ON)
    set(SANDVIK_PLATFORM_NAME "Windows")
    message(STATUS "Building for Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SANDVIK_PLATFORM_LINUX ON)
    set(SANDVIK_PLATFORM_NAME "Linux")
    message(STATUS "Building for Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SANDVIK_PLATFORM_MACOS ON)
    set(SANDVIK_PLATFORM_NAME "macOS")
    message(STATUS "Building for macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(SANDVIK_PLATFORM_ANDROID ON)
    set(SANDVIK_PLATFORM_NAME "Android")
    message(STATUS "Building for Android")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
    set(SANDVIK_PLATFORM_NAME "${CMAKE_SYSTEM_NAME}")
endif()

# 编译器检测
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(SANDVIK_COMPILER_GCC ON)
    message(STATUS "Using GCC compiler")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(SANDVIK_COMPILER_CLANG ON)
    message(STATUS "Using Clang compiler")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(SANDVIK_COMPILER_MSVC ON)
    message(STATUS "Using MSVC compiler")
else()
    message(STATUS "Using compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# 平台特定定义
if(SANDVIK_PLATFORM_WINDOWS)
    add_definitions(-DSANDVIK_PLATFORM_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # Windows特定设置
    if(MSVC)
        add_compile_options(/W4)
        add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7+
    else()
        # MinGW/GCC/Clang on Windows
        add_compile_options(-Wall -Wextra)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fcolor-diagnostics -finput-charset=UTF-8 -fexec-charset=UTF-8)
        else()
            add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
        endif()
    endif()
else()
    # POSIX平台通用设置
    add_definitions(-DSANDVIK_PLATFORM_POSIX)
    add_compile_options(-Wall -Wextra -fPIC)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fcolor-diagnostics -finput-charset=UTF-8 -fexec-charset=UTF-8)
    else()
        add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
    
    if(SANDVIK_PLATFORM_LINUX)
        add_definitions(-DSANDVIK_PLATFORM_LINUX)
    elseif(SANDVIK_PLATFORM_MACOS)
        add_definitions(-DSANDVIK_PLATFORM_MACOS)
    elseif(SANDVIK_PLATFORM_ANDROID)
        add_definitions(-DSANDVIK_PLATFORM_ANDROID)
        add_definitions(-DANDROID)
    endif()
endif()

# 共享库后缀设置
if(SANDVIK_PLATFORM_WINDOWS)
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
    set(CMAKE_IMPORT_LIBRARY_SUFFIX ".lib")
elseif(SANDVIK_PLATFORM_MACOS)
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
else()
    # Linux/Android
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
endif()

# 线程库设置
if(SANDVIK_PLATFORM_WINDOWS)
    set(THREADS_PREFER_PTHREAD_FLAG OFF)
else()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads REQUIRED)

# Android特定配置
if(SANDVIK_PLATFORM_ANDROID)
    # Android NDK设置
    set(CMAKE_ANDROID_NDK ${ANDROID_NDK})
    set(CMAKE_ANDROID_STL_TYPE c++_static)
    set(CMAKE_ANDROID_ARM_NEON ON)
    
    # Android API级别
    if(NOT ANDROID_PLATFORM)
        set(ANDROID_PLATFORM "android-21")
    endif()
    
    add_definitions(-D__ANDROID_API__=21)
endif()

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 创建输出目录
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})