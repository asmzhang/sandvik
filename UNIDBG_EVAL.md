# Sandvik × unidbg 集成评估

## 1. 两者定位对比

| 维度 | sandvik | unidbg |
|------|---------|--------|
| 语言 | C++ | Java（运行在 JVM 上） |
| 执行目标 | Dalvik DEX 字节码（x86 解释执行） | ARM/ARM64 native .so（Unicorn 仿真） |
| Java VM | 自有 C++ Dalvik VM | 假 Java VM（AbstractJni 返回 stub） |
| JNI 方向 | Java→native（DEX 调用 .so） | native→Java（ARM .so 回调 Java） |
| 典型场景 | 分析 Java 逻辑、DEX 行为 | 分析 ARM 加密算法、so 脱壳 |
| 架构依赖 | 宿主 CPU（x86/ARM 均可编译） | Unicorn（CPU 级 ARM 仿真） |

**核心差距**：sandvik 解决 DEX→native 方向；unidbg 解决 native→Java 方向。两者正好互补，但接口模型完全不同。

---

## 2. 可能的集成方向

### 方案 A：sandvik 替换 unidbg 的 AbstractJni（最有价值）

```
unidbg
  └─ Unicorn 仿真 ARM native .so
       └─ ARM JNI 调用（FindClass, CallObjectMethod...）
            └─ unidbg 拦截 → AbstractJni 子类
                              └─ JNI 桥接 → sandvik C++ VM
                                             └─ 真实 DEX 执行结果返回
```

**意义**：unidbg 目前的 AbstractJni 全是手写 stub（硬编码返回值），无法处理复杂的 Java 逻辑分支。接入 sandvik 后，ARM native 代码回调的 Java 方法将真实执行，分析结果更准确。

**技术路径**：
1. 将 sandvik 编译为 JNI 动态库（`libsandvik-java.dll/.so`），暴露 Java 可调用接口
2. 在 unidbg 中实现 `AbstractJni` 子类，所有 JNI 回调委托给 sandvik
3. 对象身份映射：unidbg 用 `long`（ARM 内存地址）表示 jobject，sandvik 用 `ObjectRef`（C++ 指针），需要维护双向映射表

**对接难度：高**

---

### 方案 B：unidbg 作为 sandvik 的 ARM 执行后端

```
sandvik DEX 解释器
  └─ System.loadLibrary("native")
       └─ 检测到 ARM ELF → 转发给 unidbg 进程
            └─ Unicorn 加载 ARM .so
                 └─ JNI 回调通过 IPC 路由回 sandvik
```

**意义**：sandvik 当前只能 dlopen x86/x64 native 库，遇到 ARM-only APK 直接失败。接入 unidbg 后可在 x86 主机上运行含 ARM native 代码的完整 APK。

**技术路径**：IPC（本地 socket 或共享内存）传递 JNI 调用

**对接难度：高**（需要 IPC 协议 + 两个进程间对象生命周期管理）

---

### 方案 C：并行互补使用（现阶段最实际）

```
DEX 分析  →  sandvik    字符串解密、逻辑还原、算法识别
ARM 分析  →  unidbg     so 脱壳、加密算法还原、反检测
人工关联  →  分析人员    将两侧结果对齐
```

**无需任何代码修改，立即可用。**

---

## 3. 技术障碍逐条评估

### 障碍 1：语言边界（C++ ↔ Java）

sandvik 是纯 C++，unidbg 是 Java。对接必须通过 JNI：

```
unidbg (Java)
  → 调用 sandvik.dll 中的 Java native 方法
    → C++ Vm 实例处理
      → 结果通过 JNI 返回 Java
```

- sandvik 需要新增一层 Java API 包装（约 500-1000 行）
- 每次 JNI 回调都有类型转换开销
- **评估**：可行，但需要 1-2 周专门开发包装层

---

### 障碍 2：对象模型不兼容

| | unidbg | sandvik |
|--|--------|---------|
| jobject 表示 | `long`（ARM 虚拟地址） | `ObjectRef`（C++ 堆指针） |
| jstring | ARM 内存中的 UTF-16 | C++ `std::string` 包装 |
| jbyteArray | ARM 内存缓冲区 | C++ `std::vector<uint8_t>` |
| 生命周期 | 手动管理 | GC 管理 |

集成时需要一个**双向句柄表**：

```cpp
// 概念示意
std::unordered_map<long, ObjectRef> armAddrToSandvik;
std::unordered_map<ObjectRef, long> sandvikToArmAddr;
```

每次跨边界调用都要查表 + 深拷贝（字符串/数组），性能和正确性都是挑战。

---

### 障碍 3：sandvik JNI 缺口（阻塞性）

unidbg 的 ARM native 代码在回调 Java 时，最常用的 JNI 函数在 sandvik 中大量未实现：

| unidbg 常用回调 | sandvik 状态 | 阻塞程度 |
|----------------|-------------|---------|
| `NewStringUTF` | ❌ 未实现 → 崩溃 | **阻塞** |
| `GetStringUTFChars` | ❌ 未实现 → 崩溃 | **阻塞** |
| `NewByteArray` | ❌ 未实现 → 崩溃 | **阻塞** |
| `GetByteArrayElements` | ❌ 未实现 → 崩溃 | **阻塞** |
| `SetByteArrayRegion` | ❌ 未实现 → 崩溃 | **阻塞** |
| `CallStaticObjectMethod` | ⚠️ 部分实现 | 高风险 |
| `Throw / ThrowNew` | ❌ 未实现 → 崩溃 | **阻塞** |
| `ExceptionOccurred` | ⚠️ 返回 false | 逻辑错误 |
| `MonitorEnter/Exit` | ❌ 未实现 → 崩溃 | 中风险 |

