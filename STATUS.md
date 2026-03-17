# Sandvik — 实现状态与兼容性说明

## 1. 项目适用场景

Sandvik 是一个 **C++ 实现的最小化 Dalvik 虚拟机**，用于在非 Android 环境（Linux / Windows）中解释执行 `.dex` 字节码。

### 典型用途

| 场景 | 说明 |
|------|------|
| 逆向分析 | 在受控环境中运行 APK/DEX，观察运行时行为，无需真机或模拟器 |
| 单元测试 | 对 Android Java 逻辑做白盒测试，脱离 Android 框架 |
| 自动化分析 | 批量执行 DEX，提取字符串、网络请求等动态特征 |
| 教学/研究 | 研究 Dalvik 字节码语义、JNI 调用约定 |

### 不适用场景

- 运行依赖 Android SDK/Framework API 的完整 APK（ActivityManager、Context 等均未实现）
- 需要 ART 特性的代码（ART AOT 优化、profile-guided compilation）
- 高性能场景（纯解释执行，无 JIT）

---

## 2. 字节码支持状态

### 已完整实现（222/256 opcodes）

| 类别 | 指令 |
|------|------|
| 数据移动 | `move`, `move-wide`, `move-object`, `move-result-*` |
| 常量加载 | `const/4`, `const/16`, `const-wide`, `const-string`, `const-class` |
| 实例字段 | `iget-*`, `iput-*`（全部类型变体） |
| 静态字段 | `sget-*`, `sput-*`（全部类型变体） |
| 数组 | `aget-*`, `aput-*`, `array-length`, `new-array`, `filled-new-array` |
| 方法调用 | `invoke-virtual`, `invoke-direct`, `invoke-static`, `invoke-interface`, `invoke-super` |
| 算术 | add/sub/mul/div/rem, 位运算, 移位, 类型转换（全部整型+浮点变体） |
| 控制流 | `if-eq/ne/lt/ge/gt/le`, `goto`, `packed-switch`, `sparse-switch` |
| 类型检查 | `check-cast`, `instance-of` |
| 对象 | `new-instance`, `monitor-enter/exit`, `throw` |
| 比较 | `cmpl-float/double`, `cmpg-float/double`, `cmp-long` |

### 未实现 / 会抛出异常

| 指令 | opcode | 错误信息 |
|------|--------|----------|
| `filled-new-array/range` | `0x25` | `VmException: filled_new_array_range not implemented` |
| 保留指令 `0x3E–0x43` | — | `VmException: invalid instruction` |
| 保留指令 `0x79–0x7A` | — | `VmException: invalid instruction` |

> **注意**：现代 d8 编译器（Android SDK Build Tools ≥ 30）基本不生成 `filled-new-array/range`，实际触发概率低。

---

## 3. Java 标准库支持状态

### 已实现的 Native 方法

| 类 | 方法 |
|----|------|
| `java.io.PrintStream` | `println()`（int/long/float/double/String/Object 重载） |
| `java.lang.Object` | `hashCode()`, `equals()`, `toString()` |
| `java.lang.String` | 基本字符串操作 |
| `java.lang.StringBuilder` | `append()`, `toString()` |
| `java.lang.Class` | `getName()`, `forName()`, `getDeclaredConstructor()`, `getField()` |
| `java.lang.System` | `loadLibrary()`, `identityHashCode()`, `getProperty()` |
| `java.lang.Thread` | `start()`, `join()`（无超时）, `sleep()` |
| `java.lang.Integer/Long/Float/Double` | `valueOf()`, `intValue()` 等装箱/拆箱 |
| `java.lang.Throwable` | `getMessage()`, `printStackTrace()` |
| `java.lang.reflect.Array` | `newInstance()`, `get()` |
| `java.util.concurrent.atomic` | `AtomicInteger`, `AtomicLong` 的 CAS 操作 |

### 调用会直接崩溃 / 抛出 VmException

以下调用在 `src/jni.cpp` 中以 `throw VmException("... not implemented")` 实现，调用即崩溃：

