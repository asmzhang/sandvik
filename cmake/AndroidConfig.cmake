#=============================================================================
# Android配置模块
# 配置Android NDK构建和JNI支持
#=============================================================================

if(SANDVIK_PLATFORM_ANDROID)
    message(STATUS "Configuring Android build")
    
    # 设置Android NDK路径
    if(NOT ANDROID_NDK)
        message(FATAL_ERROR "ANDROID_NDK is not set. Please set ANDROID_NDK to the path of Android NDK.")
    endif()
    
    # 设置Android工具链
    set(CMAKE_TOOLCHAIN_FILE ${ANDROID_NDK}/build/cmake/android.toolchain.cmake)
    
    # Android平台设置
    if(NOT ANDROID_PLATFORM)
        set(ANDROID_PLATFORM "android-21")
    endif()
    
    # Android ABI设置
    if(NOT ANDROID_ABI)
        set(ANDROID_ABI "arm64-v8a")
    endif()
    
    # Android STL设置
    if(NOT ANDROID_STL)
        set(ANDROID_STL "c++_static")
    endif()
    
    # 设置Android编译选项
    add_compile_definitions(
        __ANDROID_API__=21
        ANDROID
    )
    
    # Android特定链接库
    set(ANDROID_LIBRARIES
        log     # Android日志库
        android # Android原生库
    )
    
    # JNI支持配置
    if(SANDVIK_ENABLE_JNI)
        message(STATUS "Enabling JNI support for Android")
        
        # 查找JNI头文件
        find_package(JNI REQUIRED)
        
        if(JNI_FOUND)
            message(STATUS "JNI found:")
            message(STATUS "  JNI_INCLUDE_DIRS: ${JNI_INCLUDE_DIRS}")
            message(STATUS "  JNI_LIBRARIES: ${JNI_LIBRARIES}")
            
            # 添加JNI包含目录
            include_directories(${JNI_INCLUDE_DIRS})
            
            # 添加JNI定义
            add_compile_definitions(
                SANDVIK_JNI_ENABLED=1
            )
        else()
            message(WARNING "JNI not found, JNI support will be disabled")
            set(SANDVIK_ENABLE_JNI OFF)
        endif()
    endif()
    
    # 创建Android特定的目标
    function(sandvik_add_android_library target)
        add_library(${target} SHARED ${ARGN})
        
        # Android特定设置
        set_target_properties(${target} PROPERTIES
            PREFIX ""
            SUFFIX ".so"
        )
        
        # 链接Android库
        target_link_libraries(${target}
            PRIVATE
                ${ANDROID_LIBRARIES}
        )
        
        # 设置包含目录
        target_include_directories(${target}
            PRIVATE
                ${JNI_INCLUDE_DIRS}
        )
        
        message(STATUS "Added Android library target: ${target}")
    endfunction()
    
    # 创建JNI包装库
    if(SANDVIK_ENABLE_JNI AND SANDVIK_BUILD_SHARED)
        message(STATUS "Creating JNI wrapper library for Android")
        
        # JNI包装源文件
        set(JNI_WRAPPER_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/jni/jni_wrapper.cpp
        )
        
        # 如果JNI包装文件不存在，创建一个简单的版本
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/jni/jni_wrapper.cpp)
            file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/jni/jni_wrapper.cpp
                "// JNI wrapper for Android\n"
                "#include <jni.h>\n"
                "#include \"vm.hpp\"\n"
                "\n"
                "extern \"C\" JNIEXPORT jlong JNICALL\n"
                "Java_com_sandvik_SandvikVM_nativeCreate(JNIEnv* env, jobject thiz) {\n"
                "    try {\n"
                "        sandvik::Vm* vm = new sandvik::Vm();\n"
                "        return reinterpret_cast<jlong>(vm);\n"
                "    } catch (...) {\n"
                "        return 0;\n"
                "    }\n"
                "}\n"
                "\n"
                "extern \"C\" JNIEXPORT void JNICALL\n"
                "Java_com_sandvik_SandvikVM_nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {\n"
                "    if (handle) {\n"
                "        sandvik::Vm* vm = reinterpret_cast<sandvik::Vm*>(handle);\n"
                "        delete vm;\n"
                "    }\n"
                "}\n"
                "\n"
                "extern \"C\" JNIEXPORT jint JNICALL\n"
                "Java_com_sandvik_SandvikVM_nativeExecute(JNIEnv* env, jobject thiz, jlong handle, jstring dexPath) {\n"
                "    if (!handle) return -1;\n"
                "    \n"
                "    try {\n"
                "        sandvik::Vm* vm = reinterpret_cast<sandvik::Vm*>(handle);\n"
                "        const char* path = env->GetStringUTFChars(dexPath, nullptr);\n"
                "        // TODO: 实现DEX执行逻辑\n"
                "        env->ReleaseStringUTFChars(dexPath, path);\n"
                "        return 0;\n"
                "    } catch (...) {\n"
                "        return -1;\n"
                "    }\n"
                "}\n"
            )
        endif()
        
        # 添加JNI包装库
        sandvik_add_android_library(sandvik-jni ${JNI_WRAPPER_SOURCES})
        
        # 链接主库
        target_link_libraries(sandvik-jni
            PRIVATE
                sandvik
        )
        
        # 设置输出名称
        set_target_properties(sandvik-jni PROPERTIES
            OUTPUT_NAME "sandvik_jni"
        )
    endif()
    
    # 生成Android.mk文件
    if(SANDVIK_BUILD_SHARED)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Android.mk.in
            ${CMAKE_CURRENT_BINARY_DIR}/Android.mk
            @ONLY
        )
        
        # 安装Android.mk
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Android.mk
            DESTINATION .
        )
    endif()
    
    message(STATUS "Android configuration complete")
endif()