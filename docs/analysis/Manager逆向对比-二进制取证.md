# Manager 逆向对比（第三版）—— 二进制反汇编取证

> 分析对象：`D:\Jeffy\桌面\CrashManager沙盒\Manager`
> 参照源码：`D:\Coding\WorkSpace\SandboxieCrack`（Sandboxie-Plus 1.16.5 / 5.71.5 快照）
> 分析手段：PE 静态解析 + `.pdata` 函数级反汇编（capstone 5.0.7）+ IAT 交叉引用 + 字符串引用定位 + 常量写入扫描
> 分析日期：2026-11-01

---

## 0. 为什么要有第三版

前两版以**源码 diff** 为主证据，存在推断成分。本版改为**以已发布二进制为唯一事实源**，逐条复核。

复核结果：**核心结论全部成立**，但 v2 有 **3 处表述错误/遗漏**，已在本版更正（见 §1.3）。这正是本次逆向对比的价值所在。

### 0.1 取证方法（可复现）

| 手段 | 做法 | 用途 |
|---|---|---|
| `.pdata` 函数枚举 | 读 `IMAGE_DIRECTORY_ENTRY_EXCEPTION`，每 12 字节一个 `RUNTIME_FUNCTION` | 得到 621 个函数的精确边界，避免线性扫描失步 |
| 函数级反汇编 | 逐函数独立 `disasm`，而非整段线性扫描 | SandMan.exe 线性扫描在 `0x14001c7d1` 即失步（只解出 2.9 万条指令）；改为函数级后解出 **51.8 万条** |
| IAT 桩判别 | 区分 `FF 25` 跳转桩 与真实 `call/jmp rel32` | 判定"某导入是否真被调用"，而不是被 IAT 表项本身误导 |
| 字符串 xref | 定位字符串 VA → 反查 RIP 相对引用 → 归属函数 | 证明某段逻辑是否还活着 |
| 常量写入扫描 | 找 `mov [rip+X], imm` / `or [rip+X], imm` | 直接读出硬编码的证书位域 |

### 0.2 基线指纹

| 文件 | 大小 | SHA256(前16) | 编译时间 | 签名 |
|---|---:|---|---|---|
| `CrashDrv.sys` | 267,712 | `a50f124cd878cf68` | 2026-09-01 | **4032B（测试签名）** |
| `CrashSvc.exe` | 451,584 | `b96e80c918d6fa84` | 2026-09-01 | 无 |
| `CrashDll.dll` | 945,664 | `1308171ad08ed215` | 2026-09-01 | 无 |
| `SandMan.exe` | 3,920,896 | `6d05ac75237b57f5` | 2026-09-01 | 无 |
| `QSbieAPI.dll` | 697,344 | `0221ef0772f32ce4` | 2026-09-01 | 无 |
| `wtsapi32.dll` | 162,304 | `817229042b1b2802` | 2026-03-29 | 无 |

PDB 路径全部指向 `D:\a\SandboxieCrack\SandboxieCrack\...` —— **GitHub Actions 构建**，仓库名与本仓库一致。

> 除驱动外，**全部用户态模块均未签名**（Security 目录 Size = 0）。驱动用 `CN="WDKTestCert runneradmin,..."` 自签，有效期 2026-09-01 → 2036-09-01，无时间戳。

---

## 1. 结论速览

### 1.1 一句话

> **许可破解发生在上游信任链的最顶端：驱动把"证书校验"整个函数体删掉，直接往 `Verify_CertInfo` 里写死一个 `0xF000E401`（永久 / 最高级别 / 全功能）。** 加解密代码一行没动，只是没人再调用它了。

### 1.2 硬证据（CrashDrv.sys）

`Verify_CertInfo` 全局变量位于 **`0x140039B90`**（`SCertInfo` 联合体，8 字节）。在 `KphValidateCertificate`（函数体 `0x14002764C – 0x14002779D`）中有三条写入：

```
0x14002766F   mov qword ptr [rip+0x12516], 1          -> [0x140039B90] = 1        ; 清零 + State=1
0x14002767E   mov dword ptr [rip+0x12508], 0xF000E401 -> [0x140039B90] = 0xF000E401
0x1400276F9   or  dword ptr [rip+0x12490], 1          -> [0x140039B90] |= 1       ; active=1
```

