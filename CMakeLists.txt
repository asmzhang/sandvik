cmake_minimum_required(VERSION 3.24)

project(sandvik VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Installation prefix (default: <source>/delivery)
# ---------------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/delivery"
        CACHE PATH "Installation prefix" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(SANDVIK_DEBUG  "Build in debug mode (adds -g, defines __debug__)" OFF)
option(SANDVIK_TESTS  "Build and run unit tests"                         OFF)

# ---------------------------------------------------------------------------
# Compiler selection  (prefer clang/clang++ unless -DSANDVIK_USE_GCC=ON)
# ---------------------------------------------------------------------------
option(SANDVIK_USE_GCC "Use GCC instead of Clang" OFF)
if(NOT SANDVIK_USE_GCC AND NOT CMAKE_C_COMPILER_ID)
    find_program(CLANG_C   clang)
    find_program(CLANG_CXX clang++)
    if(CLANG_C AND CLANG_CXX)
        set(CMAKE_C_COMPILER   ${CLANG_C}   CACHE STRING "C compiler"   FORCE)
        set(CMAKE_CXX_COMPILER ${CLANG_CXX} CACHE STRING "C++ compiler" FORCE)
    endif()
endif()

# Ensure ASM compiler matches the C compiler (prevents defaulting to
# system clang with wrong target triple on Windows).
if(NOT CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}" CACHE STRING "ASM compiler" FORCE)
endif()

# ---------------------------------------------------------------------------
# Compile flags  (mirrors WAF wscript)
# ---------------------------------------------------------------------------
set(SANDVIK_COMPILE_FLAGS
    -fPIC
    -Wall
    -Werror
    -O3
    -Wno-unused-parameter
    -fno-strict-aliasing
    -fomit-frame-pointer
    -march=native
)
if(SANDVIK_DEBUG)
    list(APPEND SANDVIK_COMPILE_FLAGS -g)
    add_compile_definitions(__debug__)
endif()

if(WIN32)
    list(APPEND SANDVIK_COMPILE_FLAGS -DFMT_USE_CONSTEVAL=0)
    # llvm-mingw (UCRT) does not ship liboldnames — remove it from the
    # standard library list if a previous cmake run left it there.
    string(REPLACE "-loldnames" "" CMAKE_C_STANDARD_LIBRARIES   "${CMAKE_C_STANDARD_LIBRARIES}")
    string(REPLACE "-loldnames" "" CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES}")
endif()

add_compile_options(${SANDVIK_COMPILE_FLAGS})

# compat/ must be first so our shim headers override system ones on Windows
include_directories(BEFORE "${CMAKE_SOURCE_DIR}/src/compat")

# ---------------------------------------------------------------------------
# System libraries
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Version header generation  (src/version.in.hpp)
# ---------------------------------------------------------------------------
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(GIT_STATUS)
        set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.in.hpp.in"
    "${CMAKE_BINARY_DIR}/version.in.hpp"
    @ONLY)

# ---------------------------------------------------------------------------
# Parallel job count for ExternalProject builds
# ---------------------------------------------------------------------------
include(ProcessorCount)
ProcessorCount(NPROC)
if(NOT NPROC OR NPROC EQUAL 0)
    set(NPROC 4)
endif()

include(ExternalProject)

# Pass compiler/linker explicitly to every sub-cmake build.
# Do NOT forward CMAKE_TOOLCHAIN_FILE — the Android SDK cmake ignores it in
# favour of its own NDK detection when a toolchain file is present.
set(_EP_CMAKE_ARGS
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_AR=${CMAKE_AR}
    -DCMAKE_RANLIB=${CMAKE_RANLIB}
    -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
    -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
)

