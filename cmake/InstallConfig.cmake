#=============================================================================
# 安装配置模块
# 配置跨平台的安装规则
#=============================================================================

# 安装配置
if(SANDVIK_BUILD_SHARED OR SANDVIK_BUILD_EXECUTABLE)
    # 简化安装：只安装库和头文件，不导出目标
    message(STATUS "Using simplified installation (no target export)")
    
    # 只生成版本文件（可选）
    if(SANDVIK_INSTALL_CONFIG_FILES)
        include(CMakePackageConfigHelpers)
        
        # 版本配置文件
        write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/sandvik-config-version.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
        )
        
        # 安装版本文件
        install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/sandvik-config-version.cmake
            DESTINATION lib/cmake/sandvik
        )
    endif()
    
    # 安装文档
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            DESTINATION share/doc/sandvik
        )
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
            DESTINATION share/doc/sandvik
        )
    endif()
    
    # 安装示例（如果存在）
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples
            DESTINATION share/sandvik
        )
    endif()
    
    # Android特定安装
    if(SANDVIK_PLATFORM_ANDROID)
        # 安装Android.mk文件（如果存在）
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Android.mk)
            install(FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/Android.mk
                DESTINATION .
            )
        endif()
        
        # 安装Application.mk文件（如果存在）
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Application.mk)
            install(FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/Application.mk
                DESTINATION .
            )
        endif()
        
        # 安装JNI头文件
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/jni
            DESTINATION include
            FILES_MATCHING
                PATTERN "*.h"
        )
    endif()
    
    message(STATUS "Install configuration complete")
endif()

# 打包目标
if(SANDVIK_BUILD_SHARED OR SANDVIK_BUILD_EXECUTABLE)
    set(CPACK_PACKAGE_NAME "sandvik")
    set(CPACK_PACKAGE_VENDOR "Sandvik Project")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION "Minimal Dalvik JVM written in C++")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dalvik Virtual Machine Implementation")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/christophe.duvernois/sandvik")
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
    set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    
    # 平台特定设置
    if(SANDVIK_PLATFORM_WINDOWS)
        set(CPACK_GENERATOR "ZIP;NSIS")
        set(CPACK_NSIS_MODIFY_PATH ON)
        set(CPACK_NSIS_DISPLAY_NAME "Sandvik Dalvik VM")
        set(CPACK_NSIS_PACKAGE_NAME "Sandvik")
    elseif(SANDVIK_PLATFORM_MACOS)
        set(CPACK_GENERATOR "ZIP;DragNDrop")
        set(CPACK_DMG_VOLUME_NAME "Sandvik")
    else()
        # Linux/Android
        set(CPACK_GENERATOR "ZIP;TGZ")
    endif()
    
    # 包含CPack
    include(CPack)
endif()