按 `verify.h` 中 `SCertInfo` 位域解码 `0xF000E401`：

| 字段 | 位 | 值 | 含义 |
|---|---|---|---|
| `active` | 0 | **1** | 证书有效 |
| `expired` | 1 | 0 | 未过期 |
| `outdated` | 2 | 0 | 不过时 |
| `grace_period` / `locked` / `lock_req` | 5/6/7 | 0/0/0 | 无宽限期、无锁 |
| `type` | 8–12 | `0b00100` = 4 | **`eCertEternal`（永久授权）** |
| `level` | 13–15 | `0b111` = 7 | **`eCertMaxLevel`（最高级别）** |
| `opt_desk` | 28 | **1** | 隔离桌面 |
| `opt_net` | 29 | **1** | 高级网络功能 |
| `opt_enc` | 30 | **1** | 沙箱加密 / EFS |
| `opt_sec` | 31 | **1** | 安全增强模式 / 隐私模式 |

**这与本地 `verify.c` 第 565–575 行逐字段完全一致，且函数体内不存在任何对 `Certificate.dat` 的读取。**

### 1.3 对 v2 的修正（重要）

| # | v2 的说法 | 第三版实测 | 结论 |
|---|---|---|---|
| ① | "`KphVerifySignature()` 结尾 `return status` 被改成 `return TRUE`" | **错**。`KphVerifySignature`（`0x140027C50`，236B）**完整保留真实 ECDSA-P256 验签**：`BCryptOpenAlgorithmProvider("E")` → `BCryptImportKeyPair("ECS1 ")` → `BCryptVerifySignature`。本地源码 `verify.c:220-258` 也是完好的 `return status;` | **更正**：被改的不是它 |
| ② | "`BCryptVerifySignature` 导入仍在，但**已经没有任何调用路径能到达它**" | **错**。`ksecdd.sys!BCryptVerifySignature` 的 IAT 桩 `0x14002B58D` 有 **3 处真实 `call`**（`0x140027A55`、`0x140027CF8` 等），全部落在真实的验签函数体内 | **更正**：调用链存在，只是"没人调用这些验签函数" |
| ③ | 改名审计未提及导出族差异 | **遗漏**。`CrashDll.dll` 只改了 `SbieApi_* → CrashApi_*`（53 个），`SbieDll_*` **77 个导出原样保留**；32 位版同样是 `SbieDll_*` 46 / `CrashApi_*` 38 | **新增发现** |

> v2 中被证实的部分：`KphValidateCertificate` 整体替换、`KphVerifyFile`/`KphVerifyBuffer`/`KphVerifyCurrentProcess` 打桩、`CertBlockList` 删除、`SandMan` 的 `Hacker` 硬编码、改名覆盖面 —— **全部在二进制中得到确认**。

---

## 2. 内核驱动 `CrashDrv.sys` 逐点取证

代码段 `.text`：`0x140001000`，199,680 字节；`.pdata` 函数 621 个。