# ---------------------------------------------------------------------------
# Helper macro: declare an already-installed (or to-be-built) static library
#
# Usage:
#   sandvik_ext_static(
#       TARGET      <cmake-target-name>
#       INSTALL_DIR <path>
#       LIB_NAME    <libname.a>          # relative to INSTALL_DIR/lib/
#       INCLUDE_DIR <path>               # include dir (defaults to INSTALL_DIR/include)
#       [EP_NAME    <ExternalProject name>]  # if an ExternalProject drives the build
#   )
# ---------------------------------------------------------------------------
macro(sandvik_ext_static)
    cmake_parse_arguments(ARG "" "TARGET;INSTALL_DIR;LIB_NAME;INCLUDE_DIR;EP_NAME" "" ${ARGN})
    if(NOT ARG_INCLUDE_DIR)
        set(ARG_INCLUDE_DIR "${ARG_INSTALL_DIR}/include")
    endif()
    # Create dir so CMake does not reject the path at configure time
    file(MAKE_DIRECTORY "${ARG_INCLUDE_DIR}")
    add_library(${ARG_TARGET} STATIC IMPORTED GLOBAL)
    set_target_properties(${ARG_TARGET} PROPERTIES
        IMPORTED_LOCATION             "${ARG_INSTALL_DIR}/lib/${ARG_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${ARG_INCLUDE_DIR}")
    if(ARG_EP_NAME)
        add_dependencies(${ARG_TARGET} ${ARG_EP_NAME})
    endif()
endmacro()

