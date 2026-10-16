# Sandvik × unidbg 集成评估（重评）

本文评估的是**把 sandvik 与 unidbg 集成起来**这件事：两边的能力边界、接缝在哪、集成要先补什么、有哪几条路线、值不值得做。

- sandvik：`D:\work\Apps\com.hylztn.helper\sandvik`（C++ 最小 Dalvik VM，GPL-3.0，分支 `my2` @ `6b2d4f5`）
- unidbg：`D:\work\Apps\com.hylztn.helper\unidbg`（Java + Unicorn 的 ARM/ARM64 仿真器，Apache-2.0，`v0.9.9-48-g2ba8156e` = `2ba8156e`）

> 前一版（235 行）写于 upstream `main` 合并之前。其中 **「sandvik JNI 大面积未实现」「JavaVM 传的是 nullptr」** 等条目已被本次实测推翻，第 6 节给出更正表；第 7 节是本机实测证据。

---

## 0 结论（先看这里）

1. **两边是真互补，不是二选一。** sandvik 只在宿主架构上跑 dex（Java 世界）；unidbg 只在模拟器里跑 ARM native（native 世界），它那套 "JavaVM/JNIEnv" 是**门面**，背后没有 Java 运行时。
2. **两条腿在本机都站得住**（集成的地基是实的）：sandvik 9 套件 88 用例全绿；unidbg 打两行补丁即编成，能加载我们目标 APK 的 arm64 壳 .so。
3. **集成的瓶颈不是 JNI 函数数量，而是对象句柄的双向映射与线程模型**（§3.1、§4）。谁把 `jobject`/`JNIEnv*` 当成"自己人"，决定了桥必须放在哪一层。
4. **建议**：当前项目走**路线 A（并行互补，零改动）**；unidbg 只做窄用法——按导出符号调用壳 .so 的**单个函数**（不要碰 `JNI_OnLoad`，实测 90 s 不返回，见 §7）。**只有出现"native 必须回调 Java"的刚需时**才启动路线 B，并按 §8 的最小切片做。

---

## 1 两边各是什么（本机实测锚点）

| 维度 | sandvik | unidbg |
| --- | --- | --- |
| 本质 | C++ 写的 Dalvik VM：dex 解释器、类加载、对象/GC、线程 | Java 写的 ARM/ARM64 仿真器（Unicorn 后端）+ ELF 加载器 + syscall 仿真 + hook 框架 |
| 能跑什么 | dex/jar 里的 Java 代码（**宿主架构**的原生库也能 dlopen，见下） | arm32/arm64 的 `.so`（ARM 指令），Java 侧只有对象模型 |
| 不能跑什么 | **任何非宿主架构的 .so**：`src/system/sharedlibrary.cpp:82/84` 用 `dlopen(...)`、`:103` 用 `dlsym(...)` ⇒ x86_64 上无法加载 arm64-v8a | **任何 dex 字节码**：`dvm/` 包里没有解释器，`DalvikVM*` 只是把 JNI 表注册成模拟化的 `svc` 处理 |
| "JavaVM" 是什么 | 真货：`SandvikVM : public JavaVM_`（`src/jni.hpp:30`），`NativeInterface` 构造时实例化（`src/jni.cpp:158`），`getJavaVm()` 返回它（`src/jni.cpp:404`） | 门面：`dvm/VM.java` 的 ~30 个方法 + `AbstractJni`（~50 KB，约 90 个可覆写回调），没人填就是 stub |
| JNI 方向 | **Java → native**（它托管 Java，去调 native 实现） | **native → Java**（它托管 native，JNI 调用落到 Java 门面） |
| JNI C API 实现面 | `src/jni.cpp`（2,566 行）里 233 个 `NativeInterface::` 方法定义，22 处仍是 `not implemented` | 全部走它自己的表 + 用户回调，不实现真实语义 |
| 本机构建 | clang 22.1.8（llvm-mingw）+ CMake 4.4.4 + Ninja + JDK 25 → 9 套件 88 用例全过（`ctest` 100% passed out of 9，约 18 s） | JDK≥9 下需 2 行 `import` 补丁（§7.1），之后 `BUILD SUCCESS`（20 s）；运行需 JNA + `unicorn`/`capstone`/`keystone` 原生库 |
| 许可 | GPL-3.0 | Apache-2.0 |

---

## 2 为什么互补：JNI 的两个方向

