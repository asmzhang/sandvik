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
5. **更新（§14）：M3 已跑通。** "native 必须回调 Java" 这条链在本机已经实测通过——unidbg 里跑 ARM64 native 方法，native 回调 Java 静态方法，Java 由 sandvik 解释执行，返回值经 native 帧回到 unidbg 侧（`run_eval.ps1` 一条命令可复现）。§0.4 的条件句因此从"设想"变成"已验证可做"；但**当前 APK 项目的建议不变**（仍走路线 A，见 §10）。

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

## 12 进展：M1 与 M2 已跑通（实测，非设想）

写 §11 时 M1/M2 还是方案；本节记录它们**已经实现并通过验收**的事实，供后续 M3 判断起点。

### 12.1 M1 sandvik 库化（已完成）

- 产物：`sandvik\build\libsandvik.dll`（3.4 MB）、C API `sandvik\src\api\sandvik_c.h`、JNI 入口 `sandvik\src\api\sandvik_jni.cpp`、Java 侧 `sandvik\tests\bridge\org\sandvik\Native.java`。
- 验收：`org.sandvik.BridgeTest` 从 Java 侧 `loadDex` → 调用 dex 里的静态方法 → 拿回返回值；`ctest` 10/10 通过（含新增 `bridge_jvm`）。

### 12.2 M2 桥与句柄表（已完成）

- sandvik 侧句柄层：`sv_vm_resolve_class` / `sv_vm_new_string` / `sv_vm_call_static_handle` / `sv_vm_object_class_name` / `sv_vm_object_as_string` / `sv_vm_object_is_*` / `sv_vm_object_array_length` / `sv_vm_object_element` / `sv_vm_handle_count` / `sv_vm_release_handle`；句柄表是**根**：对象句柄在 `internRef` 里 `Vm::addRoot`（`sandvik\src\api\sandvik_c.cpp:93`），`sv_vm_release_handle` 里 `removeRoot`（同文件 650）——理由见 §13 的 A1。**返回约定：0 = 成功**（`sv_vm_resolve_class` 例外，返回句柄、0 = 失败）。
- unidbg 侧桥：`unidbg_sandvik_eval\src\com\github\unidbg\linux\android\dvm\{SandvikBridge,SandvikClass,SandvikObject}.java`。`SandvikBridge extends BaseVM`（必须与 `BaseVM` 同包，因为它的构造器 `BaseVM(AndroidEmulator, File)` 是包私有），只实现抽象方法 `loadLibraryData` / `getJavaVM` / `getJNIEnv`，并重写 `createClass` 作为 `resolveClass` 的接入点；`SandvikClass extends DvmClass`、`SandvikObject extends DvmObject`，两者都带一个 sandvik 句柄。
- 验收：`org.sandvik.SandvikBridgeTest`（classpath = unidbg 两个 jar + `sandvik\build\bridge-classes` + 本目录 `out`）四组断言全绿、exit 0：
  - `[1] unidbg resolveClass -> SandvikClass(BridgeHello, handle=1)`（`findClass` 返回同一对象）
  - `[2] newSandvikString -> SandvikObject(java.lang.String, handle=3, value=world), unidbg hash=159259014`
  - `[3] sandvik executed BridgeHello.hello -> "hello, world", add -> 42`
  - `[4] echo round trip -> sandvik handle 3, unidbg hash=2034688500`（对象进 sandvik 再回来，句柄不变）

### 12.3 M2 期间发现的两个真实坑（写进代码注释，避免复发）

1. **sandvik 的 `Array::isClass()` 返回 `true`**（`sandvik\src\array.cpp:76-78`）。若直接用 `isClass()` 判断，数组会被登记成"类句柄"，`objectIsArray` / `objectElement` 立刻失效（症状：`isArray=0 length=-1 isClass=1`）。修法：`is_class = obj->isClass() && !obj->isArray()`，已在 `sandvik\src\api\sandvik_c.cpp` 三处（`classFromHandle`、`marshalReturn`、`sv_vm_object_element`）落实。
2. **`Class::getName()` 是简单名，`getFullname()` 才是点号全名**（`sandvik\src\class.cpp:45/55/57/155/159`）。返回给 unidbg 的 `className` 必须用 `getFullname()`。另：dex 里 `value.getClass().getName()` 在 sandvik 内会报 `invoke-virtual: call method java.lang.String->getName()Ljava/lang/String; not found`，验收 dex 因此不依赖它。