| 编号 | 函数 / 对象 | 地址 | 二进制事实 | 判定 |
|---|---|---|---|---|
| **K1** | `KphValidateCertificate` | `0x14002764C`–`0x14002779D` (337B) | 头部即写 `[0x140039B90]=0xF000E401`；随后仍调用 `MyInitHash`、3 次 `Mem_Alloc`（为 `path`/`line`/`temp` 分配），但**分配后从未使用**，结尾 `0x140027775: mov eax,1` | **许可核心已被摘除** |
| **K2** | 证书解析器 | — | `NAME` / `TYPE` / `LEVEL` / `OPTIONS` / `SIGNATURE` 五个字面量在驱动中 **出现 0 次**（ASCII 与 UTF-16 双查） | 解析逻辑整体删除 |
| **K3** | `%s\Certificate.dat` | `0x1400313D4` | 字符串**存在**，但 **被引用 0 次**（孤儿字符串） | 与源码 `verify.c:539` 的 `static const WCHAR *path_cert` 声明残留、读取代码被删 **精确吻合** |
| **K4** | `\Certificate.dat`（无 `%s`） | `0x14002F370` | 被 `0x14000BD26` 引用，归属函数 `0x14000B978`(2665B) | 这是 `file.c:1374` 的**文件保护规则**（阻止沙箱内进程改写证书），与解析无关 |
| **K5** | `KphVerifyCurrentProcess` | `0x14002785C` | 7 字节桩：`B8 01 00 00 00 C3` = `mov eax,1; ret` | 进程 `.sig` 校验彻底失效 |
| **K6** | `KphReadSignature` | `0x1400274F0`–`0x14002764B` (348B) | 完整保留 `ZwCreateFile` → `ZwQueryInformationFile` → 大小上限 `0x1FFFF` 校验 → `ExAllocatePoolWithTag` → `ZwReadFile` → `ZwClose`；但尾部 `0x140027630: mov eax,1` | **照做 I/O，永远报成功** |
| **K7** | `KphVerifyFile` | `0x140027864`–`0x1400278D4` (113B) | 调 `KphHashFile`(0x140027228) 后调验签 `0x140027C50`，返回真实 `status`；**直接调用点 = 0 处** | 不可达死代码 |
| **K8** | `KphVerifySignature` | `0x140027C50`–`0x140027D3B` (236B) | 真实 ECDSA-P256 验签，无长度预检 | 完好，但仅被 `KphVerifyBuffer` 调用 |
| **K8′** | 第二份验签 | `0x1400279A0`–`0x140027A92` (243B) | 同上，但多一条 `cmp r9d,0x40`（SignatureSize 必须 = 64），失败返回 `0xC000A000` | 两份实现并存（见 §2.2） |
| **K9** | `KphVerifyBuffer` | `0x1400277A0`–`0x14002785A` (375B) | `MyInitHash` → `BCryptHashData` → `MyFinishHash` → 验签 → 释放，返回真实 status | 完好但无有效上游 |
| **K10** | `MyIsCallerSigned` | `0x140026648` | `B0 01 C3` = `mov al,1; ret`，**被 6 处调用**（`0x140002C0E`、`0x14000319A`、`0x14000330D`、`0x14001E4E3`、`0x14001E5C2` 等，集中在 `process.c`/`api.c` 区） | 恒为"已签名" |
| **K11** | `MyIsTestSigning` | `0x140026718` | `B0 01 C3` = `mov al,1; ret`，直接调用点 0（已被内联） | 恒为"测试签名模式" |
| **K12** | 吊销黑名单 | — | `CertBlockList` / `CertBlockListSig` / `CertBlocked` 在 `CrashDrv.sys` 与 `CrashSvc.exe` 中均 **0 次** | 远程吊销通道拆除 |
| **K13** | `Verify_CertInfo` 下游 | `0x140039B90` | 被 4 个函数引用：`0x140002D30`(375B)、`0x140017A74`(1895B)、`0x140026C58`(135B)、`0x14002764C` | 下游闸门全部吃这个硬编码值 |
| **K14** | `SandboxieLicenseV2` | `0x140033128` | 被 `0x140028814` 引用，归属函数 `0x1400287B0`(1262B) | V2 授权读写仍存在（服务于本地卡密体系） |

### 2.1 关键反汇编：`KphValidateCertificate`

```
0x14002764c  mov r11, rsp
0x14002765c  sub rsp, 0x70
0x140027660  mov rax, [rip+0x12099]          ; __security_cookie
0x14002766a  mov [rsp+0x60], rax
0x14002766f  mov qword ptr [rip+0x12516], 1  ; -> Verify_CertInfo = 1
0x14002767e  mov dword ptr [rip+0x12508], 0xF000E401   ; <== 硬编码证书
0x140027688  call 0x140029c80                ; MyInitHash
0x140027695  mov rcx, [rip+0x12214]          ; Driver_HomePathDos
0x14002769c  call 0x14002b116                ; wcslen
0x1400276ab  lea esi, [rax*2+0x40]           ; path_len = wcslen*2 + 64
0x1400276b4  call 0x140017108                ; Mem_Alloc(path)
0x1400276cb  call 0x140017108                ; Mem_Alloc(line, 0x800)
0x1400276e2  call 0x140017108                ; Mem_Alloc(temp, 0x800)
0x1400276f9  or  dword ptr [rip+0x12490], 1  ; Verify_CertInfo.active = 1
0x140027716  call 0x140027158                ; KphGetBuildDate
0x140027737  call [rip+0xae2b]               ; ExSystemTimeToLocalTime
0x140027742  call 0x140010804                ; Mem_Free(path)
0x140027754  call 0x140010804                ; Mem_Free(line)
0x140027766  call 0x140010804                ; Mem_Free(temp)
0x140027770  call 0x140029c34                ; MyFreeHash
0x140027775  mov eax, 1                      ; return TRUE
```