```
        ┌──────────────── sandvik ────────────────┐        ┌─────────────── unidbg ───────────────┐
Java 代码 │ dex 解释器 / 类加载 / GC / 线程          │        │  (Java 侧只是对象模型 + 回调门面)      │
   │      │     │                                   │        │        ▲                              │
   │      │     └─ invoke native ──► dlopen/dlsym ──┼── ✗ ──►│        │ JNI 表（模拟 svc）             │
   │      └─────────────────────────────────────────┘        │        │                              │
   │                                                          │  ARM 指令仿真（Unicorn）              │
   └─ 宿主架构 .so（x86_64/arm64 之一）                        │  可加载 arm64-v8a .so ✅              │
                                                              └───────────────────────────────────────┘
```

- sandvik 缺的正是 unidbg 有的：**ARM 执行**（跨架构 `.so`）。
- unidbg 缺的正是 sandvik 有的：**真 Java 语义**（dex 解释、对象、异常、线程）。
- 于是"集成"在概念上就是把上图中间那个 ✗ 变成一条通路：让 emulated native 的 JNI 调用落到**真的** Java 运行时，或反过来让 sandvik 的 native 调用走进模拟器。

---

## 3 接缝在哪：三种具体形态

### 3.1 unidbg 的 JNI 表接到 sandvik（推荐的正统形态）

- **入口**：`dvm/VM.java` 的接口（`resolveClass` / `findClass` / `getObject` / `setJni` / `loadLibrary` / `callJNI_OnLoad` / `getPackageName` / `openAsset` / `addLocalObject` / `addGlobalObject` / `throwException` / `getEmulator` / `getSignatures` …）与 `DvmClass`/`DvmObject`/`DvmMethod`/`DvmField` 对象模型。
- **做法**：写一个 `VM` 实现（或 `AbstractJni` 子类），把每个 JNI 回调转成对 sandvik 的调用：`resolveClass("com/x/Y")` → sandvik 的类加载；`getObject(...)` → sandvik 对象；emulated native 调 `JNIEnv->CallObjectMethod` → 进 sandvik 的解释器。
- **硬约束（真正的工作量）**：
  1. **句柄映射**：unidbg 侧的 Java 对象是 ARM 虚拟地址/不透明句柄（`jobject` = long，由它的解析器兜底），sandvik 侧是自己的对象模型。需要双向句柄表 + 生命周期规则（local/global/weak ref、谁持有 root、GC 何时可见）。这是最大的一块。
  2. **环境指针的归属**：emulated native 手里的 `JNIEnv*` 必须指向一张"能被两边解释"的表。要么把 sandvik 的 env 表交给模拟器（sandvik 的 env 是 `src/jni.cpp:378` 起自产填表的），要么在两边各留一层代理、把调用转运过去。
  3. **线程**：unidbg 一个 `VM` 实例内的模拟执行是单线程推进的，sandvik 有真宿主线程与自己的 Java 线程模型 → `AttachCurrentThread`/`DetachCurrentThread` 需要桥接规则。

### 3.2 sandvik 的 `SharedLibrary` 接到 unidbg（听起来漂亮，实为 3.1 的子集）

- **入口**：`src/system/sharedlibrary.cpp`（`dlopen`/`dlsym`）与 `src/native/java/...` 里"按 native 方法名找符号再直接调用"的路径。
- **问题**：sandvik 期望拿到一个**宿主可调用的函数指针**，而 unidbg 只能"模拟执行地址 A"。指针语义无法伪造 ⇒ 这条路只能改成：sandvik 遇到 native 方法时，改为调用"仿真器 + 函数地址 + 参数"的 API，并把 `JNIEnv*` 换成一个双方共享的对象。
- **结论**：做出来就是 3.1 换了外壳，**不构成独立路线**。

### 3.3 谁住在谁的进程里

| 形态 | 说明 | 代价 |
| --- | --- | --- |
| JVM 内嵌 native（JNI Invocation API） | sandvik 编成动态库，由 JVM `System.load` 载入，unidbg 在 Java 侧调用它 | 需要 sandvik 新增"外接 JVM 客户端"模式（现在它只做 env 的**提供方**，没有做**调用方**的映射层） |
| 进程外（socket/共享内存） | sandvik 暴露小型 C ABI 服务，unidbg 经 JNA 调用 | 解耦、许可清楚，但每次调用都要序列化句柄，慢且要设计协议 |
| 同进程 C++ 里造 JVM（`JNI_CreateJavaVM`） | 反过来把 JVM 嵌进 C++ 宿主 | 多一个 JVM 生命周期要管，收益不明 |

