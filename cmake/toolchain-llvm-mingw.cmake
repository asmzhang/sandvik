set(LLVM_MINGW_ROOT "D:/platform/llvm-mingw-20260311-ucrt-x86_64")
set(TRIPLE "x86_64-w64-mingw32")

set(CMAKE_SYSTEM_NAME    Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-clang.exe")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-clang++.exe")
set(CMAKE_AR           "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-llvm-ar.exe")
set(CMAKE_RANLIB       "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-llvm-ranlib.exe")
set(CMAKE_RC_COMPILER  "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-windres.exe")
set(CMAKE_LINKER       "${LLVM_MINGW_ROOT}/bin/${TRIPLE}-ld")

# Find libraries/includes only in the toolchain sysroot, not the host
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/${TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