# ---------------------------------------------------------------------------
# fmt
# ---------------------------------------------------------------------------
set(FMT_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/fmt-bin")
if(NOT EXISTS "${FMT_INSTALL_DIR}/include/fmt/core.h")
    ExternalProject_Add(fmt_ep
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/fmt"
        BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/fmt/build"
        CMAKE_ARGS
            ${_EP_CMAKE_ARGS}
            -DFMT_TEST=OFF
            -DFMT_DOC=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_INSTALL_PREFIX=${FMT_INSTALL_DIR}
        BUILD_COMMAND     ${CMAKE_COMMAND} --build . -- -j${NPROC}
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build . --target install
        BUILD_BYPRODUCTS  "${FMT_INSTALL_DIR}/lib/libfmt.a"
    )
    set(_FMT_EP fmt_ep)
endif()
sandvik_ext_static(
    TARGET      ext_fmt
    INSTALL_DIR ${FMT_INSTALL_DIR}
    LIB_NAME    libfmt.a
    EP_NAME     ${_FMT_EP})

# ---------------------------------------------------------------------------
# LIEF
# ---------------------------------------------------------------------------
set(LIEF_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/LIEF-bin")
if(NOT EXISTS "${LIEF_INSTALL_DIR}/include/LIEF/version.h")
    ExternalProject_Add(lief_ep
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/LIEF"
        BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/LIEF/build"
        CMAKE_ARGS
            ${_EP_CMAKE_ARGS}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_INSTALL_PREFIX=${LIEF_INSTALL_DIR}
            "-DCMAKE_CXX_FLAGS=-include stdlib.h -include cstdlib"
            -DLIEF_EXAMPLES=OFF
            -DLIEF_TESTS=OFF
            -DLIEF_PYTHON_API=OFF
            -DLIEF_C_API=OFF
        BUILD_COMMAND     ${CMAKE_COMMAND} --build . -- -j${NPROC}
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build . --target install
        BUILD_BYPRODUCTS  "${LIEF_INSTALL_DIR}/lib/libLIEF.a"
    )
    set(_LIEF_EP lief_ep)
endif()
sandvik_ext_static(
    TARGET      ext_lief
    INSTALL_DIR ${LIEF_INSTALL_DIR}
    LIB_NAME    libLIEF.a
    EP_NAME     ${_LIEF_EP})

# ---------------------------------------------------------------------------
# libffi  (autotools)
# ---------------------------------------------------------------------------
set(FFI_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/libffi-bin")
if(NOT EXISTS "${FFI_INSTALL_DIR}/lib/libffi.a")
    ExternalProject_Add(ffi_ep
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/libffi"
        BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/libffi/build"
        CMAKE_ARGS
            ${_EP_CMAKE_ARGS}
            -DCMAKE_INSTALL_PREFIX=${FFI_INSTALL_DIR}
            "-DFFI_FFICONFIG_TEMPLATE=${CMAKE_SOURCE_DIR}/cmake/libffi/fficonfig.h.in"
        BUILD_COMMAND     ${CMAKE_COMMAND} --build . -- -j${NPROC}
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build . --target install
        BUILD_BYPRODUCTS  "${FFI_INSTALL_DIR}/lib/libffi.a"
    )
    set(_FFI_EP ffi_ep)
endif()
sandvik_ext_static(
    TARGET      ext_ffi
    INSTALL_DIR ${FFI_INSTALL_DIR}
    LIB_NAME    libffi.a
    EP_NAME     ${_FFI_EP})

# ---------------------------------------------------------------------------
# axml-parser
# ---------------------------------------------------------------------------
set(AXML_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/axml-parser-bin")
if(NOT EXISTS "${AXML_INSTALL_DIR}/lib/libaxml_parser.a")
    ExternalProject_Add(axml_ep
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/axml-parser"
        BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/axml-parser/build"
        CMAKE_ARGS
            ${_EP_CMAKE_ARGS}
            -DAXML_BUILD_DEMO=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_INSTALL_PREFIX=${AXML_INSTALL_DIR}
        BUILD_COMMAND     ${CMAKE_COMMAND} --build . -- -j${NPROC}
        # axml-parser requires manual install (no install target)
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${AXML_INSTALL_DIR}/lib
            COMMAND ${CMAKE_COMMAND} -E make_directory ${AXML_INSTALL_DIR}/include
            COMMAND ${CMAKE_COMMAND} -E copy
                <BINARY_DIR>/libaxml_parser.a ${AXML_INSTALL_DIR}/lib/
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/ext/axml-parser/include ${AXML_INSTALL_DIR}/include
        BUILD_BYPRODUCTS  "${AXML_INSTALL_DIR}/lib/libaxml_parser.a"
    )
    set(_AXML_EP axml_ep)
endif()
sandvik_ext_static(
    TARGET      ext_axml
    INSTALL_DIR ${AXML_INSTALL_DIR}
    LIB_NAME    libaxml_parser.a
    EP_NAME     ${_AXML_EP})

# ---------------------------------------------------------------------------
# xxHash
# ---------------------------------------------------------------------------
set(XXHASH_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/xxHash-bin")
if(NOT EXISTS "${XXHASH_INSTALL_DIR}/lib/libxxhash.a")
    ExternalProject_Add(xxhash_ep
        SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/xxHash/cmake_unofficial"
        BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/xxHash/build"
        CMAKE_ARGS
            ${_EP_CMAKE_ARGS}
            -DBUILD_SHARED_LIBS=OFF
            -DXXHASH_BUILD_XXHSUM=OFF
            -DCMAKE_INSTALL_PREFIX=${XXHASH_INSTALL_DIR}
        BUILD_COMMAND     ${CMAKE_COMMAND} --build .
        INSTALL_COMMAND   ${CMAKE_COMMAND} --build . --target install
        BUILD_BYPRODUCTS  "${XXHASH_INSTALL_DIR}/lib/libxxhash.a"
    )
    set(_XXHASH_EP xxhash_ep)
endif()
sandvik_ext_static(
    TARGET      ext_xxhash
    INSTALL_DIR ${XXHASH_INSTALL_DIR}
    LIB_NAME    libxxhash.a
    EP_NAME     ${_XXHASH_EP})

# ---------------------------------------------------------------------------
# args  (header-only)
# ---------------------------------------------------------------------------
add_library(ext_args INTERFACE)
target_include_directories(ext_args INTERFACE "${CMAKE_SOURCE_DIR}/ext/args")

# ---------------------------------------------------------------------------
# googletest  (only needed for tests)
# ---------------------------------------------------------------------------
if(SANDVIK_TESTS)
    set(GTEST_INSTALL_DIR "${CMAKE_SOURCE_DIR}/ext/googletest-bin")
    if(NOT EXISTS "${GTEST_INSTALL_DIR}/lib/libgtest.a")
        ExternalProject_Add(gtest_ep
            SOURCE_DIR  "${CMAKE_SOURCE_DIR}/ext/googletest"
            BINARY_DIR  "${CMAKE_SOURCE_DIR}/ext/googletest/build"
            CMAKE_ARGS
                ${_EP_CMAKE_ARGS}
                -DBUILD_TESTING=OFF
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_DIR}
            BUILD_COMMAND     ${CMAKE_COMMAND} --build . -- -j${NPROC}
            INSTALL_COMMAND   ${CMAKE_COMMAND} --build . --target install
            BUILD_BYPRODUCTS
                "${GTEST_INSTALL_DIR}/lib/libgtest.a"
                "${GTEST_INSTALL_DIR}/lib/libgtest_main.a"
        )
        set(_GTEST_EP gtest_ep)
    endif()
    sandvik_ext_static(
        TARGET      ext_gtest
        INSTALL_DIR ${GTEST_INSTALL_DIR}
        LIB_NAME    libgtest.a
        EP_NAME     ${_GTEST_EP})
    sandvik_ext_static(
        TARGET      ext_gtest_main
        INSTALL_DIR ${GTEST_INSTALL_DIR}
        LIB_NAME    libgtest_main.a
        EP_NAME     ${_GTEST_EP})
endif()

# ---------------------------------------------------------------------------
# Java runtime: sanddirt.jar -> sanddirt.dex.jar -> sanddirt.dex.jar.o
# The shared library embeds the DEX jar as a binary resource so that
# _binary_sanddirt_dex_jar_start/end/size symbols are available at link time.
# ---------------------------------------------------------------------------
find_package(Java REQUIRED COMPONENTS Development)
include(UseJava)

file(GLOB_RECURSE JAVA_SOURCES "${CMAKE_SOURCE_DIR}/sanddirt/**/*.java")

set(CMAKE_JAVA_COMPILE_FLAGS -source 8 -target 8 -Xlint:-options)
add_jar(sanddirt_jar
    SOURCES     ${JAVA_SOURCES}
    OUTPUT_NAME sanddirt
    OUTPUT_DIR  "${CMAKE_BINARY_DIR}")

get_target_property(SANDDIRT_JAR_FILE sanddirt_jar JAR_FILE)

# d8: convert sanddirt.jar -> sanddirt.dex.jar  (via bundled d8.jar)
find_package(Java REQUIRED COMPONENTS Runtime)
set(D8_JAR "${CMAKE_SOURCE_DIR}/sanddirt/d8.jar")
if(NOT EXISTS "${D8_JAR}")
    message(FATAL_ERROR "d8.jar not found at ${D8_JAR}")
endif()

add_custom_command(
    OUTPUT  "${CMAKE_BINARY_DIR}/sanddirt.dex.jar"
    COMMAND ${Java_JAVA_EXECUTABLE}
            -cp "${D8_JAR}" com.android.tools.r8.D8
            --output "${CMAKE_BINARY_DIR}/sanddirt.dex.jar"
            "${SANDDIRT_JAR_FILE}"
    DEPENDS sanddirt_jar
    COMMENT "Converting sanddirt.jar to DEX")

# Embed sanddirt.dex.jar via .incbin (portable, works with lld on Windows)
set(_DEX_ASM "${CMAKE_BINARY_DIR}/sanddirt_dex_resource.S")
add_custom_command(
    OUTPUT  "${_DEX_ASM}"
    COMMAND ${CMAKE_COMMAND}
            -DDEX_PATH="${CMAKE_BINARY_DIR}/sanddirt.dex.jar"
            -DOUT="${_DEX_ASM}"
            -P "${CMAKE_SOURCE_DIR}/cmake/gen_incbin.cmake"
    DEPENDS "${CMAKE_BINARY_DIR}/sanddirt.dex.jar"
    COMMENT "Generating .incbin assembler for sanddirt.dex.jar")

# ---------------------------------------------------------------------------
# sandvik shared library
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SANDVIK_LIB_SOURCES
    "src/*.cpp"
    "src/*.c")
list(FILTER SANDVIK_LIB_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

add_library(sandvik SHARED
    ${SANDVIK_LIB_SOURCES}
    "${_DEX_ASM}")

target_include_directories(sandvik PUBLIC src "${CMAKE_BINARY_DIR}")

target_link_libraries(sandvik
    PUBLIC
        ext_fmt
        ext_lief
        ext_ffi
        ext_axml
        ext_xxhash
        Threads::Threads
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:psapi>)

if(WIN32)
    target_link_options(sandvik PRIVATE -Wl,--allow-multiple-definition)
else()
    target_link_options(sandvik PRIVATE -Wl,-z,defs)
endif()

set_target_properties(sandvik PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

install(TARGETS sandvik
    LIBRARY DESTINATION lib)

# Install public headers (preserving directory structure)
install(DIRECTORY src/
    DESTINATION include
    FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

# ---------------------------------------------------------------------------
# sandvik executable
# ---------------------------------------------------------------------------
add_executable(sandvik_exe src/main.cpp)

set_target_properties(sandvik_exe PROPERTIES OUTPUT_NAME sandvik)

target_include_directories(sandvik_exe PRIVATE src)

target_link_libraries(sandvik_exe
    PRIVATE
        sandvik
        ext_fmt
        ext_args
        ext_lief
        ext_ffi
        ext_axml
        ext_xxhash
        Threads::Threads)

if(NOT WIN32)
    target_link_options(sandvik_exe PRIVATE -rdynamic -Wl,-z,defs)
endif()

install(TARGETS sandvik_exe
    RUNTIME DESTINATION .)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(SANDVIK_TESTS)
    enable_testing()

    # Build libnative (JNI test library) for VM.Native test
    if(WIN32)
        find_package(JNI QUIET)
        add_library(native SHARED "tests/java/native/native.c")
        set_target_properties(native PROPERTIES
            OUTPUT_NAME "libnative"
            PREFIX ""
            SUFFIX ".dll"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
        target_compile_options(native PRIVATE -Wno-format)
        if(JNI_INCLUDE_DIRS)
            target_include_directories(native PRIVATE ${JNI_INCLUDE_DIRS})
        else()
            # Fallback: locate jni.h from the Java executable path
            get_filename_component(_java_bin_dir "${Java_JAVA_EXECUTABLE}" DIRECTORY)
            get_filename_component(_java_home "${_java_bin_dir}" DIRECTORY)
            target_include_directories(native PRIVATE
                "${_java_home}/include"
                "${_java_home}/include/win32")
        endif()
    endif()

    file(GLOB_RECURSE UNIT_TEST_SOURCES "tests/unit/test_*.cpp")

    foreach(test_src ${UNIT_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)

        add_executable(${test_name} ${test_src})

        # gtest headers trigger -Wcharacter-conversion with clang >= 19
        target_compile_options(${test_name} PRIVATE -Wno-character-conversion)

        target_compile_definitions(${test_name} PRIVATE __unit_test__)

        target_include_directories(${test_name} PRIVATE
            .
            src
            "${CMAKE_SOURCE_DIR}/ext/googletest/googletest/include")

        target_link_libraries(${test_name} PRIVATE
            sandvik
            ext_gtest
            ext_gtest_main
            ext_fmt
            Threads::Threads)

        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
