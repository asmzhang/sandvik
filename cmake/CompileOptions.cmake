#=============================================================================
# 编译选项配置模块
# 配置跨平台的编译选项和预处理器定义（优先使用LLVM/Clang）
#=============================================================================

# 全局编译选项
function(sandvik_set_compile_options target)
    # 让 spdlog 使用外部的 fmt 库，避免重复符号定义
    target_compile_definitions(${target}
        PRIVATE
            SPDLOG_FMT_EXTERNAL
    )
    
    # 获取当前平台
    if(SANDVIK_PLATFORM_WINDOWS)
        if(MSVC)
            # MSVC编译选项
            target_compile_options(${target}
                PRIVATE
                    /W4
                    /wd4251  # 'type' : class 'std::string' needs to have dll-interface
                    /wd4275  # 'type' : base class 'std::string' needs to have dll-interface
                    /wd4244  # 'conversion' conversion from 'type' to 'type'
                    /wd4359  # 'alignment' : alignment specifier is less than actual alignment
                    /wd4334  # '<<' : result of 32-bit shift implicitly converted to 64 bits
                    /wd4819  # File contains characters that cannot be represented in current code page
                    /wd4200  # 非标准警告
                    /wd4295  # 数组太小警告
                    /wd4189  # 变量初始化但未使用
                    /utf-8
                    /permissive-  # 严格标准符合
            )
            
            # MSVC预处理器定义
            target_compile_definitions(${target}
                PRIVATE
                    _CRT_SECURE_NO_WARNINGS
                    _SCL_SECURE_NO_WARNINGS
                    _HAS_EXCEPTIONS=1
                    NOMINMAX      # 禁用min/max宏
            )
        else()
            # Windows上的GCC/Clang
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                # Clang on Windows
                target_compile_options(${target}
                    PRIVATE
                        -Wall
                        -Wextra
                        -Wno-unused-parameter
                        -Wno-unused-variable
                        -Wno-missing-field-initializers
                        -Wno-unused-function
                        -fcolor-diagnostics
                        -finput-charset=UTF-8
                        -fexec-charset=UTF-8
                        -fPIC
                )
            else()
                # GCC on Windows
                target_compile_options(${target}
                    PRIVATE
                        -Wall
                        -Wextra
                        -Wno-unused-parameter
                        -Wno-unused-variable
                        -Wno-missing-field-initializers
                        -Wno-unused-function
                        -finput-charset=UTF-8
                        -fexec-charset=UTF-8
                        -fPIC
                )
            endif()
        endif()
    else()
        # POSIX平台 (Linux/macOS/Android)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # LLVM/Clang选项
            target_compile_options(${target}
                PRIVATE
                    -Wall
                    -Wextra
                    -Wno-unused-parameter
                    -Wno-unused-variable
                    -Wno-missing-field-initializers
                    -Wno-unused-function
                    -fcolor-diagnostics
                    -finput-charset=UTF-8
                    -fexec-charset=UTF-8
                    -fPIC           # 位置无关代码
                    -fno-strict-aliasing
                    -fomit-frame-pointer
                    -Wno-gnu-zero-variadic-macro-arguments
                    -Wno-nullability-completeness
            )
        else()
            # GCC选项
            target_compile_options(${target}
                PRIVATE
                    -Wall
                    -Wextra
                    -Wno-unused-parameter
                    -Wno-unused-variable
                    -Wno-missing-field-initializers
                    -Wno-unused-function
                    -finput-charset=UTF-8
                    -fexec-charset=UTF-8
                    -fPIC           # 位置无关代码
                    -fno-strict-aliasing
                    -fomit-frame-pointer
            )
        endif()
        
        # 如果不是Android，添加架构优化
        if(NOT SANDVIK_PLATFORM_ANDROID)
            target_compile_options(${target}
                PRIVATE
                    -march=native
            )
        endif()
    endif()
    
    # 调试模式选项
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${target}
            PRIVATE
                __debug__
                SANDVIK_DEBUG=1
        )
        
        if(SANDVIK_PLATFORM_WINDOWS AND MSVC)
            target_compile_options(${target}
                PRIVATE
                    /Zi           # 调试信息
                    /Od           # 禁用优化
            )
        else()
            target_compile_options(${target}
                PRIVATE
                    -g           # 调试信息
                    -O0          # 禁用优化
            )
        endif()
    else()
        # 发布模式选项
        if(SANDVIK_PLATFORM_WINDOWS AND MSVC)
            target_compile_options(${target}
                PRIVATE
                    /O2          # 优化速度
                    /Ob2         # 内联扩展
                    /GF          # 消除重复字符串
                    /Gy          # 函数级链接
            )
        else()
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${target}
                    PRIVATE
                        -O3          # 最大优化
                        # -flto        # 链接时优化，禁用以避免C++标准库兼容性问题
                )
            else()
                target_compile_options(${target}
                    PRIVATE
                        -O3          # 最大优化
                        # -flto        # 链接时优化，禁用以避免C++标准库兼容性问题
                )
            endif()
        endif()
    endif()
    
    # 平台定义
    if(SANDVIK_PLATFORM_WINDOWS)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_PLATFORM_WINDOWS=1
        )
    elseif(SANDVIK_PLATFORM_LINUX)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_PLATFORM_LINUX=1
                SANDVIK_PLATFORM_POSIX=1
        )
    elseif(SANDVIK_PLATFORM_MACOS)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_PLATFORM_MACOS=1
                SANDVIK_PLATFORM_POSIX=1
        )
    elseif(SANDVIK_PLATFORM_ANDROID)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_PLATFORM_ANDROID=1
                SANDVIK_PLATFORM_POSIX=1
                ANDROID=1
        )
    endif()
    
    # 编译器定义
    if(SANDVIK_COMPILER_MSVC)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_COMPILER_MSVC=1
        )
    elseif(SANDVIK_COMPILER_CLANG)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_COMPILER_CLANG=1
        )
    elseif(SANDVIK_COMPILER_GCC)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_COMPILER_GCC=1
        )
    endif()
    
    # 特性定义
    if(SANDVIK_ENABLE_JNI)
        target_compile_definitions(${target}
            PRIVATE
                SANDVIK_ENABLE_JNI=1
        )
    endif()
    
    # C++标准特性
    target_compile_features(${target}
        PRIVATE
            cxx_std_20
    )