**在修复 P0 JNI 缺口之前，任何集成都无法正常运行。**

---

### 障碍 4：JavaVM* 为空（立即崩溃）

```cpp
// src/vm.cpp:138 当前代码
on_load(nullptr, nullptr);   // ← nullptr 传给 JNI_OnLoad
```

unidbg 加载的几乎所有真实 ARM .so 在 `JNI_OnLoad` 中都会执行：

```c
// ARM native 库的典型 JNI_OnLoad
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);  // ← vm 为 null → 段错误
    ...
}
```

**修复前提**：实现 `JavaVM` 包装对象并传入 `JNI_OnLoad`（对应 STATUS.md P0 项）。

---

### 障碍 5：RegisterNatives 是空实现

```cpp
// src/jni.cpp 当前状态
jint NativeInterface::RegisterNatives(...) {
    // 只打日志，不存储映射！
    return JNI_OK;
}
```

大量 ARM native 库（尤其加固 SDK）通过 `RegisterNatives` 注册方法，注册后再调用就找不到符号，报：

```
VmException: Native method X is not available!
```

这是集成前必须修复的核心缺陷。

---

## 4. 修复路线（集成前置条件）

若决定推进集成，需按以下顺序修复 sandvik：

```
第一阶段（1-2 周）—— 基础 JNI 修复
├─ [1] 实现 JavaVM 包装，正确传入 JNI_OnLoad
├─ [2] 修复 RegisterNatives，存储并可查找映射
├─ [3] 实现 NewStringUTF / GetStringUTFChars / ReleaseStringUTFChars
└─ [4] 实现 NewByteArray / GetByteArrayElements / ReleaseByteArrayElements / SetByteArrayRegion

第二阶段（1-2 周）—— Java ↔ native 回调
├─ [5] 实现 Throw / ThrowNew / ExceptionOccurred / ExceptionClear
├─ [6] 实现 CallVoidMethod / CallIntMethod / CallObjectMethod（可变参数版）
└─ [7] 实现 NewIntArray / NewObjectArray / GetArrayLength

第三阶段（2-4 周）—— 集成包装层
├─ [8] 将 sandvik 编译为 Java 可加载的 JNI 库
├─ [9] 实现 Java 侧 SandvikVM 包装类
├─ [10] 实现双向对象句柄映射表
└─ [11] 在 unidbg 中实现 AbstractJni 子类委托给 sandvik
```

---

## 5. 综合评估

### 可行性

| 集成方案 | 技术可行性 | 工作量 | 推荐度 |
|----------|-----------|--------|--------|
| A：sandvik 替代 AbstractJni | ✅ 可行 | 6-10 周 | ★★★ |
| B：unidbg 作为 ARM 后端 | ✅ 可行 | 8-16 周 | ★★ |
| C：并行互补使用 | ✅ 立即可用 | 0 | ★★★★★ |

### 主要结论

1. **现阶段不推荐紧耦合集成**。sandvik 的 JNI 实现存在多处阻塞性缺陷（JavaVM null、RegisterNatives stub、数组操作缺失），在修复之前接入 unidbg 只会得到大量崩溃。

2. **最短路径：并行互补**。sandvik 做 DEX Java 逻辑分析，unidbg 做 ARM native 分析，人工关联两侧结果。这是当前最实用的工作流。

3. **长期价值：方案 A 值得投入**。修复 P0/P1 JNI 缺口（约 2-3 周工作量）后，将 sandvik 作为 unidbg 的真实 Java VM 后端，能够消除 AbstractJni 中大量手写 stub，显著提升分析精度，尤其对逻辑复杂的加密/协议分析。

4. **最大障碍不是架构，是完整性**。JNI 接口是标准的（`JNINativeInterface_`），sandvik 已经以此为基础实现了 `NativeInterface` 类。桥接代码并不难写，难的是先把 sandvik 自身的 JNI 实现做完整。

### 与 unidbg 功能对位图

```
unidbg 需要的 Java VM 能力              sandvik 当前状态
─────────────────────────────────────────────────────
FindClass(className)              →  ✅ 已实现
NewStringUTF(str)                 →  ❌ 未实现（P0）
GetStringUTFChars(str)            →  ❌ 未实现（P0）
CallObjectMethod(obj, mid, ...)   →  ⚠️ 部分
CallStaticObjectMethod(...)       →  ⚠️ 部分
NewByteArray / GetByteArrayXxx    →  ❌ 未实现（P0）
ExceptionOccurred / Clear         →  ❌ 未实现（P0）
RegisterNatives(methods)          →  ❌ stub（P0）
JNI_OnLoad(JavaVM*, void*)        →  ❌ vm 传 null（P0）
```

满足 unidbg 基本对接需求，需先实现上表中全部 P0 项（估计 10-15 个函数，约 1-2 周）。