---

## 4 真实缺口（按"集成必须先补"排序，已按本次实测更正）

| # | 缺口 | 位置 | 现状 | 对集成的影响 |
| --- | --- | --- | --- | --- |
| 1 | `RegisterNatives` 不存映射 | `src/jni.cpp:2638` | 只 `logger.fdebug` 后 `return JNI_OK` | **致命**：真实 .so 普遍靠它注册方法，不存映射就会报 `Native method X is not available!` |
| 2 | 22 处 `not implemented` | `src/jni.cpp`（grep 计数） | 高频缺：`Get/ReleasePrimitiveArrayCritical`、`NewDirectByteBuffer`/`GetDirectBufferAddress`/`GetDirectBufferCapacity`、`PushLocalFrame`/`PopLocalFrame`/`EnsureLocalCapacity`、`DefineClass`、`AllocObject`、`GetObjectRefType`、`Get/ReleaseStringCritical`、`From/ToReflectedMethod`/`ToReflectedField`、`GetSuperclass`、`IsAssignableFrom`、`UnregisterNatives`、`FatalError` | 集成后调用面变大，这些会立刻被 emulated native 撞上 |
| 3 | 没有 ARM 执行 | `src/system/sharedlibrary.cpp:82/84/103` | 宿主 `dlopen`/`dlsym` | 决定"ARM 侧只能由 unidbg 承担"，架构上排除 3.2 独立成立 |
| 4 | 对象句柄双向映射 | 两侧 | 无 | 集成的主体工作量（§3.1） |
| 5 | 线程 attach/detach | 两侧 | 无 | 第二块工作量 |
| 6 | sandvik 当"外接 JVM 客户端" | `src/jni.cpp` | 只做 env 提供方 | 若要 3.3 的第一种形态，需要新映射层 |

**本次已修复（不再算缺口）**：
- `JNI_OnLoad` 不再收到 `nullptr`：`src/vm.cpp:120-122` 现为
  `auto jniOnLoad = reinterpret_cast<void (*)(JavaVM*, void*)>(JNI_onLoad);` … `jniOnLoad(static_cast<JavaVM*>(getJNIEnv()->getJavaVm()), nullptr);`
  （证据见 §7.3。此改动与探针**尚未提交**：`git status` = `M src/vm.cpp`、`M tests/java/jni/src/test_helloworld.c`。）
- 旧版列为"未实现"的常用函数实际都在：`NewStringUTF` `jni.cpp:2117`、`GetStringUTFChars` `:2132`、`ReleaseStringUTFChars` `:2150`、`GetArrayLength` `:2162`、`NewByteArray` `:2212`、`GetByteArrayElements` `:2280`、`SetByteArrayRegion` `:2538`、`ExceptionOccurred` `:483`、`MonitorEnter` `:2656`。

---

## 5 许可与分发

- sandvik **GPL-3.0**、unidbg **Apache-2.0**。Apache-2.0 与 GPL-3.0 兼容，但**合并成一个发行物**时整体按 GPL-3.0 走（unidbg 侧要保留其 NOTICE/许可声明）。
- **保持两个独立进程**（路线 A 或 3.3 的进程外形态）可以完全回避这个议题。

---

## 6 旧版结论更正表

| 旧版（235 行）说法 | 现状（本次实测） |
| --- | --- |
| "sandvik 的 JNI 大面积未实现：`NewStringUTF`/`GetStringUTFChars`/`NewByteArray`/`GetByteArrayElements`/`SetByteArrayRegion`/`Throw`/`ThrowNew`/`ExceptionOccurred`/`MonitorEnter` 缺失 → 集成必崩" | **已过期**：这些全在（行号见 §4 末）。真实缺口是 22 处 `not implemented` + `RegisterNatives` 不存映射 |
| "JavaVM 传的是 `nullptr`（`src/vm.cpp:138 on_load(nullptr, nullptr)`）" | **已修**：`src/vm.cpp:122` 传 `getJNIEnv()->getJavaVm()`；探针实测从 `JavaVM is NULL` 变为 `JavaVM=… GetEnv rc=0 env=…` |
| "`AbstractJni` 是 unidbg 的 Java VM，替换它即可" | **更准确的表述**：`AbstractJni` 只是回调门面；真正的接缝是 `dvm/VM.java` 的接口 + `Dvm*` 对象模型，"替换"等于从零实现一个 Java 运行时后端 |
| 未提及 | **unidbg 没有 dex 解释器**（`dvm/` 内无解释循环）——这既解释了为什么必须集成，也解释了为什么 sandvik 有不可替代的价值 |
| 未提及 | **unidbg 在本机需要 2 行补丁才能在 JDK≥9 下构建**；运行期单库加载约 1.5 s；不重定向 stdout 会写出 GB 级日志（§7.2） |

