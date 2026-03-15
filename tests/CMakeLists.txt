#=============================================================================
# 测试配置
# 集成单元测试和功能测试
#=============================================================================

# 检查是否启用测试
if(NOT SANDVIK_BUILD_TESTS)
    message(STATUS "Tests disabled, skipping test configuration")
    return()
endif()

message(STATUS "Configuring tests")

# 包含Google Test
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    
    # 配置Google Test
    set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock")
    set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation")
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT")
    
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# 启用测试
enable_testing()

# 单元测试配置
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unit)
    message(STATUS "Configuring unit tests")
    
    # 收集单元测试源文件
    file(GLOB UNIT_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
    )
    
    # 创建单元测试可执行文件
    foreach(test_source ${UNIT_TEST_SOURCES})
        # 获取测试名称
        get_filename_component(test_name ${test_source} NAME_WE)
        
        # 创建测试目标
        add_executable(${test_name} ${test_source})
        
        # 链接库
        target_link_libraries(${test_name}
            PRIVATE
                sandvik
                gtest_main
                ${SANDVIK_DEPENDENCIES}
        )
        
        # 包含目录
        target_include_directories(${test_name}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
                ${googletest_SOURCE_DIR}/googletest/include
        )
        
        # 配置编译选项
        if(COMMAND sandvik_configure_target)
            sandvik_configure_target(${test_name})
        endif()
        
        # 添加测试
        add_test(
            NAME ${test_name}
            COMMAND ${test_name}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        
        # 设置测试属性
        set_tests_properties(${test_name} PROPERTIES
            TIMEOUT 30
            LABELS "unit"
        )
        
        message(STATUS "  Added unit test: ${test_name}")
    endforeach()
endif()

# 功能测试配置
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/java)
    message(STATUS "Configuring functional tests")
    
    # 创建功能测试目标
    add_custom_target(run-functional-tests
        COMMENT "Running functional tests"
        DEPENDS sandvik-vm
    )
    
    # 添加各个功能测试
    set(FUNCTIONAL_TESTS
        hello
        add
        fib
        dalvik
        native
    )
    
    foreach(test_name ${FUNCTIONAL_TESTS})
        set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/java/${test_name})
        set(dex_file ${test_dir}/classes.dex)
        set(ref_file ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_${test_name}.ref)
        
        if(EXISTS ${dex_file})
            # 创建测试命令
            add_test(
                NAME functional-${test_name}
                COMMAND $<TARGET_FILE:sandvik-vm> ${dex_file}
                WORKING_DIRECTORY ${test_dir}
            )
            
            # 如果有参考输出文件，设置输出比较
            if(EXISTS ${ref_file})
                set_tests_properties(functional-${test_name} PROPERTIES
                    PASS_REGULAR_EXPRESSION "FILE: ${ref_file}"
                    LABELS "functional"
                )
            else()
                set_tests_properties(functional-${test_name} PROPERTIES
                    LABELS "functional"
                )
            endif()
            
            # 添加到自定义目标
            add_custom_command(
                TARGET run-functional-tests
                POST_BUILD
                COMMAND ${CMAKE_CTEST_COMMAND} -R functional-${test_name} --output-on-failure
                COMMENT "Running ${test_name} functional test"
            )
            
            message(STATUS "  Added functional test: ${test_name}")
        endif()
    endforeach()
    
    # 添加单元DEX测试
    set(unit_dex_file ${CMAKE_CURRENT_SOURCE_DIR}/java/unit/TestUnitDex.jar)
    if(EXISTS ${unit_dex_file})
        add_test(
            NAME functional-unit-dex
            COMMAND $<TARGET_FILE:sandvik-vm> ${unit_dex_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/java/unit
        )
        
        set_tests_properties(functional-unit-dex PROPERTIES
            LABELS "functional"
            TIMEOUT 60
        )
        
        add_custom_command(
            TARGET run-functional-tests
            POST_BUILD
            COMMAND ${CMAKE_CTEST_COMMAND} -R functional-unit-dex --output-on-failure
            COMMENT "Running unit DEX functional test"
        )
        
        message(STATUS "  Added unit DEX functional test")
    endif()
endif()

# 创建运行所有测试的目标
add_custom_target(run-all-tests
    COMMENT "Running all tests"
    DEPENDS sandvik-vm
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
)

# 测试发现和配置完成
message(STATUS "Test configuration complete")
message(STATUS "  Use 'make run-all-tests' to run all tests")
message(STATUS "  Use 'make run-functional-tests' to run only functional tests")
message(STATUS "  Use 'ctest -L unit' to run only unit tests")
message(STATUS "  Use 'ctest -L functional' to run only functional tests")