### 12.4 结论与下一步

- §11 的判断（宿主是 JVM、sandvik 以 JNI 库形态被调用、代码归新项目）在 M1/M2 上**已被实测支持**：桥只需要 3 个类，没有改动 sandvik 的执行核心。
- **M3 native 执行器：已跑通（见 §14）**。验收标准（一个 arm64 native 方法成功回调一个 Java 方法并拿到结果）在 `org.sandvik.SandvikNativeTest` 上首跑即过；做法不是"让 sandvik 去执行 native"，而是反过来——**unidbg 自己当 VM，把 Java 侧交给 sandvik**：`vm.setDvmClassFactory(bridge)` + `vm.setJni(new SandvikJni(bridge))`。
- §14.5 列了这次没做的（实例方法 / 构造 / 数组 / `RegisterNatives`）。

## 13 桥的审计（读源码 + A/B 实测；问题已修）

M2 跑通不等于逻辑自洽。本节是对 M2 的审计：先读两边的实现（unidbg 的 `dvm/BaseVM`、`DvmObject`、`DvmClass`；sandvik 的 `gc.cpp`、`vm.cpp`、`object.cpp`），再对每个可疑点做可观测的 A/B 实验。**A1–A6 已修并复验；A7–A10 是设计约束/更正，记录在案。**

### A1 句柄悬空（严重 → 已修，有实测证据）

- **机制**：句柄表 `sv_vm::refs_by_id` 存的是裸 `ObjectRef`。sandvik 的 GC 标记阶段只看三个来源——root set、class loader、线程帧（`sandvik\src\gc.cpp:157-159` → `vm->visitReferences(mark)`；`sandvik\src\vm.cpp:309-322` 只访问 `_roots` / `_classloader` / `_threads`）。也就是说：**只被句柄引用的对象不在任何根上**，一次回收就会把它扫掉，句柄随之变野指针。
- **修法**：句柄即根。`internRef` 对非 Class 句柄 `Vm::addRoot`（`sandvik\src\api\sandvik_c.cpp:93`），`sv_vm_release_handle` 对称 `removeRoot`（同文件 650）；新增 `Vm::addRoot/removeRoot`（`sandvik\src\vm.hpp:168/170`、`vm.cpp:324/334`），`_roots` 在 `visitReferences` 里最先被访问（`vm.cpp:310-317`）。
- **A/B 实测**（同一次回收里对照，`SandvikBridgeTest` 第 [5] 组）：新加诊断入口 `sv_gc_probe_unrooted_string` 造一个"谁都不引用"的 String（走 `Object::make(ClassLoader&, const std::string&)`，`object.cpp:338-344`，**不进 StringPool**，所以 `StringPool::markAll()` 不会误标它）；`gcRequestCollect()` 后：
  - 未持句柄的探针 → 被扫（tracked **469 → 47**，`gcCycles` 0 → 1）；
  - 持句柄的 `world` → `objectIsString(handle) == 1`、`wrapHandle(handle)` 仍返回**同一个** wrapper、静态调用仍返回 `hello, after-gc`；
  - 句柄数不变（5）。
  两边对象由同一条 `Object::make` 路径产生，唯一差别就是有没有 `addRoot` ⇒ 这个修复是**承重**的，不是修饰。
- 新增的 GC 入口（`sandvik\src\api\sandvik_c.h` + `sandvik_jni.cpp` + `tests\bridge\org\sandvik\Native.java`）：`sv_gc_start` / `sv_gc_request_collect` / `sv_gc_cycles` / `sv_gc_tracked_count` / `sv_gc_probe_unrooted_string`。前四个是宿主可观测的旋钮与仪表，最后一个是诊断专用（造一个必然可回收的对象，用来证明"回收真的发生了"）。