---

## 7 本机实测证据（附录 A）

### 7.1 unidbg 构建（Windows / JDK 17 或 25）

```
cd D:\work\Apps\com.hylztn.helper\unidbg
.\mvnw.cmd -B -DskipTests -pl unidbg-android -am package
→ BUILD SUCCESS (20 s)
   unidbg-api\target\unidbg-api-0.9.10-SNAPSHOT.jar        432,569 B
   unidbg-android\target\unidbg-android-0.9.10-SNAPSHOT.jar  6,155,141 B
```
- 必需补丁（JDK≥9 下 `import com.github.unidbg.*;` 与裸用 `Module` 会与 `java.lang.Module` 歧义）：
  - `unidbg-api\src\main\java\com\github\unidbg\arm\AbstractARMDebugger.java`
  - `unidbg-android\src\main\java\com\github\unidbg\linux\AndroidElfLoader.java`
  各加一行 `import com.github.unidbg.Module;`（单类型导入优先于按需导入）。
- 本机 JDK 只有 11/17/21/25（无 JDK 8）；`mvnw install` 会因 `maven-gpg-plugin` 无 gpg 失败，用 `-Dgpg.skip=true`。

### 7.2 加载 arm64 库（unidbg 单独跑）

| 目标 | 结果 |
| --- | --- |
| unidbg 自带测试库 `libjnidispatch.so`（arm64-v8a） | base=`0x12000000` size=`0x30000`，加载 1.5 s；`callJNI_OnLoad` = true，随后 `JNI_OnLoad` 抛 `java.lang.IllegalStateException: Illegal JNI version: 0xffffffff` |
| 本项目壳库 `D:\work\Apps\com.hylztn.helper\unpack\shell_lib91d2fe.so`（arm64，615,904 B） | base=`0x12000000` size=`0xa4000`；`callJNI_OnLoad` = true，但 **`JNI_OnLoad` 在 90 s 内没有返回**（看门狗 `Runtime.halt(3)`）；期间它加载了 libc++/libc 与一个解密出来的 `lib3fa5e15702635193.so` 并跑了 init array |

**读法**：unidbg 能把它"搬进模拟器并跑起来"，但它的 JNI 门面不足以支撑一个真实库的 `JNI_OnLoad`——这既是**集成必要性的最强证据**，也标出了集成最硬的那一块（env 语义）。

### 7.3 sandvik 侧 JNI_OnLoad 探针（真 JavaVM）

探针在 `tests\java\jni\src\test_helloworld.c:17-25`：

```
before（src/vm.cpp 传 nullptr）:  [testjni] JNI_OnLoad: JavaVM is NULL
after （src/vm.cpp:122 传 getJNIEnv()->getJavaVm()）:
        [testjni] JNI_OnLoad: JavaVM=0000023411AE2028 GetEnv rc=0 env=0000023411AD8910
```

⇒ sandvik 侧的 JNI **Invocation API** 是可用真货（`GetEnv` 返回 0 且给出非空 env），这正好补上 unidbg 侧最缺的那一环。

### 7.4 成本与陷阱

- 单次 unidbg 运行开销：JVM + JNA + unicorn 启动约 1 s，加载一个 arm64 库约 1.5 s。
- **必须把 stdout 置空或限量**：unidbg 会记录每一次文件访问；更进一步，一旦它的 `BaseVM.checkVersion()` 判定 JNI 版本非法（`unidbg-android\...\dvm\BaseVM.java:234-245`），就会 `emulator.attach().debug(...)` 挂上调试器，此后**每一次被 hook 的 native 调用都打印寄存器**——实测 120 s 写出 **943 MB** 日志（约 1.1 万次/秒）。诊断时务必给日志设上限。
- 判超时要在你自己的**看门狗线程**里做（`Runtime.halt(3)`），不要指望 API 自带超时。

### 7.5 "能不能调用一个完整的 JNI 解密函数"——实测（答案：今天不行）

