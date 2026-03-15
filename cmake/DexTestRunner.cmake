#=============================================================================
# DEX测试运行器模块
# 提供运行.dex文件和自定义字节码测试的功能
#=============================================================================

# 函数：添加DEX测试
# 参数：
#   test_name - 测试名称
#   dex_file - .dex文件路径
#   expected_output - 期望输出文件（可选）
#   args - 传递给VM的额外参数（可选）
function(sandvik_add_dex_test test_name dex_file)
    # 检查参数
    if(NOT test_name OR NOT dex_file)
        message(FATAL_ERROR "sandvik_add_dex_test requires test_name and dex_file parameters")
    endif()
    
    # 检查.dex文件是否存在
    if(NOT EXISTS ${dex_file})
        message(WARNING "DEX file not found: ${dex_file}")
        return()
    endif()
    
    # 解析可选参数
    set(expected_output "")
    set(test_args "")
    set(parse_mode "none")
    
    foreach(arg ${ARGN})
        if(arg STREQUAL "EXPECTED_OUTPUT")
            set(parse_mode "expected_output")
        elseif(arg STREQUAL "ARGS")
            set(parse_mode "args")
        elseif(parse_mode STREQUAL "expected_output")
            set(expected_output ${arg})
            set(parse_mode "none")
        elseif(parse_mode STREQUAL "args")
            list(APPEND test_args ${arg})
        else()
            message(WARNING "Unknown argument to sandvik_add_dex_test: ${arg}")
        endif()
    endforeach()
    
    # 确保sandvik-vm目标存在
    if(NOT TARGET sandvik-vm)
        message(WARNING "sandvik-vm target not found, skipping test: ${test_name}")
        return()
    endif()
    
    # 创建测试命令
    set(test_command $<TARGET_FILE:sandvik-vm>)
    
    # 添加参数
    if(test_args)
        foreach(arg ${test_args})
            set(test_command ${test_command} ${arg})
        endforeach()
    endif()
    
    # 添加.dex文件
    set(test_command ${test_command} ${dex_file})
    
    # 创建测试
    add_test(
        NAME ${test_name}
        COMMAND ${test_command}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # 如果有期望输出文件，配置输出验证
    if(expected_output AND EXISTS ${expected_output})
        # 读取期望输出
        file(READ ${expected_output} expected_content)
        
        # 设置测试属性进行输出比较
        set_tests_properties(${test_name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${expected_content}"
        )
        
        message(STATUS "  Added DEX test with output validation: ${test_name}")
    else()
        message(STATUS "  Added DEX test: ${test_name}")
    endif()
    
    # 设置测试标签
    set_tests_properties(${test_name} PROPERTIES
        LABELS "dex"
        TIMEOUT 30
    )
endfunction()

# 函数：添加自定义字节码测试
# 参数：
#   test_name - 测试名称
#   bytecode_file - 字节码文件路径
#   expected_output - 期望输出文件（可选）
function(sandvik_add_bytecode_test test_name bytecode_file)
    # 检查参数
    if(NOT test_name OR NOT bytecode_file)
        message(FATAL_ERROR "sandvik_add_bytecode_test requires test_name and bytecode_file parameters")
    endif()
    
    # 检查字节码文件是否存在
    if(NOT EXISTS ${bytecode_file})
        message(WARNING "Bytecode file not found: ${bytecode_file}")
        return()
    endif()
    
    # 解析可选参数
    set(expected_output "")
    if(ARGC GREATER 2)
        set(expected_output ${ARGV2})
    endif()
    
    # 确保sandvik-vm目标存在
    if(NOT TARGET sandvik-vm)
        message(WARNING "sandvik-vm target not found, skipping test: ${test_name}")
        return()
    endif()
    
    # 创建测试命令（假设VM支持--bytecode参数）
    set(test_command $<TARGET_FILE:sandvik-vm> --bytecode ${bytecode_file})
    
    # 创建测试
    add_test(
        NAME ${test_name}
        COMMAND ${test_command}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # 如果有期望输出文件，配置输出验证
    if(expected_output AND EXISTS ${expected_output})
        # 读取期望输出
        file(READ ${expected_output} expected_content)
        
        # 设置测试属性进行输出比较
        set_tests_properties(${test_name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${expected_content}"
        )
        
        message(STATUS "  Added bytecode test with output validation: ${test_name}")
    else()
        message(STATUS "  Added bytecode test: ${test_name}")
    endif()
    
    # 设置测试标签
    set_tests_properties(${test_name} PROPERTIES
        LABELS "bytecode"
        TIMEOUT 30
    )
endfunction()

# 函数：批量添加DEX测试
# 参数：
#   test_dir - 包含.dex文件的目录
#   pattern - 文件模式（默认为*.dex）
function(sandvik_add_dex_tests_from_dir test_dir)
    # 检查参数
    if(NOT test_dir)
        message(FATAL_ERROR "sandvik_add_dex_tests_from_dir requires test_dir parameter")
    endif()
    
    # 解析可选参数
    set(pattern "*.dex")
    if(ARGC GREATER 1)
        set(pattern ${ARGV1})
    endif()
    
    # 检查目录是否存在
    if(NOT EXISTS ${test_dir})
        message(WARNING "Test directory not found: ${test_dir}")
        return()
    endif()
    
    # 查找所有.dex文件
    file(GLOB dex_files ${test_dir}/${pattern})
    
    # 为每个.dex文件添加测试
    foreach(dex_file ${dex_files})
        # 获取测试名称（使用文件名，不含扩展名）
        get_filename_component(test_name ${dex_file} NAME_WE)
        
        # 查找期望输出文件
        set(expected_output "")
        set(ref_file ${test_dir}/test_${test_name}.ref)
        if(EXISTS ${ref_file})
            set(expected_output ${ref_file})
        endif()
        
        # 添加测试
        if(expected_output)
            sandvik_add_dex_test(${test_name} ${dex_file} EXPECTED_OUTPUT ${expected_output})
        else()
            sandvik_add_dex_test(${test_name} ${dex_file})
        endif()
    endforeach()
    
    message(STATUS "Added ${dex_files} DEX tests from ${test_dir}")
endfunction()

# 函数：创建测试运行器目标
# 参数：
#   target_name - 目标名称
#   test_labels - 要运行的测试标签（可选）
function(sandvik_add_test_runner target_name)
    # 解析可选参数
    set(test_labels "")
    if(ARGC GREATER 1)
        set(test_labels ${ARGV1})
    endif()
    
    # 构建CTest命令
    set(ctest_command ${CMAKE_CTEST_COMMAND} --output-on-failure)
    
    if(test_labels)
        # 如果有指定标签，只运行这些标签的测试
        foreach(label ${test_labels})
            set(ctest_command ${ctest_command} -L ${label})
        endforeach()
    endif()
    
    # 创建自定义目标
    add_custom_target(${target_name}
        COMMAND ${ctest_command}
        COMMENT "Running tests"
        DEPENDS sandvik-vm
    )
    
    message(STATUS "Created test runner target: ${target_name}")
endfunction()

# 函数：验证测试配置
function(sandvik_validate_test_config)
    message(STATUS "Validating test configuration...")
    
    # 检查必要目标
    if(NOT TARGET sandvik-vm)
        message(WARNING "sandvik-vm target not found, tests may not work correctly")
    endif()
    
    # 检查测试目录
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests)
        message(STATUS "  Tests directory: OK")
    else()
        message(WARNING "Tests directory not found")
    endif()
    
    # 检查.dex测试文件
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/java)
        file(GLOB dex_files ${CMAKE_SOURCE_DIR}/tests/java/*/*.dex)
        message(STATUS "  Found ${dex_files} DEX test files")
    endif()
    
    message(STATUS "Test configuration validation complete")
endfunction()