### A2 残留字段导致编译失败（中 → 已修）

审计把 `refs_is_class` 表改成 `refs_kind`（三态：对象 / 类 / Class 对象）时，`sandvik_c.cpp:218`（`makeArg`）仍读旧字段 → `error: no member named 'refs_is_class' in 'sv_vm'`。已改为 `refs_kind` + `KIND_CLASS`。教训：改了结构体字段要 `grep` 全仓旧名，别信"我只改了几处"。

### A3 双重包装导致身份分裂（中 → 已修）

- **机制**：unidbg 的对象身份是 **Java 实例身份**（`DvmObject` 没有覆写 `hashCode/equals`，只有 `Object` 的默认实现），而桥原来每次 `wrapHandle` 都 `new`。于是同一个 sandvik 对象可以被包成两个 wrapper、拿到两个不同的 unidbg hash——"对象往返不丢身份"这条 M2 验收标准就只在"调用者自己记住实例"的前提下成立。
- **修法**：`SandvikBridge` 增加规范化表 `objectsByHandle`，`wrapHandle` 命中即返回既有实例；并补 `sandvikHandleOf(int hash)` 反向查询（M3 的 env 需要"从 unidbg hash 找 sandvik 句柄"）。

### A4 参数表长度不齐会静默越界（中 → 已修）

`callStatic(clazz, method, signature, tokens, handles)` 若 `tokens.length != handles.length`，原来会把越界读数交给 sandvik。现在抛 `IllegalArgumentException`；`resolveSandvikClass` 与 `callStatic` 对 `handle == 0` 的类**直接抛错**，而不是把 0 当合法句柄传进去。

### A5 生命周期（低 → 已修）

构造时若某个 runtime jar 加载失败，原实现会漏掉 `Native.destroy`（VM 泄漏）——现在失败即销毁；`close()` 改为幂等（可重入），并清空规范化表。

### A6 M3 的桩不该返回 null（低 → 已修）

`getJavaVM()` / `getJNIEnv()` 原来返回 null，调用方会拿到一个"看起来能用、用起来崩"的指针。现在抛 `UnsupportedOperationException`（`loadLibraryData` 返回 null 是**正确**的：那是 unidbg 对"这个 APK 没有这个库"的约定答复）。

### A7 局部引用不会自动释放句柄（设计约束，未修）

unidbg 的 `deleteLocalRefs()` 无法回调桥去释放 sandvik 句柄（`DvmObject.onDeleteRef()` 是包私有 final）。所以**句柄的生命周期由宿主显式管**：`releaseSandvikHandle(handle)` 或 `close()`。桥的类注释已写明，避免后来者以为"unidbg 会自动帮我释放"。

### A8 GC 不扫 C++ 栈（设计约束，未修）

`sandvik\src\gc.cpp:138-141` 自带 TODO："scan thread's local JNI handles table / global JNI handles" 尚未实现。含义：**桥之外的裸 `ObjectRef` 一律不安全**，跨边界对象必须走句柄（而现在句柄就是根）。这也解释了 A1 为什么必须先修：句柄是这套 GC 下唯一可靠的跨边界引用。

### A9 更正：GC 线程其实早就启动了（信息）

早先（§12 期间）以为"VM 从不启动 GC 线程"。错：`Vm` 构造器就调 `GC::manageVm(this)`（`sandvik\src\vm.cpp:46`），后者在第一个 VM 时 `run()` 起线程（`sandvik\src\gc.cpp:85-87`）；默认 `_limit = 50_000_000`，所以实际回收只由 `requestCollect()` 触发。`sv_gc_start()` 因此是**幂等保证**（保留，但注释已改成事实），不是开关。

### A10 更正：Hash 不会碰撞覆盖（信息）

一度担心两个 sandvik 对象在 unidbg 对象表里哈希相撞。查证：`DvmObject` 未覆写 `hashCode/equals` ⇒ 身份即实例，配 A3 的规范化后不会互相覆盖（验收里 `hash` 与 wrapper 一一对应）。