实验 `unidbg_sandvik_eval\EvalJni.java`（给 unidbg **真实 APK**，而不是空 VM）：

| 阶段 | 结果 |
| --- | --- |
| `createDalvikVM(辉韵一键翻译_1.2.0.apk)` | ✅ `pkg=com.hylztn.helper versionName=1.2.0 versionCode=120`，`manifestXml=60658 chars` |
| `loadLibrary(shell_lib91d2fe.so)` | ✅ `base=0x12000000 size=0xa4000` |
| `callJNI_OnLoad(emulator)` | ⚠️ 进入后**永不返回**：**自引用重入**：`lib3fa5e15702635193.so` 就是这个模块自己的 SONAME（见 §7.6），壳在自己的 `JNI_OnLoad` 里又触发了同一个模块的 `JNI_OnLoad`，unidbg 的 Java 侧 `callJNI_OnLoad` 随之**以约 1.1 万次/秒**重复调用它（参数恒为 `(JavaVM*, null)`），每层都被 `BaseVM.checkVersion` 拒绝 |
| 解密后 ShellConfig 是否出现 | ❌ 120 s 后扫描全部内存映射，`app_sign_sha256` / `app_name` / `junk_cls_name` **0 命中** |

**读法**：unidbg 的 JNI 门面能把库加载起来、能进入 `JNI_OnLoad`，但一个**真实库的 `JNI_OnLoad` 内部会做一整套 JNI 工作**（`FindClass`/`RegisterNatives`/`GetStringUTFChars`…）；只要其中任何一步在门面上拿不到真实语义，库就返回 `JNI_ERR`（`0xffffffff`，与 unidbg 自带测试库 `libjnidispatch.so` 的失败签名一致），而壳的 wrapper 把这个返回值当成"装载失败"，于是**死循环重试**。

**对照（同一件事在 sandvik 上）**：`tests\java\jni\src\test_helloworld.c` 的探针显示 sandvik 的 `JNI_OnLoad` 拿到的是真 env（`GetEnv rc=0 env=0x…`）。**这正是集成能补上的那一环**：unidbg 缺的"真 JNI 语义"，sandvik 有。

**要让这条链路走通，三条具体做法**：

1. **喂饱门面**：给日志设上限后重跑，按 unidbg 打出的 JNI 调用序列逐条补 `AbstractJni`/`DalvikVM` 的回调，直到内层 `JNI_OnLoad` 返回 `0x00010006`（判据：重试循环停止、内存里出现 ShellConfig JSON）。工作量可迭代、可度量，是路线 B 的缩影。
2. **钉住返回值**：hook 这个模块自己的 `JNI_OnLoad`（`base+0x888e4`），让 wrapper 看到"装载成功"继续往下走。**已实测（§7.6）：入口钉住 = 整个函数体被跳过，解密结果拿不到；只有"第 1 次放行、后续钉住"才有意义，但壳仍跑不完。**
3. **绕开 JNI**：不调 `JNI_OnLoad`，直接按符号/地址调用它内部的加解密例程（标量参数），即 §10 的"窄桥"用法。

### 7.6 钉住 `JNI_OnLoad` 的两次跟踪实验（更正 §7.5 的一处误读）

**更正**：`loadLibrary(shell_lib91d2fe.so)` 之后 unidbg 报出的模块名是 `lib3fa5e15702635193.so` —— 那是**这个文件自己的 SONAME**（设备侧 dpt 用的是随机文件名，二者不同）。所以并不存在"壳解密出来的内层库"，从头到尾只有**一个**模块；它的 `JNI_OnLoad` 就在 `base+0x888e4`（.so 的 `.dynsym` 里 `JNI_OnLoad@0x888e4`）。

实验 `unidbg_sandvik_eval\EvalJni2.java` 在这个模块的 `JNI_OnLoad` 入口装 `CodeHook`，两种钉法：

| 钉法 | 结果 |
| --- | --- |
| 每次进入都钉（`x0=0x00010006`，`PC=LR`） | `callJNI_OnLoad` **立即正常返回**；钩子命中 **1 次** = 函数体一秒都没执行；内存里 ShellConfig 明文键 **0 命中**（`[7] = 0`）。⇒ 入口钉住等于"跳过整个函数"，拿不到任何解密结果 |
| 第 1 次放行、第 ≥2 次钉住 | 壳的 `JNI_OnLoad` **真的执行了一次**（命中 1 次即首次进入），45 s 后 `callJNI_OnLoad` 线程**仍存活**（自引用重入仍在继续，只是这次日志级别不是 TRACE，只有 11 次 `debugger break`）；内存扫描 ShellConfig 明文键仍 **0 命中** |