三次 `Mem_Alloc` 出来的缓冲区**全程没有任何写入或读取** —— 这是"把读证书的代码删掉、但保留了分配语句"的典型编译产物，也是本报告最有力的证据之一：**它证明代码是被人工挖空的，而不是原版行为**。

### 2.2 为什么有两份验签实现

`0x140027C50`（无长度校验）与 `0x1400279A0`（`SignatureSize==64` 预检）几乎逐指令相同，且各配一个 `KphVerifyBuffer` 克隆（`0x1400277A0` / `0x1400278D8`）。

这符合"同一份 `verify.c` 被两个编译单元各自实例化"的特征（`verify.c` 在驱动工程中被 include 两次，或一份来自静态库）。**不影响结论**：两份验签都完好，但都只被 `KphVerifyBuffer` 调用，而 `KphVerifyBuffer` 在破解后的调用图中没有任何通向"证书校验"的入口。

### 2.3 关于 `BCryptVerifySignature` 的可达性（修正 v2）

```
ksecdd.sys!BCryptVerifySignature  IAT=0x1400320E0
  └─ IAT 跳转桩 @0x14002B58D  FF 25 4D 6B 00 00   (jmp qword ptr [rip+0x6B4D])
       ├─ call @0x140027A55  (函数 0x1400279A0，即第二份验签)
       ├─ call @0x140027CF8  (函数 0x140027C50，即主验签)
       └─ 第三处同上
```

**真实调用点 = 3 处，不是 0 处。** v2 把"IAT 表项存在"误当成"只有表项"，本次已纠正。

真正的断点是：**`KphValidateCertificate` 不再调用任何验签函数**。也就是说，破解者没有去阉割密码学，而是把"调用密码学的那一段"整段删了 —— 这比打补丁更难被静态检测发现，因为所有加密 API 的调用图看起来完全正常。

---

## 3. 用户态取证

### 3.1 `SandMan.exe`

| 项 | 证据 |
|---|---|
| `Hacker` 硬编码 | 字符串池 `0x14024D2E0` 起连续存放 `This copy of Sandboxie-Plus is certified for: %1\0` 与 `Hacker\0`；两者**同属函数 `0x1400209D0`**（About 对话框构造函数），分别在 `0x140020ACC` 与 `0x140020AF8` 被 `lea` 引用，中间走 `QMetaObject::tr` + `QString::arg` |
| 证书告警整段删除 | `The supporter certificate is not valid for this build, please get an updated certificate` → **0 次**；`The evaluation period has expired!!!` → **0 次**。对应源码 `SandMan.cpp:3097-3106` 被整块删掉 |
| 证书模板文案**保留** | `SIGNATURE: AAAA…`、`NAME`、`This does not look like a certificate` 仍在 —— 这是设置界面给用户"粘贴证书"用的模板，属 UI 层，与驱动校验无关 |
| 吊销黑名单 | `CertBlockList` → **0 次** |
| 版本资源未改 | `sandboxie-plus.com` / `Sandboxie Manager` / `1.16.5` / `SandMan.exe` 原样保留 |
| 自动更新 | 源码 `OnlineUpdater.cpp` 的 `CheckForUpdates` 首行 `return;`、`IsLockRequired()` 恒 `false`、`cbl` 黑名单整段注释 —— 二进制中对应告警文案缺失可佐证 |

### 3.2 `CrashSvc.exe`

- 导入 `crashdll.dll`、`wtsapi32.dll`
- 保留自有卡密体系：`\License.dat`、`Sandboxie product activation`、`SandboxieCrashLicenseRequestKeyV1`、`http://223.247.195.3:8848/api/sandbox-license/v1/lease`、`SBXD2-`、`Sandboxie-License/1.0`、`SANDBOX_LICENSE_SERVER_URL`、`LicenseServerUrl`
- **`CertBlockList` = 0 次** → 上游吊销机制同步移除
- `Sandboxie_ComProxy`(4) / `GuiProxy`(2) / `NetProxy`(1) / `UacProxy`(1) / `ConsoleReadyEvent`(1) / `SandboxieLogon`(1) —— 这些**命名空间一个都没改**