### 审计后的复验（全部通过）

- sandvik 全量测试：`ctest --timeout 120` → **11/11 通过**（18.8 s，含 `bridge_jvm`、`bridge_objects`）。
- 桥验收 `org.sandvik.SandvikBridgeTest`：**21 项断言全绿**，exit 0，其中第 [5][6] 组是本次审计新增的 GC 断言（回收真的发生 / 根上的对象活下来 / 句柄数不变 / 释放句柄后不打扰幸存者）。

## 14 进展：M3 已跑通 —— ARM64 native 回调 Java，由 sandvik 执行（实测）

**验收标准（§11.3 的 M3）已达成**：unidbg 里跑一个 ARM64 native 方法，该方法通过 JNI 回调 Java 静态方法，Java 由 sandvik 解释执行，返回值经 native 帧回到 unidbg 侧。

### 14.1 接缝：`Hashable.checkJni`（M3 只需要这一处）

- `unidbg-android/src/main/java/com/github/unidbg/linux/android/dvm/Hashable.java:5-11`：`checkJni(BaseVM vm, DvmClass dvmClass)` 返回 `dvmClass.getJni()`（非空时），否则 `vm.jni`；两者都空则抛 `IllegalStateException("Please vm.setJni(jni)")`。
- `DvmMethod.java:54` 起，所有 `callXxxMethod` 都先走 `checkJni(vm, dvmClass)`。
- 所以只要 `vm.setJni(new SandvikJni(bridge))`，unidbg 收到的每个 Java 调用都进我们的实现；`vm.setDvmClassFactory(bridge)` 则让 unidbg 解析出的每个类都是带 sandvik 句柄的 `SandvikClass`。

宿主侧装配只有三行（`SandvikNativeTest.java`）：

```java
VM vm = emulator.createDalvikVM();   // unidbg 自己的 DalvikVM64：自带 JNI 表 + classMap + 引用表
vm.setDvmClassFactory(bridge);       // FindClass -> SandvikClass（带 sandvik 句柄）
vm.setJni(new SandvikJni(bridge));   // Java 调用 -> sandvik 执行
```

### 14.2 一次 `CallStaticObjectMethod` 的完整调用链

1. native `env->FindClass("org/sandvik/Hello")` → `DalvikVM64._FindClass`（`DalvikVM64.java:56-83`）→ `resolveClass` → 工厂 → `SandvikClass`（含 sandvik 句柄）→ 返回其 `hashCode()` 当 jclass。
2. `env->GetStaticMethodID(...)` → `_GetStaticMethodID`（`DalvikVM64.java:1777-1800`）→ `DvmClass.getStaticMethodID(name, args)`。
3. `env->NewStringUTF("world")` → `_NewStringUTF`（`DalvikVM64.java:3036-3057`）→ `StringObject`。
4. `env->CallStaticObjectMethod(clazz, mid, str)` → `_CallStaticObjectMethod`（`DalvikVM64.java:1802-1824`）→ `DvmMethod.callStaticObjectMethod` → **`checkJni` → `SandvikJni`** → sandvik C API（`resolveClass` / `newString` / `callStaticHandle`）→ `SandvikObject`（unidbg 身份 = sandvik 句柄的规范化包装，A3）。
5. 返回值被 `addLocalObject` 收进 unidbg 引用表，native 拿到 jstring；native 原样返回，unidbg 侧再 `vm.getObject(...)` 取回 Java 字符串。

### 14.3 本次新增/改动的文件