**机制（源码）**——`unidbg-android\...\dvm\BaseVM.java`：

```java
final void checkVersion(int version) {
    if (version != JNI_VERSION_1_1 && version != JNI_VERSION_1_2 &&
        version != JNI_VERSION_1_4 && version != JNI_VERSION_1_6 &&
        version != JNI_VERSION_1_8) {
        if (log.isTraceEnabled()) {
            emulator.attach().debug("Illegal JNI version: 0x" + Integer.toHexString(version));
        }
        throw new IllegalStateException("Illegal JNI version: 0x" + Integer.toHexString(version));
    }
}
```

即：库返回的版本号只要不是那五个之一，unidbg 就丢弃该次结果并抛异常；TRACE 级别下每次还会 `attach()` 打印整套寄存器（这就是 943 MB 日志的来源）。壳的 `JNI_OnLoad` 之所以返回 `0xffffffff`，是因为它内部那一整套 JNI 工作（反射取包名、签名校验、`RegisterNatives`…）在门面上拿不到真实语义。

**结论（对齐用户问题"能调用一个完整的 JNI 解密函数吗"）**：

- **unidbg 侧：不能。** 能加载、能进入真实 `JNI_OnLoad`、能把它跑起来，但**跑不完**——真库要的是真 JNI 语义，门面给不了；钉返回值只能骗过 wrapper 的版本校验，代价是整个函数体被跳过，解密结果依然拿不到。
- **sandvik 侧：能。** `JNI_OnLoad` 拿到真 `JavaVM`，`GetEnv` 返回 `0` 与真 env（§7.3），这才叫"完整的 JNI 调用"；但 sandvik 加载不了 arm64 的壳库（宿主机 `dlopen`）。
- **两句话合起来就是本评估的核心**：完整 JNI 语义在 sandvik，ARM64 执行在 unidbg，**缺的正是中间那条桥**（路线 B）。
- **但对本项目的实际影响 = 0**：壳的"解密函数"我们早在离线路线上做完了（16 字节 encKey → 整块 XOR 出 store 明文 → 159,016 条记录 / 185,784 个方法体，junk 从 30.3% 归零），noshell 产物已在真机跑通。**不要为了"让壳自己解密"去接桥**。

---

## 8 路线对比与建议

| 路线 | 内容 | 增量工作 | 风险 | 建议 |
| --- | --- | --- | --- | --- |
| **A. 并行互补（推荐）** | sandvik 管 dex/Java，unidbg 管 arm64 .so；两边都用"按需调用"，不建桥 | 0 | 无 | **现在就这么用**：本机两条腿都已验证 |
| **B. sandvik 当 unidbg 的 Java 后端** | 实现 `dvm/VM.java` 接口 + `Dvm*` 对象模型，把 JNI 回调转进 sandvik | VM 接口（~30 方法）+ 句柄映射 + 线程桥接 + 补 §4 缺口 1/2 | 对象/线程语义不一致，调试成本高 | **只在出现"native 必须回调 Java"的刚需时做**，且先做最小切片：只实现 `resolveClass`/`findClass`/`getObject`/`setJni`/`throwException` 等高频项 + 修 `RegisterNatives` |
| **C. unidbg 当 sandvik 的 ARM 后端** | 把 `SharedLibrary` 的加载/调用改为走仿真器 | 同上（是 B 的子集） | 调用约定/函数指针语义无法保持 | 不单独立项 |
| **D. 只借 unidbg 的部件** | 用它的 ELF 解析 / Unicorn / hook 框架自建工具，不碰它的 Java VM 模型 | 中等（取决于复用范围） | 与上游演进脱节 | 需要"离线看 ARM 代码"的专用工具时可选 |

**对当前项目（dpt-shell 残留字符串）**：残留的 228 处站点是 **dex 层**问题（字符串解密器调用点），sandvik/真机能解决，unidbg 帮不上；unidbg 的可复用点是**离线核对壳 .so 的算法与常量**——务必**按导出符号调用单个函数**，不要走 `JNI_OnLoad`（§7.2 实测挂住）。

---

## 9 一页速览

