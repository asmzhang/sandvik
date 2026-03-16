# Sandvik — CMake 构建说明

## 依赖


| 工具     | 版本    | 说明                 |
| -------- | ------- | -------------------- |
| CMake    | ≥ 3.24 | 构建系统             |
| Ninja    | 任意    | 推荐构建后端         |
| Clang    | ≥ 14   | C++20 编译器         |
| Java JDK | ≥ 8    | 编译 sanddirt 运行时 |

**Windows 工具链**：`D:\platform\llvm-mingw-20260311-ucrt-x86_64`

---

## Linux 构建

```bash
git submodule update --init --recursive

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build

./build/sandvik --dex tests/java/add/classes.dex --main Add 5 10
```

---

## Windows 构建

```powershell
git submodule update --init --recursive

cmake -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DLLVM_MINGW_ROOT=D:/platform/llvm-mingw-20260311-ucrt-x86_64 `
  -B build -S .

ninja -C build

.\build\sandvik.exe --dex tests\java\add\classes.dex --main Add 5 10
```

---

## CMake 选项


| 选项              | 默认 | 说明                                |
| ----------------- | ---- | ----------------------------------- |
| `SANDVIK_DEBUG`   | OFF  | 添加`-g` 调试符号，定义 `__debug__` |
| `SANDVIK_TESTS`   | OFF  | 构建单元测试                        |
| `SANDVIK_USE_GCC` | OFF  | 使用 GCC 代替 Clang                 |

---

## 运行测试

```powershell
# 构建（含测试）
cmake -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DLLVM_MINGW_ROOT=D:/platform/llvm-mingw-20260311-ucrt-x86_64 `
  -DSANDVIK_TESTS=ON `
  -B build -S .
ninja -C build

# 运行全部测试
ctest --output-on-failure -j1 --test-dir build

# 运行指定测试
build/test_dalvik.exe --gtest_filter=VM.Add
```

---

## 调试构建

```powershell
cmake -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DLLVM_MINGW_ROOT=D:/platform/llvm-mingw-20260311-ucrt-x86_64 `
  -DSANDVIK_DEBUG=ON `
  -DSANDVIK_TESTS=ON `
  -B build-debug -S .
ninja -C build-debug
```

VS Code 调试：安装 `vadimcn.vscode-lldb`，按 F5 选择配置（见 `.vscode/launch.json`）。

---

## 构建产物


| 目标             | 产物                                       |
| ---------------- | ------------------------------------------ |
| `sandvik`        | `libsandvik.dll` / `libsandvik.so`         |
| `sandvik_exe`    | `sandvik.exe` / `sandvik`                  |
| `test_dalvik` 等 | 单元测试可执行文件                         |
| *(自动)*         | `sanddirt.dex.jar`（d8 转换的 DEX 运行时） |

---

## 项目结构

```
sandvik/
├── CMakeLists.txt
├── cmake/
│   ├── gen_incbin.cmake          # 生成 .incbin ASM 嵌入 DEX
│   ├── toolchain-llvm-mingw.cmake
│   └── version.in.hpp.in
├── src/
│   ├── compat/                   # Windows 兼容层
│   │   ├── dlfcn.h               # dlopen/dlsym → LoadLibrary/GetProcAddress
│   │   └── byteswap.h            # bswap_* → _byteswap_*
│   └── ...
├── sanddirt/
│   ├── d8.jar                    # Android D8 DEX 编译器
│   └── java/                     # Java 运行时源码
├── ext/                          # 第三方依赖（首次构建自动编译）
│   ├── fmt/          → fmt-bin/
│   ├── LIEF/         → LIEF-bin/
│   ├── libffi/       → libffi-bin/
│   ├── axml-parser/  → axml-parser-bin/
│   ├── xxHash/       → xxHash-bin/
│   ├── googletest/   → googletest-bin/
│   └── args/         （header-only）
└── tests/
    ├── java/                     # Java 测试程序
    └── unit/                     # C++ 单元测试（gtest）
```

---

## Windows 兼容性说明


| 问题                                             | 解决方案                                                                |
| ------------------------------------------------ | ----------------------------------------------------------------------- |
| `dlfcn.h` 不存在                                 | `src/compat/dlfcn.h`：`LoadLibraryA` / `GetProcAddress` 实现            |
| `byteswap.h` 不存在                              | `src/compat/byteswap.h`：`_byteswap_*` 实现                             |
| `ld -r -b binary` 不支持（lld/PE）               | `cmake/gen_incbin.cmake` 生成 `.incbin` ASM 嵌入 DEX                    |
| `System.loadLibrary` 加载 `.so`                  | `System.cpp` 中 `#ifdef _WIN32` 改用 `.dll` 扩展名                      |
| fmt`color.h` consteval bug（clang 22）           | `FMT_STRING("{}") → fmt::runtime("{}")`                                |
| libffi`.hidden` 指令（ELF only）                 | `fficonfig.h` 注释掉 `HAVE_HIDDEN_VISIBILITY_ATTRIBUTE`                 |
| fmt / LIEF 重复符号                              | 链接选项`-Wl,--allow-multiple-definition`                               |
| `inet_pton` 未定义                               | 链接`ws2_32`                                                            |
| DLL 单例边界问题                                 | `GC`、`Logger`、`Trace` 各自提供显式 `getInstance()` 实现               |
| 运行时缺 DLL                                     | 已解决：CMake 自动探测并静态链接`libc++.a`、`libunwind.a`，无需额外 DLL |
| CMake cache 污染（`-loldnames`、NDK ASM 编译器） | 删除`build/` 目录，重新配置时显式传 `CMAKE_ASM_COMPILER`                |
| libffi`fficonfig.h` 缺失（GitHub 屏蔽/未提交）   | 已解决：由`ext/libffi/fficonfig.h.in` 在 CMake 配置时自动生成           |
| libffi`win64.S` 含 `.hidden`（ELF-only 指令）    | 已解决：CMakeLists 运行时自动生成去掉`.hidden` 的 `win64_pe.S`          |