| 文件 | 作用 |
| --- | --- |
| `unidbg_sandvik_eval\src\com\github\unidbg\linux\android\dvm\SandvikJni.java` | `extends AbstractJni`；覆写静态调用家族（`callStatic{Void,Boolean,Int,Long,Object}Method` 的 `DvmMethod` 与 `String` 两种签名），把 `DvmMethod.getSignature()` 解析成 sandvik 的类/方法/描述符，按描述符打包参数并校验返回值 |
| `unidbg_sandvik_eval\src\org\sandvik\SandvikNativeTest.java` | M3 验收程序（stdout 置空 + 180 s 看门狗 + 三组断言） |
| `unidbg_sandvik_eval\native\sandvik_bridge.c` | ARM64 测试库源码：`bridgeHello` / `bridgeAdd` / `bridgeEcho`（分别走 `CallStaticObjectMethod` / `CallStaticIntMethod` / 对象往返） |
| `unidbg_sandvik_eval\native\libsandviktest.so` | 上者的 arm64-v8a 产物（5,936 B，NDK clang `-shared -fPIC -O1`） |
| `sandvik\tests\java\bridge\org\sandvik\Hello.java` | 被回调的 Java 类（`hello` / `add` / `echo`） |
| `sandvik\tests\java\bridge\org\sandvik\NativeBridge.java` | native 声明（`bridgeHello` / `bridgeAdd`） |
| `sandvik\tests\java\bridge\classes.dex` | 上述类的 dex（1,616 B；**必须 `javac --release 11`**：JDK 25 产 major 69，d8 8.10 拒绝） |
| `unidbg_sandvik_eval\run_eval.ps1` | 一条命令重建并跑 M2 + M3（`-Only m2|m3`；`-Rebuild` 会重编 dex 与 .so） |

### 14.4 验收输出（首跑即过，exit 0）

```
  [ok] the ARM64 test library is loaded (LinuxModule{base=0x12000000, size=12288, name='libsandviktest.so'})
  [ok] unidbg resolved the native class through sandvik (SandvikClass(org.sandvik.NativeBridge, handle=1))
  [ok] native bridgeHello() -> "hello, world" (executed by sandvik)
[1] CallStaticObjectMethod answered by sandvik -> "hello, world"
  [ok] native bridgeAdd(40, 2) -> 42
[2] CallStaticIntMethod answered by sandvik -> 42
  [ok] native bridgeEcho("ping") -> "ping"
[3] object round trip through native -> "ping"
[ok] M3 native bridge acceptance passed
```

同一轮的 M2 回归：**21 项断言全绿**（`echo` 往返的 unidbg hash 与 `newSandvikString` 一致 = 159259014，A3 的规范化生效）。

### 14.5 这次没做、下次会撞上的

- **实例方法 / 构造 / 数组**：`SandvikJni` 目前只覆写静态调用家族。实例调用需要 sandvik C API 增 `sv_vm_call_handle`（按句柄调实例方法），数组需要 `jarray` 双向拷贝；`AbstractJni` 未覆写的方法仍然抛异常——这是有意的，遇到再补一个，不要预先全实现。
- **`RegisterNatives`**：真实壳的 native 方法走它注册（§10 已记为 sandvik 侧的硬前提，`sandvik\src\jni.cpp:2638` 仍待修）。
- **引用生命周期**：unidbg 的 `deleteLocalRefs()` 无法回调桥去释放句柄（A7），跨边界对象仍由宿主显式 `releaseSandvikHandle` / `close`。
- **性能**：每次 Java 调用都是「unidbg 解释 native 帧 → JNI → sandvik 解释 Java」，两边都是解释器；这条链适合正确性验证与少量回调，不适合当高频热路径。
- **两个真坑**：① 用 `& java ... 2>&1 | Where-Object` 时若 `$ErrorActionPreference = 'Stop'`，java 的 stderr 会被 PowerShell 包成终止性 `NativeCommandError`（脚本假失败）；`run_eval.ps1` 在调用前后临时放宽为 `Continue`。② 用 `&` 调用 `.ps1` 时它内部的 `exit` 会结束调用方 pwsh。

---

## 15 代码已落地：`arsenal/attack/tools/sandvik-bridge`（实测）

§11 说「新项目做中间层、JVM 当宿主」——这一节记录它现在**住在哪、怎么跑、怎么算绿**，而不是又一个设想。

### 15.1 形态