- 互补是真的：**sandvik = Java 世界，unidbg = ARM 世界**，谁都不能单独跑起一个真实 Android 应用。
- 接缝是 JNI 的两个方向；正统形态是 **B**（unidbg 的 JNI 表接到 sandvik 的真 env）。
- 集成的钱花在**句柄映射 + 线程模型**，不是 JNI 函数数量；真正会立刻绊倒你的两个具体缺口是 **`RegisterNatives` 不存映射** 与 **22 处 `not implemented`**。
- 现状：**A 可用且已在本机验证**；B 是"有需要再做"的项目级投入；C 不成立；D 按需。

---

## 10 评估者意见（直说）

**结论：现在不要做 B/C 那种"接 runtime"的集成；要做的是"薄桥 + 各自 CLI"。**

理由（按重要性排序）：

1. **需求还没出现。** 手上的活（残留 228 处字符串站点）是纯 dex 层问题；壳的 native 侧我们已经在静态侧拿到了需要的东西（16 字节 RC4 key、`.bitcode` 解密）。B 解决的是"arm64 native 必须回调 Java"这类问题，目前没有这样的任务。
2. **成本集中在最难切分的地方。** 集成的钱花在**对象句柄双向映射 + 线程 attach/detach**上，这里没有便宜的第一刀——任何"先做一半"的版本都会在第一个真实回调（`jobject` 参数）上失败。
3. **"离线跑真实 arm64 应用"这条需求，当前有更便宜、更保真的替代**：设备/模拟器 + FART（我们已经在用，整条 APK 还原链路就是靠它跑通的）。用 B 去替代它，是拿 6–10 周工程量换一个保真度更低的运行时。
4. **unidbg 的价值是真实的，但是"窄"的**：把 arm64 代码搬进模拟器执行——**按导出符号调单个函数**（喂标量/字节数组，不碰 Java 对象）。这是分析壳 `.so` 的利器，且本机实测可用（单库加载约 1.5 s）。

我建议的先后（每一步都能独立交付）：

1. **固化两个 CLI**：sandvik 侧已有 `sandvik.exe`；给 unidbg 写一个 `unidbg-cli`（`--lib X.so --symbol foo --arg i32:5 --arg bytes:<b64>`，输出 JSON；内部必须把 stdout 置空 + 自带看门狗超时——这两个坑见 §7.4）。两边都能独立干活，价值立刻兑现。
2. **修 sandvik 的 `RegisterNatives`**（`src/jni.cpp:2638`）。它跟 unidbg 无关，但**是 sandvik 当"真 Java 运行时"的硬前提**：不存映射，任何真实 `.so` 一注册 native 方法就会失败。投入小、收益明确。
3. **只在出现刚需时启动 B 的最小切片**：实现 unidbg `VM` 的 5–8 个高频方法（`resolveClass` / `findClass` / `getObject` / `setJni` / `throwException` / `addLocalObject` / `addGlobalObject`）+ 句柄表 v1（只支持 Object/String/数组/Class，暂不细化 weak/global 生命周期）+ 单线程模型。**验收标准 = 一个 arm64 native 方法成功回调一个 Java 方法并拿到正确返回值。**

会让我改变意见的触发条件（具体、可观测）：

- 在 unidbg 里 `callSymbol` 一个函数，它卡住或返回错误，而失败原因是**"它要回调 Java"**（而不是缺 syscall / 缺库 / 缺文件）；
- 出现"必须在离线环境跑壳逻辑、且结果依赖 Java 侧状态"的任务（例如 native 侧要验证签名、读 Java 字段才继续）。

我不会做的两件事：

- 不"顺手"把 unidbg 的 Java 门面往全量实现推——那是 B 的工程量，做了也验收不了；
- 不假设 unidbg 是万能 ARM 后端：实测它对真实库很脆（`Illegal JNI version: 0xffffffff`；壳库 `JNI_OnLoad` 90 s 不返回）。任何以它为基础的方案，先按实测降级预期。

许可层面：独立进程/窄桥可以完全回避 GPL-3.0 × Apache-2.0 的合并问题；一旦做成同一个发行物，整体按 GPL-3.0 走（对开源项目无碍，但要知道）。

---

## 11 项目级结论：谁融入谁，还是新项目

**问题**：这不是"给某个 APK 用的临时评估"，而要决定三者关系——sandvik 融入 unidbg、unidbg 融入 sandvik、还是新起一个项目。