### 3.3 `CrashDll.dll`（注入 DLL）

导出 142 个，前缀分布：

| 前缀 | x64 数量 | 32 位数量 | 状态 |
|---|---:|---:|---|
| `SbieDll_` | **77** | **46** | **未改名** |
| `CrashApi_` | 53 | 38 | 已改名（原 `SbieApi_`） |
| `Scm_` | 8 | — | 未改名 |
| `Sbie_` | 2 | 2 | 未改名 |

**这是本次新发现的改名不一致**：同一个 DLL 里，两套导出族一套改了、一套没改。任何按 `SbieDll_*` 名字调用它的第三方代码/旧版工具仍能正常工作，而按 `SbieApi_*` 调用的会失败。

### 3.4 `CrashMsg.dll`

3,322,368 字节的消息库中仍有 **3306 处 `Sandboxie`**、**252 处 `SbieDrv`**、**95 处 `SbieSvc`**、**95 处 `Sandboxie.ini`**。改名没有穿透到消息正文 —— 用户在报错对话框里看到的仍然是 "Sandboxie"、"SbieDrv"。

---

## 4. 改名审计（二进制层）

### 4.1 已改名（确认生效）

| 项 | 原值 | 新值 | 证据 |
|---|---|---|---|
| 驱动文件名 / 服务名 | `SbieDrv` | `CrashDrv` | 全部 10 个二进制中 `SbieDrv` **0 次** |
| 设备名 | `\Device\SbieDrv` | `\Device\CrashDrv` | 旧名 0 次，新名 2 次 |
| 服务程序 | `SbieSvc` | `CrashSvc` | 驱动侧 0 次 |
| 注入 DLL | `SbieDll` | `CrashDll` | 文件名已改（导出前缀未改，见 §3.3） |
| 导出前缀（部分） | `SbieApi_` | `CrashApi_` | 旧前缀 **0 次** |
| 主工程版本资源 | `Sandboxie` / `Tonalio GmbH` | `My` / `my.163.com` | `CrashDrv`/`CrashSvc`/`CrashDll`/`CrashMsg` |

### 4.2 未改名（原样保留）

| 命名空间 | 出现位置（次数） |
|---|---|
| `Sandboxie_SingleInstanceMutex_Control` | QSbieAPI.dll 1、SbieCtrl.exe 1 |
| `SandboxieControlWndClass` | QSbieAPI.dll 1、SbieCtrl.exe 1 |
| `Sandboxie_ComProxy` | CrashSvc.exe 4 |
| `Sandboxie_GuiProxy` | CrashSvc.exe 2 |
| `Sandboxie_NetProxy` | CrashSvc.exe 1 |
| `Sandboxie_UacProxy` | CrashSvc.exe 1 |
| `Sandboxie_ConsoleReadyEvent` | CrashSvc.exe 1 |
| `SandboxieAllGroup` | CrashDrv.sys 1、SandMan.exe 1 |
| `SandboxieLogon` | CrashSvc.exe 1、SandMan.exe 1 |
| `SbieFilter` / `SbieCallout` / `SbieSublayer` | CrashDrv.sys 各 1（WFP 过滤器名） |
| `Sandboxie.ini` | CrashMsg.dll 95、SandMan.exe 8、CrashDrv.sys 1、CrashSvc.exe 1 |
| `SandboxieLicenseV2` / `SandboxieClientBindingV1` | CrashDrv.sys 各 1（注册表值名） |
| `Xanasoft` | SandMan.exe 3（`%LOCALAPPDATA%\Xanasoft\...` 配置路径） |

### 4.3 后果