endfunction()

# 链接选项配置
function(sandvik_set_link_options target)
    if(SANDVIK_PLATFORM_WINDOWS)
        if(MSVC)
            target_link_options(${target}
                PRIVATE
                    /DEBUG       # 生成调试信息
                    /INCREMENTAL:NO  # 禁用增量链接
            )
            
            # 如果是DLL，设置导出
            if(TARGET ${target} AND ${target} STREQUAL "sandvik")
                target_link_options(${target}
                    PRIVATE
                        /DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/sandvik.def
                )
            endif()
        else()
            # Windows上的GCC/Clang
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_link_options(${target}
                    PRIVATE
                        -static-libgcc
                        -static-libstdc++
                        -Wl,--stack,0x400000
                )
            else()
                target_link_options(${target}
                    PRIVATE
                        -static-libgcc
                        -static-libstdc++
                        -Wl,--stack,0x400000
                )
            endif()
        endif()
    else()
        # POSIX平台链接选项
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_link_options(${target}
                PRIVATE
                    -Wl,-z,defs     # 报告未定义符号
                    -rdynamic       # 导出所有符号
            )
        else()
            target_link_options(${target}
                PRIVATE
                    -Wl,-z,defs     # 报告未定义符号
                    -rdynamic       # 导出所有符号
            )
        endif()
        
        # 如果不是调试模式，启用链接时优化（已禁用，避免C++标准库兼容性问题）
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            # target_link_options(${target}
            #     PRIVATE
            #         -flto
            # )
        endif()
        
        # Android特定链接选项
        if(SANDVIK_PLATFORM_ANDROID)
            target_link_options(${target}
                PRIVATE
                    -llog        # Android日志库
                    -landroid   # Android原生库
            )
        endif()
    endif()
endfunction()

# 配置目标编译和链接选项
function(sandvik_configure_target target)
    sandvik_set_compile_options(${target})
    sandvik_set_link_options(${target})
    
    # 设置C++标准
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    message(STATUS "Configured target: ${target}")
endfunction()