**结论（一句话）**：**新项目做中间层，但进程宿主是 JVM（unidbg 那侧）；sandvik 以 JNI 库的形态被调用。** 代码上不存在"谁吞谁"——两边都保持独立仓库与各自测试，被改造的只有两处扩展点，且都以**上游补丁**方式提交，不硬 fork。

### 11.1 为什么宿主必须是 JVM

- unidbg 是 Java：emulator 只能在 JVM 里被驱动。把 unidbg 塞进 C++ 宿主，就得在 sandvik 进程里内嵌一个 JVM（`JNI_CreateJavaVM`），多出第二个 VM 的生命周期、信号处理、线程所有权问题，收益只是"保持单进程 C++"。
- sandvik 是 C++，天然可以编成 DLL 被 JVM 通过标准 JNI 调用；Java→native 是最常见、工具链最成熟的方向。
- 两个方向都免费：unidbg（Java）→ sandvik（C++）用 JNI；sandvik（C++）→ unidbg（Java）也用 JNI（宿主本来就是 JVM，env 是真的）。
- 反过来（sandvik 作宿主）只有"进程里只有一个 VM 实现"这个形式收益，技术上不划算：sandvik 没有嵌入 JVM 的线程/信号模型，unidbg 也没有"C 宿主"形态的 API。

### 11.2 新项目里放什么、不放什么

放：

- `SandvikBridge`（Java）：实现 unidbg 的 `dvm/VM.java` + `DvmClass/DvmObject/DvmMethod` 对象模型，把 `resolveClass` / `findClass` / `getObject` / `setJni` / `throwException` / `addLocalObject` / `addGlobalObject` 转到 sandvik；维护**句柄表**（unidbg 的 `DvmObject` ↔ sandvik 的 `ObjectRef`）。
- `NativeExecutor`（sandvik 侧新增的扩展点 + 默认实现）：默认 = 宿主 `dlopen`（今天的行为，`src/system/sharedlibrary.cpp:82/84`）；本项目 = 回调 JVM → unidbg 在 unicorn 里执行，参数与缓冲区双向拷贝。
- 构建与测试：把 sandvik 编成 DLL（JNI 入口：`createVM` / `loadDex` / `resolveClass` / `callStaticMethod`）；CI 跑"一个 arm64 native 方法回调一个 Java 方法"的验收用例。

不放：

- 不把 sandvik 重写成 Java，也不把 unidbg 的 JNI 门面推到全量实现。
- 不新建第三个 VM：新项目只是**适配层 + 测试**，语义核心仍是 sandvik，执行核心仍是 unidbg。

### 11.3 三个里程碑与止损线

| 里程碑 | 内容 | 验收 |
| --- | --- | --- |
| **M1 sandvik 库化** | 编 DLL + JNI 入口 + `NativeExecutor` 接口（默认行为不变） | 从 Java 调到一个 dex 静态方法并拿到返回值 |
| **M2 桥与句柄表** | unidbg 的 VM 接口 over sandvik；jobject/Class/数组句柄双向登记为强引用 | unidbg 侧 `resolveClass` 拿到 sandvik 类；Java 对象往返一次不丢身份 |
| **M3 native 执行器** | sandvik 遇 native → 回调 unidbg：ELF 加载 + 内存/参数拷贝 + 返回；native 内再调 JNI 回到 sandvik 真 env | 一个 arm64 native 方法成功回调一个 Java 方法并拿到结果 |

止损线：M1 若两周内做不到"从 Java 调一个 dex 静态方法"，说明宿主方向或 sandvik 库化前提不成立 → 回退到并行互补（§8 路线 A）。M3 若卡在 jarray 双向拷贝 / GC 根管理，就把范围缩到"标量参数 + 单线程"，不要硬撑成通用运行时。

### 11.4 与 §10 的关系

§10 是**当下项目**（dpt-shell 还原）的意见：那时不要接桥，因为需求在 dex 层。§11 是**产品级**意见：一旦目标改成"离线跑真实 APK（dex + arm64 .so）"，桥就是主线，宿主选 JVM、代码归新项目。两者不矛盾——是"什么时候开始"的问题，不是"往哪走"的问题。

### 11.5 许可

sandvik GPL-3.0 × unidbg Apache-2.0：**同一个发行物整体按 GPL-3.0**（Apache-2.0 → GPL-3.0 单向兼容）。若新项目只做"外部进程 + 窄桥"（两边不链接），可回避合并问题；做了 M1/M3 的链接与同发行，就按 GPL-3.0 走。