1. **可与官方版共存**（这正是目的）：服务名/驱动名/设备名不同，两套可以同时安装、同时运行。
2. **但与官方版共享状态**：`Sandboxie.ini`、`SandboxieAllGroup`、`SandboxieLogon`、`Sandboxie_*Proxy` 管道、单实例互斥体全部同名 —— 同时运行时**会互相串扰**（配置互相覆盖、管道抢占、单实例互斥体冲突）。
3. **改名不彻底暴露了工作方式**：只改"用户看得见的"（文件名、服务名、版本信息、主图标），不改"运行时内部标识"。这是典型的**低成本换皮**，不是干净的分支。

---

## 5. 完整攻击链（更新版）

```
┌─ 编译期（源码层，一次性）────────────────────────────────────┐
│  verify.c   : KphValidateCertificate 挖空 → Verify_CertInfo = 0xF000E401 │
│  util.c     : MyIsTestSigning / MyIsCallerSigned → return TRUE          │
│  driver.c   : Driver_OsTestSigning = TRUE                               │
│  api.c      : Api_SetServicePort 签名失败改为 STATUS_SUCCESS            │
│               FeatureFlags 无条件置 CERTIFIED/SECURITY/ENCRYPTION/...   │
│  DriverAssistStart.cpp : 删 CertBlockList / CertBlockListSig / CertBlocked│
│  MountManager.cpp      : 删 opt_enc 门槛（加密卷不再需要授权）           │
│  SandMan.cpp : ReloadCert 硬写 g_CertInfo + About 显示 "Hacker"          │
│  OnlineUpdater.cpp : CheckForUpdates 直接 return / IsLockRequired=false  │
└──────────────────────────────────────────────────────────────────────────┘
                              ↓ 产出
┌─ 安装期 ────────────────────────────────────────────────────┐
│  CrashDrv.sys 用 WDKTestCert 自签（需测试签名模式或关 DSE）  │
│  wtsapi32.dll 侧加载（LocalSystem 服务）→ BYOVD 关 DSE      │
│  Certificate.dat 写入 NAME:Administrator / TYPE:CONTRIBUTOR │
│    （纯装饰，驱动根本不读它）                                │
└──────────────────────────────────────────────────────────────┘
                              ↓ 运行期
┌─ 运行期 ────────────────────────────────────────────────────┐
│  DriverEntry → KphValidateCertificate → 直接返回永久全功能   │
│  → Verify_CertInfo = 0xF000E401                             │
│  → 4 个下游函数读到"永久 + 最高级 + 全功能"                  │
│  → Api_QueryDriverInfo 回给用户态同一份位域                  │
│  → SandMan::ReloadCert 再硬写一遍（双保险）                  │
│  → UI 显示 "certified for: Hacker"                          │
└──────────────────────────────────────────────────────────────┘
```

**关键洞察**：整条链上，**唯一真正起作用的是第一环**（驱动里的那 4 条指令）。后面所有环节（`Certificate.dat`、`wtsapi32.dll` 的 DSE 关闭、SandMan 的硬编码）都是"让这个驱动能被加载、能显示得像那么回事"的配套设施。

---

## 6. 本地源码 vs 已发布二进制：一致性核对

| 源码补丁 | 二进制中是否可见 | 说明 |
|---|---|---|
| `KphValidateCertificate` → 硬编码 | ✅ 完全一致 | `0xF000E401` 逐位吻合 |
| `KphVerifyCurrentProcess` → `return TRUE` | ✅ 7 字节桩 | |
| `KphReadSignature` → `return TRUE` | ✅ 保留 I/O + `mov eax,1` | |
| `MyIsCallerSigned` / `MyIsTestSigning` → TRUE | ✅ `mov al,1; ret` | |
| `CertBlockList` 删除 | ✅ 全链 0 次 | |
| `SandMan` `"Hacker"` | ✅ 同函数引用 | |
| 证书告警文案删除 | ✅ 0 次 | |
| `SbieApi_` → `CrashApi_` | ✅ | |
| **`KphVerifyFile` → `return TRUE`** | ⚠️ **不一致** | 二进制里该函数仍 `return status`（`0x1400278C8: mov eax,edi`）。但**该函数直接调用点为 0**，属不可达死代码，**无实际影响** |

> 这条不一致说明：本地工作副本与 CI 实际构建的那一版存在细微漂移（本地多改了一处）。考虑到它落在死代码上，不影响任何结论；但若后续要复现构建，需注意这一点。

---

## 7. 未验证项与残余风险