```
JNIEnv::Throw / ThrowNew           ← native 代码抛 Java 异常
JNIEnv::ExceptionOccurred          ← 检查待处理异常
JNIEnv::AllocObject / NewObject    ← native 侧创建对象
JNIEnv::NewObjectArray             ← native 侧创建数组
JNIEnv::GetIntArrayElements 等     ← 数组元素访问
JNIEnv::SetIntArrayRegion 等       ← 数组区域写入
JNIEnv::GetStringChars             ← 字符串底层访问
JNIEnv::CallNonvirtualXxxMethod    ← 非虚方法回调
JNIEnv::MonitorEnter / MonitorExit ← JNI 侧加锁
JNIEnv::DefineClass                ← 动态加载类
JNIEnv::FromReflectedMethod        ← 反射转 JNI
```

### 调用会打印警告但不崩溃

```
JNIEnv::AttachCurrentThread        ← 返回 JNI_OK，实际无操作
JNIEnv::DetachCurrentThread        ← 同上
JNIEnv::ExceptionCheck             ← 始终返回 false
Thread.join(long timeout)          ← 超时参数被忽略
```

---

## 4. JNI / .so 交互问题

### 4.1 当前流程

```
Java: System.loadLibrary("native")
  └─▶ System.cpp: loadLibrary()
        └─▶ Vm::loadLibrary("libnative.dll" / "libnative.so")
              ├─ SharedLibrary::load()         ← dlopen / LoadLibrary
              ├─ 查找 JNI_OnLoad 符号
              │     └─ 调用 JNI_OnLoad(nullptr, nullptr)  ← ❌ JavaVM* 传 nullptr
              └─ 存入 _sharedlibs[]

Java: native方法调用
  └─▶ Interpreter → findNativeSymbol("Java_pkg_Class_method")
        └─▶ libffi: 动态调用函数指针
```

### 4.2 已知 JNI 问题

#### 问题 1：`JNI_OnLoad` 收到空指针

**位置**：`src/vm.cpp:138`

```cpp
// TODO: pass actual JavaVM
jint (*on_load)(JavaVM*, void*) = ...;
on_load(nullptr, nullptr);   // ← nullptr 导致库内崩溃
```

**影响**：任何在 `JNI_OnLoad` 中调用 `vm->GetEnv()` 的库（如 Frida、大多数加固 SDK）都会立即段错误。

**修复方向**：
```cpp
// vm.cpp 中需要一个实现了 JavaVM 接口的对象
JavaVM* jvm = _javaVm.get();   // 需要实现 JavaVM 包装类
on_load(jvm, nullptr);
```

---

#### 问题 2：native 代码无法抛出 Java 异常

**影响**：凡是通过 `env->Throw()` / `env->ThrowNew()` 向 Java 层报错的 native 库，调用即抛 `VmException` 而非正常传递。

**修复方向**：
```cpp
// jni.cpp 中 Throw() 实现应改为：
jint Throw(jthrowable obj) override {
    _pendingException = obj;   // 挂起异常
    return 0;
}
// 解释器在每次方法返回时检查 _pendingException 并处理
```

---

#### 问题 3：数组操作完全缺失

**影响**：native 代码无法读写 Java 数组内容（图像处理、加密运算等常见场景）。

**最高频缺失函数**：
```
GetByteArrayElements / ReleaseByteArrayElements
GetIntArrayElements  / ReleaseIntArrayElements
SetByteArrayRegion   / GetByteArrayRegion
NewByteArray         / NewIntArray
GetArrayLength       ← 有实现，但 NewXxxArray 缺失
```

---

#### 问题 4：`RegisterNatives` 仅支持静态符号查找

当 native 库通过 `RegisterNatives` 注册方法时，Sandvik 能正确处理（`src/jni.cpp:1710`）。但若 native 库只导出标准命名函数（`Java_pkg_Class_method`），且符号名含 `$`（嵌套类、lambda）或经过混淆，查找会失败并抛出：

```
VmException: Native method java.lang.XXX::yyy is not available!
```

---

#### 问题 5：库文件扩展名硬编码

**位置**：`src/native/java/lang/System.cpp`

```cpp
#ifdef _WIN32
    loadLibrary(fmt::format("lib{}.dll", name->str()));
#else
    loadLibrary(fmt::format("lib{}.so", name->str()));
#endif
```

不支持带路径的 `loadLibrary`（如 `System.loadLibrary("/data/local/libnative.so")`），也不支持 `.so` 版本后缀（`libnative.so.1`）。