| 组成 | 位置（伞仓 `D:\work\vmp\arsenal`） | pin |
| --- | --- | --- |
| sandvik 引擎 | `attack/tools/sandvik-bridge/third_party/sandvik` | submodule，分支 `my2`（M1/M2/M3 三笔都在远端） |
| unidbg 引擎 | `attack/tools/sandvik-bridge/third_party/unidbg` | submodule，分支 `arsenal-jdk-modules`（补两个显式 `import com.github.unidbg.Module;`，JDK 9+ 模块解析需要） |
| 桥类 | `<tool>/java/com/github/unidbg/linux/android/dvm/{SandvikBridge,SandvikClass,SandvikJni,SandvikObject}.java` | 伞仓内跟踪；**必须与 `BaseVM` 同包**（构造器包私有） |
| 验收程序 | `<tool>/java/org/sandvik/{SandvikBridgeTest,SandvikNativeTest}.java` | M2 / M3 的断言程序 |
| 原生侧 | `<tool>/native/sandvik_bridge.c` | M3 的 arm64 库，用 NDK 现场编 |
| 入口 | `python -m sandvik_bridge {info,plan,verify,unidbg,sandvik}` | 两个引擎都构建到 `<tool>/build/`，**不写进 checkout** |

两个引擎各自保留 history 与 origin（sandvik = asmzhang 的 fork，GPL-3.0；unidbg = Apache-2.0）⇒ 合并分发整体 GPL-3.0（§11.5 的落地版）。

### 15.2 怎么跑（在伞仓根）

```
cd D:\work\vmp\arsenal
python -m arsenal attack sandvik-bridge info
python -m arsenal attack sandvik-bridge plan --which both
python -m arsenal attack sandvik-bridge verify --which both --ndk <NDK 目录> --report build\verify.json
```

`verify` 依次跑 `bridge:javac` → `native:ndk` → `accept:m2` → `accept:m3`；**绿 = 末两条的 marker**（`[ok] M2 bridge acceptance passed` / `[ok] M3 native bridge acceptance passed`），不是「exit 0 就算过」。

### 15.3 实测结果

- 两个引擎现场构建：`sandvik:configure` 1.84 s、`sandvik:build` 50.74 s、`unidbg:build` 22.08 s、`unidbg:classpath` 2.2 s。
- `verify --which both` 绿：`bridge:javac` rc=0、`native:ndk` rc=0、`accept:m2` rc=0（`SandvikBridgeTest` 21 项断言）、`accept:m3` rc=0（native 回调 Java 三条全过）；报告里带 `java_version`、两个引擎的 revision 与分支。
- 工具自带单测 **67 个全过**；伞仓根套件 `python -m pytest tests -q` **191 passed + 111 subtests**。

### 15.4 落地期间撞到的三个平台坑（写进代码注释）

1. **NDK clang 的无扩展名脚本**：`aarch64-linux-android21-clang` 是 POSIX 脚本，Windows 直接 `CreateProcess` 报 `WinError 193` ⇒ `Layout.ndk_clang()` 在 Windows 优先选 `.cmd`/`.exe`；`flow.run_step` 增加 `OSError → rc=126`（不再抛栈）。
2. **maven-gpg 插件**：`mvnw install` 在无 gpg 的机器上失败（`Could not determine gpg version`）⇒ 构建加 `-Dgpg.skip=true`；且必须 `install`（不是 `package`），否则 `unidbg-android` 解析不到 `unidbg-api` 的快照。
3. **sandvik 的嵌套 submodule**：`third_party/sandvik` 自己还挂着 `ext/{args,axml-parser,fmt,googletest,libffi,xxHash}`，浅克隆取不到任意 SHA ⇒ 去掉 `--depth 1` 递归 update（工具为此有独立的 `sandvik:submodules` 步骤）。

### 15.5 边界（别把它当脱壳器）

§10 的结论没变：这条链是**native 侧动态取证 + dex 行为验证**，不是脱壳路线。壳把明文写回运行时 dex 结构（真机上是 ART 的 CodeItem），unidbg/sandvik 都没有那个落点；本项目也不需要（store 已明文、encKey 已静态拿到）。同样地，`hook.min` 才是去保护终点，这里不声称任何「已脱壳」。


