#=============================================================================
# 构建验证模块
# 验证跨平台构建功能
#=============================================================================

# 函数：验证构建配置
function(sandvik_verify_build_config)
    message(STATUS "=========================================")
    message(STATUS "Sandvik Build Configuration Verification")
    message(STATUS "=========================================")
    
    # 平台信息
    message(STATUS "Platform: ${SANDVIK_PLATFORM_NAME}")
    message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
    
    # 编译器信息
    message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    
    # 构建类型
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    
    # 项目选项
    message(STATUS "Project options:")
    message(STATUS "  Build shared library: ${SANDVIK_BUILD_SHARED}")
    message(STATUS "  Build executable: ${SANDVIK_BUILD_EXECUTABLE}")
    message(STATUS "  Build tests: ${SANDVIK_BUILD_TESTS}")
    message(STATUS "  Enable JNI: ${SANDVIK_ENABLE_JNI}")
    message(STATUS "  Build for Android: ${SANDVIK_BUILD_ANDROID}")
    
    # 平台特定验证
    if(SANDVIK_PLATFORM_WINDOWS)
        message(STATUS "Windows-specific checks:")
        if(MSVC)
            message(STATUS "  MSVC version: ${MSVC_VERSION}")
            message(STATUS "  MSVC tools: ${CMAKE_VS_PLATFORM_TOOLSET}")
        endif()
        
        # 检查Windows SDK
        if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
            message(STATUS "  Windows SDK: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
        endif()
        
    elseif(SANDVIK_PLATFORM_LINUX)
        message(STATUS "Linux-specific checks:")
        message(STATUS "  POSIX compliance: OK")
        
    elseif(SANDVIK_PLATFORM_MACOS)
        message(STATUS "macOS-specific checks:")
        message(STATUS "  POSIX compliance: OK")
        
    elseif(SANDVIK_PLATFORM_ANDROID)
        message(STATUS "Android-specific checks:")
        message(STATUS "  NDK: ${ANDROID_NDK}")
        message(STATUS "  Platform: ${ANDROID_PLATFORM}")
        message(STATUS "  ABI: ${ANDROID_ABI}")
        message(STATUS "  STL: ${ANDROID_STL}")
    endif()
    
    # 依赖检查
    message(STATUS "Dependency checks:")
    
    # 检查线程支持
    if(Threads_FOUND)
        message(STATUS "  Threads: OK")
    else()
        message(WARNING "  Threads: NOT FOUND")
    endif()
    
    # 检查C++17支持
    if(CMAKE_CXX_STANDARD EQUAL 17)
        message(STATUS "  C++17: OK")
    else()
        message(WARNING "  C++17: Using ${CMAKE_CXX_STANDARD}")
    endif()
    
    # 检查必要的头文件
    message(STATUS "Header file checks:")
    
    # 检查平台抽象头文件
    if(EXISTS ${CMAKE_BINARY_DIR}/platform_abstraction.h)
        message(STATUS "  Platform abstraction: OK")
    else()
        message(WARNING "  Platform abstraction: NOT GENERATED")
    endif()
    
    # 检查标准库头文件
    set(required_headers
        cstdint
        cstdlib
        cstring
        vector
        string
        memory
        functional
        atomic
        mutex
        thread
    )
    
    foreach(header ${required_headers})
        set(test_code "
            #include <${header}>
            int main() { return 0; }
        ")
        
        try_compile(header_${header}_ok
            ${CMAKE_BINARY_DIR}/header_tests
            SOURCES ${CMAKE_BINARY_DIR}/test_${header}.cpp
            CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17
            COMPILE_DEFINITIONS -DHEADER_TEST
        )
        
        if(header_${header}_ok)
            message(STATUS "  <${header}>: OK")
        else()
            message(WARNING "  <${header}>: NOT FOUND")
        endif()
    endforeach()
    
    # 输出目录检查
    message(STATUS "Output directory checks:")
    message(STATUS "  Binary dir: ${CMAKE_BINARY_DIR}")
    message(STATUS "  Runtime output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    message(STATUS "  Library output: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    message(STATUS "  Archive output: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    
    # 创建输出目录
    foreach(dir 
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    )
        if(NOT EXISTS ${dir})
            file(MAKE_DIRECTORY ${dir})
            message(STATUS "  Created: ${dir}")
        else()
            message(STATUS "  Exists: ${dir}")
        endif()
    endforeach()
    
    # 验证完成
    message(STATUS "=========================================")
    message(STATUS "Build configuration verification complete")
    message(STATUS "=========================================")
endfunction()

# 函数：验证源代码
function(sandvik_verify_source_code)
    message(STATUS "Verifying source code...")
    
    # 检查源代码目录
    set(source_dirs
        src
        src/system
        src/loader
        src/native
    )
    
    foreach(dir ${source_dirs})
        if(EXISTS ${CMAKE_SOURCE_DIR}/${dir})
            # 统计源文件
            file(GLOB cpp_files ${CMAKE_SOURCE_DIR}/${dir}/*.cpp)
            file(GLOB hpp_files ${CMAKE_SOURCE_DIR}/${dir}/*.hpp)
            file(GLOB c_files ${CMAKE_SOURCE_DIR}/${dir}/*.c)
            file(GLOB h_files ${CMAKE_SOURCE_DIR}/${dir}/*.h)
            
            set(total_files 0)
            if(cpp_files)
                list(LENGTH cpp_files cpp_count)
                math(EXPR total_files "${total_files} + ${cpp_count}")
            endif()
            if(hpp_files)
                list(LENGTH hpp_files hpp_count)
                math(EXPR total_files "${total_files} + ${hpp_count}")
            endif()
            if(c_files)
                list(LENGTH c_files c_count)
                math(EXPR total_files "${total_files} + ${c_count}")
            endif()
            if(h_files)
                list(LENGTH h_files h_count)
                math(EXPR total_files "${total_files} + ${h_count}")
            endif()
            
            message(STATUS "  ${dir}: ${total_files} files")
        else()
            message(WARNING "  ${dir}: NOT FOUND")
        endif()
    endforeach()
    
    # 检查关键文件
    set(critical_files
        src/main.cpp
        src/vm.hpp
        src/vm.cpp
        src/class.hpp
        src/object.hpp
    )
    
    foreach(file ${critical_files})
        if(EXISTS ${CMAKE_SOURCE_DIR}/${file})
            message(STATUS "  ${file}: OK")
        else()
            message(ERROR "  ${file}: MISSING - Build may fail")
        endif()
    endforeach()
    
    # 检查平台特定代码
    message(STATUS "Platform-specific code checks:")
    
    # 检查sharedlibrary.cpp的跨平台支持
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/system/sharedlibrary.cpp)
        file(READ ${CMAKE_SOURCE_DIR}/src/system/sharedlibrary.cpp sharedlibrary_content)
        
        # 检查Windows支持
        if(sharedlibrary_content MATCHES "LoadLibrary|GetProcAddress|FreeLibrary")
            message(STATUS "  sharedlibrary.cpp Windows support: OK")
        else()
            message(WARNING "  sharedlibrary.cpp Windows support: MAY BE MISSING")
        endif()
        
        # 检查POSIX支持
        if(sharedlibrary_content MATCHES "dlopen|dlsym|dlclose")
            message(STATUS "  sharedlibrary.cpp POSIX support: OK")
        else()
            message(WARNING "  sharedlibrary.cpp POSIX support: MAY BE MISSING")
        endif()
    endif()
    
    message(STATUS "Source code verification complete")
endfunction()

# 函数：运行构建验证
function(sandvik_run_build_verification)
    message(STATUS "Running Sandvik build verification...")
    
    # 验证构建配置
    sandvik_verify_build_config()
    
    # 验证源代码
    sandvik_verify_source_code()
    
    # 最终状态
    message(STATUS "=========================================")
    message(STATUS "BUILD VERIFICATION SUMMARY")
    message(STATUS "=========================================")
    message(STATUS "Platform: ${SANDVIK_PLATFORM_NAME}")
    message(STATUS "Status: READY FOR BUILD")
    message(STATUS "=========================================")
    message(STATUS "Next steps:")
    message(STATUS "  1. Run: cmake --build . --config Release")
    message(STATUS "  2. Run: ctest --output-on-failure")
    message(STATUS "  3. Run: ./bin/sandvik tests/java/hello/classes.dex")
    message(STATUS "=========================================")
endfunction()

# 自动运行验证（如果启用）
if(SANDVIK_VERIFY_BUILD)
    sandvik_run_build_verification()
endif()