---

### 4.3 JNI 功能完善路线图

优先级从高到低：

| 优先级 | 任务 | 影响范围 |
|--------|------|----------|
| P0 | 实现 `JavaVM` 包装，传入 `JNI_OnLoad` | 所有使用 JNI_OnLoad 的库 |
| P0 | 实现 `GetByteArrayElements` / `SetByteArrayRegion` 等 | 几乎所有实际 native 库 |
| P0 | 实现 `Throw` / `ThrowNew` / `ExceptionOccurred` | 带错误处理的 native 代码 |
| P1 | 实现 `NewByteArray` / `NewIntArray` 等 | native 侧构造数组返回 Java |
| P1 | 实现 `NewStringUTF` / `GetStringUTFChars` | 字符串传递 |
| P1 | 实现 `CallVoidMethod` / `CallIntMethod` 等回调 Java | 回调型 native 库 |
| P2 | 实现 `MonitorEnter` / `MonitorExit` | synchronized native 块 |
| P2 | 实现 `AllocObject` / `NewObject` | native 侧创建 Java 对象 |
| P3 | 支持带路径的 `loadLibrary` | 路径硬编码的 APK |
| P3 | 实现 `Thread.join(long timeout)` | 带超时的线程等待 |

---

## 5. 常见报错速查

| 报错信息 | 触发场景 | 解决方案 |
|----------|----------|----------|
| `filled_new_array_range not implemented` | 数组初始化（旧 dx 编译器） | 用 d8 重新编译 DEX |
| `Native method X is not available!` | `loadLibrary` 后找不到符号 | 确认 .dll/.so 在同目录，符号名未混淆 |
| `Cannot open library X` | 库文件不存在或依赖缺失 | 检查路径，Windows 下检查 DLL 依赖链 |
| `VmException: ... not implemented` (JNI) | 调用了未实现的 JNI 函数 | 查看上方 P0/P1 列表，手动实现 |
| 段错误 / `STATUS_ACCESS_VIOLATION` | `JNI_OnLoad` 收到 nullptr | 等待 JavaVM 包装实现（P0） |
| `invalid instruction` | 保留/未知 opcode | 检查 DEX 是否损坏或来自非标准编译器 |
| `Object.clone() not implemented` | 枚举（Enum.values()）、数组 clone | 需在 VM 层面特殊处理数组 clone |

---

## 6. 测试覆盖情况

```
tests/
├── unit/                    C++ 单元测试（gtest）
│   ├── test_gc              GC 正确性
│   ├── test_object          对象模型
│   ├── test_stream          字节流
│   ├── test_thread          线程 / monitor
│   └── test_zip             ZIP/JAR 读取
│
└── java/                    DEX 集成测试
    ├── add/         VM.Add          ✅ 通过
    ├── hello/       VM.Hello        ✅ 通过
    ├── fib/         VM.Fibonacci    ✅ 通过
    ├── dalvik/      VM.Dalvik       ✅ 176/176 子测试通过
    ├── native/      VM.Native       ⚠️  基本通过（%lx 格式 Windows 下 32/64 位差异）
    └── unit/        VM.Enums        ❌ Object.clone() 未实现
```

### 50 个 Java 单元测试涵盖

算术边界、数组、接口/抽象类、内部类、多线程、Switch（含字符串）、自动装箱、异常处理、反射、正则表达式、默认方法、枚举、原子操作、递归、集合操作等。

---

## 7. 项目架构速览

```
Vm::run()
  ├─ ClassLoader::load(dex/apk)          ← LIEF 解析 DEX
  ├─ Thread::create() → Interpreter::run()
  │     └─ switch(opcode) → handler()   ← 222 个 opcode 处理器
  │           ├─ invoke-virtual/static   ← 查 vtable / ClassLoader
  │           ├─ native 方法             ← Vm::findNativeSymbol() → libffi 调用
  │           └─ 异常处理               ← try_handlers 表查找
  ├─ GC::collect()                       ← 标记-清除，跨线程
  └─ JNIEnv (NativeInterface)            ← C++ 实现 JNINativeInterface_
        ├─ RegisterNatives               ← 符号映射
        ├─ GetXxxField / SetXxxField     ← 字段访问
        └─ CallXxxMethod (部分)          ← Java 回调
```