| 项 | 状态 | 原因 |
|---|---|---|
| 与**官方原版二进制**的逐字节对照 | ❌ 未完成 | 官方安装包是 **Inno Setup 6.3.0**，`innoextract` 1.9（最高支持 6.0.5）与 `innounp` 0.50（最高 6.2）均报 `Stream error while parsing setup headers` / `The setup files are corrupted`；1.10/1.11 版本下载 404。**本报告的所有"官方应有行为"均来自官方源码 `sbx_official.zip`（v1.16.5，1702 条目）而非官方二进制** |
| `wtsapi32.dll` 的实际劫持效果 | ⚠️ 静态可读，动态未测 | 导出全部 69 个 WTS API；含 `RTCore64 v4.6.2`、`\\.\RTCore64`、`g_CiOptions`、`SeLoadDriverPrivilege`、`NtLoadDriver`。其 `PatchDriver()` 的目标服务名表是 `{SbieDrv, LukDrv}` —— **不含 `CrashDrv`**，所以它对 `CrashDrv.sys` 的 4032 字节测试签名**没有做任何处理**（这也反证了许可破解在源码层完成，与驱动加载器无关） |
| `SbieKey` 工程（`D:/Coding/WorkSpace/SbieKey`）的二进制 | ❌ 无产物 | 仓库内只有 `zlib1.dll`，未找到编译出的 `wtsapi32.dll`/驱动；无法与 Manager 内的 `wtsapi32.dll` 做同源比对 |
| 官方安装包内 `SbieShellPkg.msix` 签名 | ⚠️ | Manager 内的 `SbieShellPkg.msix` 仅 2,700B（AppxManifest + BlockMap + ContentTypes），**无签名**，`Executable="SbieShell.exe"` 而该文件在目录中不存在 —— 即这个 MSIX 包是**不可安装的占位文件** |

---

## 8. 附录：复现命令

```bash
# 1. 函数边界
python - <<'EOF'
import struct, pefile
pe = pefile.PE(r'...\CrashDrv.sys'); base = pe.OPTIONAL_HEADER.ImageBase
dd = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']]
raw = pe.get_data(dd.VirtualAddress, dd.Size)
for i in range(0, len(raw), 12):
    b, e, _ = struct.unpack('<III', raw[i:i+12])
    if b and e: print(f'{base+b:#x}-{base+e:#x}')
EOF

# 2. 定位硬编码证书位域
#    在 0x14002764C 处，可见  C7 05 08 25 01 00 01 E4 00 F0
#    即 mov dword ptr [rip+0x12508], 0xF000E401 -> [0x140039B90]

# 3. 验证 %s\Certificate.dat 是孤儿字符串
python rev4.py CrashDrv.sys --xref 0x1400313D4
#   -> 内存引用=0  直接调用=0

# 4. 验证 KphVerifyFile 不可达
python rev4.py CrashDrv.sys --fn 0x140027864
#   -> 被调用 0 次
```

---

## 9. 一页总结

1. **许可破解在内核**：`CrashDrv.sys` 的 `KphValidateCertificate`（`0x14002764C`）把 `Verify_CertInfo`（`0x140039B90`）直接写成 `0xF000E401` —— 永久授权、最高级别、桌面/网络/加密/安全四项全开。证书文件根本不被读取。
2. **密码学没被动过**：ECDSA-P256 验签函数两份都完好，`BCryptVerifySignature` 有 3 处真实调用 —— 只是没人再调它们了。这种改法在静态扫描里几乎看不出来。
3. **改名只做了"面子"**：文件名、服务名、驱动名、设备名、`SbieApi_` 前缀、主工程版本资源都改了；`Sandboxie.ini`、所有 `Sandboxie_*Proxy` 管道、窗口类、互斥体、WFP 过滤器名、`SandboxieLicenseV2` 注册表值一个没改。
4. **改名还漏了一整族导出**：`CrashDll.dll` 里 `SbieDll_*` 77 个导出原样保留，只有 `SbieApi_*` 变成了 `CrashApi_*`。
5. **v2 有三处要更正**：`KphVerifySignature` 没被改成 `return TRUE`；`BCryptVerifySignature` 不是"零调用"；`SbieDll_*` 导出未改名是本版新发现。
