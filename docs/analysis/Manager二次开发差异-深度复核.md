# Manager 二次开发差异 · 深度复核（改名 + 破解上游许可）

> ⚠️ **勘误（后补）**：本版以源码 diff 为主要证据，其中有 3 处结论已被第三版《Manager逆向对比-二进制取证.md》用反汇编推翻或修正：
> 1. 本文称 `KphVerifySignature` 被改成 `return TRUE` —— **错**，该函数完整保留真实 ECDSA 验签，被改的是 `KphVerifyFile` / `KphReadSignature` / `KphVerifyCurrentProcess`。
> 2. 本文称 `BCryptVerifySignature` "已无任何调用路径能到达" —— **错**，存在 3 处真实调用点；真正的断点是 `KphValidateCertificate` 不再调用验签。
> 3. 本文遗漏：`CrashDll.dll` 的 `SbieDll_*` 导出族（x64 77 个 / x86 46 个）**未改名**，只有 `SbieApi_*` → `CrashApi_*` 改了。
>
> 其余结论（改名覆盖面、`Verify_CertInfo` 硬编码、`CertBlockList` 移除、`"Hacker"` 硬编码等）均已获二进制证实。**请以第三版为准。**

> 本报告是对第一版《Manager二次开发差异分析.md》的**替代与补强**，只聚焦两条线：
> **① 改名（穷尽审计）**、**② 破解上游许可（完整逻辑链）**。
> 自建的 `License.dat` 卡密体系按要求忽略，仅在必要处一句带过。
>
> 复核新增的关键证据：已取得官方 **v1.16.5 全量源码**（1592 个文件）与本仓库做**全树 diff**，
> 并逐条回落到 Manager 二进制验证。

---

## 0. 先说一个颠覆性结论

**本项目（`D:\Coding\WorkSpace\SandboxieCrack`）的源码本身，就已经是"破解完成"的状态。**

Manager 不是"官方二进制 + 打补丁"，而是：

```
官方 Sandboxie-Plus 5.71.5 源码
   ↓ ① 源码级破解（8 个文件被改，见 §1）
   ↓ ② 源码级改名（11 个产物改名，见 §2）
   ↓ ③ CI 编译（PDB: D:\a\SandboxieCrack\SandboxieCrack\...）
   ↓ ④ 测试证书签名驱动 + 附带 wtsapi32.dll（关 DSE 才能加载）
= Manager 目录
```

全树 diff 结果：

| 项 | 数量 |
|---|---|
| 官方文件 / 本地文件 | 1592 / 1538 |
| 内容不同的文件 | **106** |
| 其中属于**破解改动** | **8** |
| 其余 98 个 | 基线漂移（本仓库基于 v1.16.5 发布前的 master 快照，CHANGELOG 日期 `2025-10-xx`，官方 tag 为 `2025-11-04`） |

> 这一点很重要：如果不做全树 diff，很容易把 98 个上游自身演进的差异误判成"二开改动"，也会漏掉真正藏起来的破解点。

---

## 1. 破解上游许可：完整逻辑链

### 1.1 上游的许可体系是怎么工作的（4 层）

**第 1 层：证书文件**
`<安装目录>\Certificate.dat`，纯文本键值对，驱动逐行解析：

```
NAME: xxx
TYPE: CONTRIBUTOR | SUPPORTER | BUSINESS | EVALUATION | DEVELOPER | PATREON ...
LEVEL: 0..7
OPTIONS: ...
SIGNATURE: <base64(RSA-2048 签名)>
```

**第 2 层：驱动验签**（`Sandboxie/core/drv/verify.c`，官方 1260 行）
- `KphValidateCertificate()` 读文件、逐行解析、base64 解码签名
- 用**内嵌公钥**经 `BCryptImportKeyPair` + `BCryptVerifySignature`（`ksecdd.sys`）验签
- 验签通过后填充全局 `Verify_CertInfo`（`SCertInfo` 位域，见下表）
- 另外把证书写进注册表值 `SandboxieLicenseV2`、做机器绑定 `SandboxieClientBindingV1`

**第 3 层：驱动运行时闸门**（5 处 + 特性标志）

| 位置 | 作用 |
|---|---|
| `process.c:764` | 无 `opt_sec` → 用了 SecurityMode/PrivacyMode/ProtectHostImages/RestrictDevices 等 → **`Process_ScheduleKill(proc, 5*60*1000)` 5 分钟后杀掉进程** |
| `process.c:795` | 无 `opt_enc` → 用了 ConfidentialBox → 同样 5 分钟杀 |
| `process.c:1241` | 无 `active` → BreakoutProcess 功能降级并记日志 |
| `api.c:1246` | `GetDriverInfo` 把 `Verify_CertInfo.State` 回传用户态 |
| `api.c:1210+` | 计算 `FeatureFlags`（CERTIFIED / SECURITY_MODE / PRIVACY_MODE / COMPARTMENTS / ENCRYPTION / NET_PROXY / NO_SIG） |

**第 4 层：用户态闸门**（`SandMan.exe`）
`g_CertInfo` 驱动 UI：盒子类型、加密选项、网络过滤、自动更新、支持提醒、区域锁等。

**`SCertInfo` 位域语义**（`verify.h`）：

```c
active:1  expired:1  outdated:1  grace_period:1  locked:1  lock_req:1
type:5    level:3
opt_desk:1  // UseSandboxDesktop
opt_net:1   // NetworkDnsFilter / NetworkUseProxy
opt_enc:1   // ConfidentialBox / UseFileImage / EnableEFS
opt_sec:1   // UseSecurityMode / SysCallLockDown / RestrictDevices /
            // UseRuleSpecificity / UsePrivacyMode / ProtectHostImages / NoSecurityIsolation
```

证书类型枚举里，**`eCertEternal`(0b00100) 是"永久"**，且被 `CERT_IS_INSIDER` 判定为 insider；
**`eCertMaxLevel`(0b111) 是最高档**。

---

### 1.2 破解点总表（16 处 + 3 个配套物）

| # | 文件 | 位置 | 官方逻辑 | 改成 | 效果 |
|---|---|---|---|---|---|
| D1 | `core/drv/verify.c` | `KphVerifySignature()` | `return status` | `return TRUE` | RSA 验签函数**永远成功** |
| D2 | `core/drv/verify.c` | `KphVerifyFile()` | `return status` | `return TRUE` | 文件签名校验**永远成功** |
| D3 | `core/drv/verify.c` | `KphVerifyCurrentProcess()` | 校验进程镜像签名 | 函数首行插入 `return TRUE;` | 当前进程签名校验**永远成功** |
| D4 | `core/drv/verify.c` | `KphValidateCertificate()` | 254 行：读文件→解析→验签→算过期 | **整段删除**；`Verify_CertInfo` 硬编码 | 证书不再被读取，直接得到"永久+最高级+全功能" |
| D5 | `core/drv/util.c` | `MyIsTestSigning()` | 查询 `SystemCodeIntegrityInformation` | 首行 `return TRUE` | 驱动认为系统处于**测试签名模式** |
| D6 | `core/drv/util.c` | `MyIsCallerSigned()` | 校验调用者镜像签名 | 首行 `return TRUE` | **任何调用者都算"已签名"** |
| D7 | `core/drv/driver.c` | `Driver_OsTestSigning` | 初值 `FALSE` | 初值 **`TRUE`** | 全局默认开启测试签名豁免 |
| D8 | `core/drv/api.c` | `Api_SetServicePort()` | 调用者未签名 → `STATUS_INVALID_SIGNATURE` | 注释掉，改 `STATUS_SUCCESS` | 未签名的服务也能向驱动注册 LPC 端口 |
| D9 | `core/drv/api.c` | `FeatureFlags` 计算 | 7 个 flag 各自受 `Verify_CertInfo` 条件控制 | **7 个 flag 全部无条件置位** | 上层看到"已认证 + 全功能" |
| S1 | `core/svc/DriverAssistStart.cpp` | 证书黑名单 | 拉取 `CertBlockList`/`CertBlockListSig`，命中则置 `CertBlocked=1` | **整段删除（-14 行）** | 官方**远程吊销证书**的能力被摘除 |
| S2 | `core/svc/MountManager.cpp` | 加密卷/RAM 盘 | 需 `CertInfo.active && opt_enc` | **条件删除** | 无证书也能用文件镜像/内存盘 |
| U1 | `SandMan/SandMan.cpp` | `ReloadCert()` | 验签失败弹窗 + 删除无效证书 + 一堆 `Debug/CertFake*` 模拟 | **全部删除**，改为硬编码 | 见下 |
| U2 | `SandMan/SandMan.cpp` | `g_CertInfo` | 由驱动回传 | `active=true; expired=false; outdated=false; type=eCertEternal; level=eCertMaxLevel;` | **UI 认为持有永久最高级证书** |
| U3 | `SandMan/SandMan.cpp` | `SandMan.exe.sig` 校验 | 失败弹"没有有效签名"并中止 | 弹窗**注释掉**，`Status = SB_OK` | 未签名的管理器可正常启动 |
| U4 | `SandMan/SandMan.cpp` | `CheckForUpdates()` | 联网检查更新 | 首行 `return;` | 不联网、不更新 |
| U5 | `SandMan/SandMan.cpp` | 证书文案 | `certified for: %1`（取证书 NAME） | `certified for: **"Hacker"**` | 关于框直接写死 "Hacker" |
| U6 | `SandMan/OnlineUpdater.cpp` | ①`cbl` 黑名单 ②`IsLockRequired()` ③`CheckForUpdates()` | ①解析证书黑名单 ②`lock_req` **或系统语言 0x0804（简体中文）** 时锁定 ③检查更新 | ①整段注释 ②`return false` ③`return;` | 摘掉黑名单 + **去掉中文区域限制** + 不更新 |

**三个配套物（不属于源码改动，但共同构成完整破解）：**

| # | 物 | 说明 |
|---|---|---|
| A1 | `Certificate.dat` | 内容：`NAME: Administrator / TYPE: CONTRIBUTOR / SIGNATURE: NONE`。驱动已不看它，但用户态 `main.cpp` 仍会在启动时 `LoadCertificate()` 并显示，属于"给界面看的道具" |
| A2 | `CrashDrv.sys` 测试签名 | `CN="WDKTestCert runneradmin,134327185649426872"` 自签、无时间戳 → 正常系统加载不了 |
| A3 | `wtsapi32.dll` | 代理 DLL，关 DSE 让 A2 能加载（详见 §1.6） |

---

### 1.3 逐层详解

#### （1）驱动层：把"验签"整条链路拔掉

官方 `KphValidateCertificate()` 是 254 行的完整流程：
`拼路径 → Stream_Open → 读 BOM → 逐行 Conf_Read_Line → 解析 SIGNATURE/TYPE/LEVEL/OPTIONS → base64 解码 → 计算文件哈希 → BCryptVerifySignature → 校验有效期/绑定/节点锁 → 填 Verify_CertInfo`。

本地版本（821 行，官方 1260 行，**净删 439 行**）把它压缩成：

```c
_FX NTSTATUS KphValidateCertificate()
{
    Verify_CertInfo.State = 1;
    Verify_CertInfo.type  = eCertEternal;     // 永久
    Verify_CertInfo.level = eCertMaxLevel;    // 最高档
    Verify_CertInfo.opt_sec = 1;              // 安全增强全开
    Verify_CertInfo.opt_enc = 1;              // 加密全开
    Verify_CertInfo.opt_net = 1;              // 网络全开
    Verify_CertInfo.opt_desk = 1;             // 隔离桌面
    Verify_CertInfo.expired = 0;
    Verify_CertInfo.outdated = 0;
    Verify_CertInfo.active = 1;
    Verify_CertInfo.lock_req = 0;
    ... // 只做内存分配，然后直接 goto CleanupExit
    return TRUE;
}
```

配套把三个验签原语（`KphVerifySignature` / `KphVerifyFile` / `KphVerifyCurrentProcess`）改成恒真，
并把 `Driver_OsTestSigning` 初值改成 `TRUE`、`MyIsTestSigning()` 直接 `return TRUE` ——
等于驱动**永远认为自己跑在测试签名模式下**，从而对所有镜像签名检查走豁免分支。

> 注意：`BCryptVerifySignature` 的导入表项仍然存在（来自 `ksecdd.sys`），但已经没有任何调用路径能到达它。
> 这一点在二进制里可交叉验证：`CrashDrv.sys` 中 `SIGNATURE` / `TYPE` / `LEVEL` / `OPTIONS` /
> `CONTRIBUTOR` / `SUPPORTER` / `EVALUATION` 这些**解析用字符串全部消失**（§1.5）。

#### （2）服务层：摘掉"远程吊销"和"功能门槛"

- `DriverAssistStart.cpp`：官方会在服务启动时拉取证书黑名单并验签，命中则置 `CertBlocked`，
  用户态据此禁用 EVALUATION 证书。这段被整段删除 —— **官方再也无法吊销一份泄露/伪造的证书**。
- `MountManager.cpp`：官方用 `CertInfo.active && opt_enc` 卡住"文件镜像卷 / 内存盘"，
  条件被删 —— 无证书也能用加密盒。

#### （3）UI 层：把"证书状态"变成常量

`CSandMan::ReloadCert()` 里，官方逻辑是"调驱动拿证书 → 失败则弹对应错误（签名无效/不适用/节点锁定/被吊销）→ 删除无效证书 → 按类型做各种降级"。本地版本把这 130 多行全删，替换成 5 行硬编码：

```cpp
g_CertInfo.active   = true;
g_CertInfo.expired  = false;
g_CertInfo.outdated = false;
g_CertInfo.type     = eCertEternal;
g_CertInfo.level    = eCertMaxLevel;
```

于是所有基于 `g_CertInfo` 的 UI 判断（盒子类型、加密开关、网络过滤、自动更新、支持提醒、"即将过期"提示）全部走"已认证"分支。

再加三刀：`SandMan.exe.sig` 校验失败不再拦、`CheckForUpdates()` 直接 return、`IsLockRequired()` 恒 false。

#### （4）一个容易被忽略的点：`Api_SetServicePort` 的签名检查

官方驱动要求"注册 LPC 端口的服务进程必须是**已签名**的可执行文件"。
本产品服务 `CrashSvc.exe` 无签名 → 官方逻辑下会拿到 `STATUS_INVALID_SIGNATURE`、服务无法与驱动通信。
所以第 8 处补丁是**功能性必需**，不是顺手改的。

---

### 1.4 破解后的能力矩阵

| 功能 | 上游条件 | 破解后 | 上游不满足时的实际后果 |
|---|---|---|---|
| UseSecurityMode / SysCallLockDown / RestrictDevices / UseRuleSpecificity / UsePrivacyMode / ProtectHostImages / NoSecurityIsolation | `opt_sec` | ✅ 全开 | **进程 5 分钟后被驱动杀掉**（`Process_ScheduleKill`） |
| ConfidentialBox / UseFileImage / EnableEFS | `opt_enc` | ✅ 全开 | 同上 5 分钟杀 |
| NetworkDnsFilter / NetworkUseProxy | `opt_net` | ✅ 全开 | 功能不可用 |
| UseSandboxDesktop | `opt_desk` | ✅ 全开 | 功能不可用 |
| 盒子类型（Privacy Enhanced 等） | `active` + 类型 | ✅ 全开 | 界面禁用 |
| 自动更新 / 预览通道 | `CERT_IS_INSIDER`（`eCertEternal` 满足） | ✅ 可用 | 只能手动装 | 
| 证书吊销（CertBlockList） | 服务拉取并验签 | ❌ 已摘除 | 无法远程封禁 |
| 简体中文区域锁（`IsLockRequired`） | 语言 0x0804 → 锁定 | ❌ 已摘除 | 中文系统被限制 |
| 管理器自身签名（SandMan.exe.sig） | 必须有效 | ❌ 已忽略 | 未签名管理器拒绝启动 |
| 驱动验签 / 进程签名 | 必须有效 | ❌ 全豁免 | 加载失败 |

> **一句话**：破解后的证书状态（永久 + 最高档 + 全功能 + insider）**比上游任何一份真实证书都高**——
> 连 `Business`、`GreatPatreon` 都有到期与功能边界，而 `eCertEternal + eCertMaxLevel + opt_*全 1` 没有。

---

### 1.5 二进制侧交叉验证（不是只看源码）

| 验证点 | 方法 | 结果 |
|---|---|---|
| 证书解析被删 | 在 `CrashDrv.sys` 搜 `SIGNATURE`/`TYPE`/`LEVEL`/`OPTIONS`/`CONTRIBUTOR`/`SUPPORTER`/`EVALUATION` | **全部不存在**（ASCII 与 UTF-16 双查） |
| 残留骨架 | 搜 `Certificate.dat` / `SandboxieLicenseV2` / `SandboxieClientBindingV1` | 仍存在（未被调用的死代码） |
| 黑名单被摘 | 在 `CrashSvc.exe` 搜 `CertBlockList` / `CertBlockListSig` | **不存在** |
| 黑名单残留 | `SandMan.exe` 搜 `CertBlocked` | 仅剩 `SupportDialog.cpp` 的读取点，但驱动不再写入 → 恒为 0，无害 |
| 未签名的管理器 | `SandMan.exe` 里 `SandMan.exe.sig` 相关弹窗字符串 | 仍在，但调用点被注释 → 只影响外观 |

---

### 1.6 第二套破解：`wtsapi32.dll`（解"签名"，与源码破解互补）

源码级破解解决了"**许可**"，但解决不了"**驱动签名**"：`CrashDrv.sys` 只有一张
`WDKTestCert runneradmin` 自签证书，在开启 Secure Boot / DSE 的机器上加载不了。

于是附带一个**同名代理 DLL**：

```
CrashSvc.exe（静态导入 wtsapi32.dll）
  └─ 安装目录下的伪造 wtsapi32.dll 被优先加载（DLL 侧加载，LocalSystem 权限）
       ├─ 转发全部 69 个 WTS 导出到 %SystemRoot%\System32\wtsapi32.dll（功能无感）
       ├─ MinHook 挂钩 ntdll!NtLoadDriver
       └─ 命中驱动加载时：
            ① PatchDriver()：清 PE Security 目录、把 ksecdd!BCryptVerifySignature
               的调用改成 `31 C0 C3`（xor eax,eax; ret）、重算校验和
            ② 内嵌 RTCore64.sys（MSI Afterburner 4.6.2，CVE-2019-16098）拿内核读写
               → 清 ci!g_CiOptions，**关闭 DSE**
            ③ 原始 NtLoadDriver 加载驱动
            ④ 恢复 g_CiOptions
```

关键细节（源码 `D:\Coding\WorkSpace\SbieKey`，远端 `JeffyV/1sbiekey`）：

- 它只对服务名 **`SbieDrv` / `LukDrv`** 做"剥签+短路验签"，**不包含本产品的 `CrashDrv`**；
- 但"关 DSE"这一步是**无条件**执行的，所以 `CrashDrv` 仍能加载；
- 反过来证明：`CrashDrv` 的证书校验**不是靠这个 DLL 运行时打补丁**，而是**在源码里就删掉了**（§1.3 第 1 层）——
  二进制证据：`CrashDrv.sys` 至今仍带着 4032 字节的测试签名（若被 `PatchDriver` 处理过，签名目录会被清零）。

**两套破解的分工**：

| | 源码级（本项目内） | 外部 DLL（wtsapi32） |
|---|---|---|
| 解决 | 许可校验、功能开关、吊销、区域锁 | 驱动签名 / DSE |
| 作用对象 | 自己的 `CrashDrv` | 官方 `SbieDrv`、`LukDrv`，以及任意 `NtLoadDriver` |
| 通用性 | 只对本产品 | **通用破解器**，可直接用于官方 Sandboxie |

---

## 2. 改名：穷尽审计

### 2.1 审计方法（可复现）

1. 从源码中抽取**所有含 `sbie`/`sandboxie`（不区分大小写）的字符串字面量**：**429 条**；
2. 把 Manager 目录全部二进制（59.7 MB）拼成一个 blob；
3. 每条字面量做三态判定：原样命中 / `Sandboxie→Crash`·`Sbie→Crash` 变体命中 / 都未命中。

结果：

| 状态 | 数量 | 含义 |
|---|---|---|
| `RENAMED` | **14** | 原样不存在、改名后存在 → 真的被改了 |
| `BOTH` | 10 | 两种形式都存在（多为宏/短词） |
| `KEPT` | **196** | 原样存在 → **没改** |
| `ABSENT` | 209 | 二进制里都没有（注释、未编译分支、被裁掉的代码） |

> 也就是说：**429 条里真正被改名的只有 14 条**。改名不是"全局替换"，而是**精准的、最小化的**。

### 2.2 已改名的完整清单

**（a）产物文件名（11 个）**

| 官方 | Manager |
|---|---|
| `SbieDll.dll` | `CrashDll.dll`（含 `32\`） |
| `SbieSvc.exe` | `CrashSvc.exe`（含 `32\`） |
| `SbieDrv.sys` | `CrashDrv.sys` |
| `SbieMsg.dll` | `CrashMsg.dll` |
| `SboxHostDll.dll` | `CrashHostDll.dll` |
| `SbieShellExt.dll` | `CrashShellExt.dll` |
| `SandboxieBITS.exe` | `CrashBITS.exe` |
| `SandboxieCrypto.exe` | `CrashCrypto.exe` |
| `SandboxieDcomLaunch.exe` | `CrashDcomLaunch.exe` |
| `SandboxieRpcSs.exe` | `CrashRpcSs.exe` |
| `SandboxieWUAU.exe` | `CrashWUAU.exe` |

**（b）运行时标识符**

| 类别 | 官方 | Manager |
|---|---|---|
| 服务名 | `SbieSvc` | `CrashSvc` |
| 驱动服务 | `SbieDrv` | `CrashDrv` |
| 驱动设备 | `\Device\SbieDrv` | `\Device\CrashDrv` |
| RPC 端点 | `\RPC Control\SbieSvcPort` | `\RPC Control\CrashSvcPort` |
| 导出 API 前缀 | `SbieApi_*`（x64 53 / x86 38） | `CrashApi_*` |
| 日志/调试串 | `SbieApi_Ioctl: %s %s` | `CrashApi_Ioctl: ...` |
| 版本资源 | sandboxie-plus.com / Sandboxie* | `my.163.com` / `My*` |
| ShellExt 注册子键 | `...\Sandboxie-Plus\SbieShellExt\Lang` | `...\Sandboxie-Plus\CrashShellExt\Lang` |

**（c）配置模板同步（`Templates.ini`，全文件仅 4 行不同）**

```diff
- OpenIpcPath=\RPC Control\SbieSvcPort
+ OpenIpcPath=\RPC Control\CrashSvcPort

- HostInjectDll=\SboxHostDll.dll
- HostInjectDll64=\SboxHostDll.dll
- HostInjectDllARM64=\SboxHostDll.dll
+ HostInjectDll=\CrashHostDll.dll
+ HostInjectDll64=\CrashHostDll.dll
+ HostInjectDllARM64=\CrashHostDll.dll
```

**（d）未改名的 6 个可执行文件**

`SandMan.exe`、`SbieCtrl.exe`、`SbieIni.exe`、`Start.exe`、`KmdUtil.exe`、`UpdUtil.exe`、`ImBox.exe`
—— 文件名保留，但内部引用（`CrashDll.dll` / `CrashSvc` / `CrashDrv`）全部改过，属于"重编译未改名"。

### 2.3 没改名、但**必须改**的功能标识符（会与官方冲突）

这是本次复核最重要的发现之一：改名只覆盖了"文件名 / 服务 / 驱动 / 设备 / 导出 / 端口"，
**所有跨进程、跨内核的命名空间原封不动**。

| 类别 | 保留的官方名字 | 出现位置 | 冲突后果 |
|---|---|---|---|
| 单实例互斥体 | `Sandboxie_SingleInstanceMutex_Control` | `QSbieAPI.dll`、`SbieCtrl.exe` | 与官方版**不能同时运行**（互相判定为"已有实例"） |
| 窗口类 | `SandboxieControlWndClass`、`SandboxieControlBorderWindow`、`Sandboxie_BorderWindow`、`SandboxieActivationWindow`、`SandboxieStartMenuMessageOnlyWindowClass`、`SandboxieKmdUtilWindow`、`Sandboxie_UAC_WindowClass`、`Sandboxie_v4_Proxy_Console_Window`、`Sandboxie_DDE_ProxyClass2` | 各组件 | 窗口消息互串、`FindWindow` 命中错误对象 |
| 事件对象 | `Sandboxie_ConsoleReadyEvent_`、`Sandboxie_StartMenu_WorkArea_`、`Sandboxie_Delete_Sandbox_Session_` | `CrashSvc`、`Start.exe` | 跨产品误触发（如删会话、抢工作区） |
| COM/代理对象 | `Sandboxie_ComProxy`、`Sandboxie_ComProxy_ComServer`、`Sandboxie_GuiProxy`、`Sandboxie_GuiProxy_Console`、`Sandboxie_NetProxy`、`Sandboxie_UacProxy`、`Sandboxie_UserProxy` | `CrashSvc.exe` | 沙盒内 COM 代理**互相劫持**，可能串到另一个产品的沙盒 |
| 内核令牌组 | `SandboxieAllGroup` | `CrashDrv.sys`（`token.c`） | 令牌组共享，权限判定互相影响 |
| 登录用户 | `SandboxieLogon` | `CrashSvc.exe`（`DriverAssistSid.cpp`） | 两个产品共用一个本地登录账户 |
| WFP 对象 | `SbieFilter`、`SbieCallout`、`SbieSublayer` | `CrashDrv.sys`（`wfp.c`） | **两个驱动同时装时，第二个注册 WFP callout 会失败**（名字冲突）→ 网络隔离失效 |
| 配置文件 | `Sandboxie.ini` | 驱动/服务/管理器 | 便携模式同目录会互相覆盖 |
| Qt 配置目录 | `%LOCALAPPDATA%\Xanasoft\Sandboxie-Plus\Sandboxie-Plus.ini` | `SandMan.exe` | **UI 设置完全共用**（语言、选项、分组、密钥…） |
| 环境变量 | `SBIEHOME`、`%SbieHome%` | `UpdUtil`、`SandMan` | 环境变量互相污染 |
| 消息号 | `SBIE1101`…`SBIE%1`、`MSGID_SBIE_*` | `CrashMsg.dll`、`trace.c` | 日志/事件源分类混淆（本身无害，但与官方无法区分） |
| INI 键名 | `SbieCtrl_*`、`HideSbieProcesses`、`DisableSbieDll`、`NoSandboxieConsole/Desktop/RpcSs` | 全组件 | 配置项与官方同名同义 |
| 安装包身份 | `SandboxieShell`、Publisher `CN=Tonalio GmbH` | `SbieShellPkg.msix` | 与官方 MSIX 身份冲突，且未签名装不上 |

**结论**：这套改名只解决了"**同名文件不能共存 / 服务名不能重复 / 导出名不能冲突**"这三件事，
**没有做命名空间隔离**。所以它和官方 Sandboxie-Plus **在同一台机器上会互相干扰**（尤其是 WFP、单实例互斥、COM 代理、Qt 配置）。

### 2.4 没改名也无所谓的（196 条中的绝大多数）

全部是**展示文案**：`About Sandboxie-Plus`、`Sandboxie-Plus - Recovery Log`、
`This copy of Sandboxie-Plus is certified for: %1`、`Sandboxie Manager can not be run sandboxed!`、
`Install your Sandboxie-Plus support certificate`……以及 `CrashMsg.dll` 里成百上千条消息正文
（`The Sandboxie driver (SbieDrv) is not available.`、`Make sure both the driver and Sandboxie service (SbieSvc) have started successfully.`）。

> 这就是为什么用户看到的现象是：**文件全叫 Crash*，界面全叫 Sandboxie-Plus，报错信息里还让你去启动 SbieSvc**。

### 2.5 改名策略推断：为什么恰好是这 14 条

按"是否会导致加载/通信失败"排序，只有这几类**必须**改：

1. **文件名**（否则和官方安装目录混装会覆盖；也便于按哈希/文件名规避检测）
2. **服务名 / 驱动名**（`CreateService` 唯一键，不能重复）
3. **设备名**（`IoCreateDevice` 唯一）
4. **导出函数名前缀**（`GetProcAddress` 按名查找，且能避开基于导出名的特征）
5. **RPC 端点 + Host 注入 DLL 名**（沙盒内进程要按名字加载，必须跟着改，否则通信断）
6. **版本资源**（对外呈现"另一个产品"）

**不需要改**的（跨进程对象名、INI、环境变量、Qt 配置）就一律不动——省事，而且不影响单机单产品运行。
代价就是 §2.3 列的那些冲突。

### 2.6 改名不彻底导致的实际问题清单

| # | 问题 | 触发条件 | 影响 |
|---|---|---|---|
| 1 | WFP callout/filter/sublayer 重名 | 与官方 Sandboxie 同机 | 后装的那个网络隔离**注册失败**，`InternetAccessDevices` 等策略失效 |
| 2 | 单实例互斥体重名 | 同时启动两个管理器 | 第二个直接被判定"已在运行" |
| 3 | Qt 配置目录共用 | 同机 | 语言/选项/分组互相覆盖；卸载一个会带走另一个的设置 |
| 4 | `Sandboxie.ini` 同名 | 便携模式同目录 | 配置互相覆盖 |
| 5 | 报错文案指向不存在的 `SbieDrv`/`SbieSvc` | 任何故障 | 排障误导（用户会去找根本不存在的服务名） |
| 6 | `SBIEHOME` 环境变量共用 | 同机 | 路径解析可能指向另一个安装目录 |

### 2.7 品牌不一致清单（版本资源 / 文件名 / UI 三方打架）

| 组件 | 文件名 | 版本资源 | UI 文案 |
|---|---|---|---|
| `CrashDll.dll` | Crash | `my.163.com` / "My User Mode DLL" | 消息里写 "Sandboxie" |
| `CrashSvc.exe` | Crash | `my.163.com` / "My Service" | — |
| `CrashDrv.sys` | Crash | `my.163.com` / "My Kernel Mode Driver" | 日志前缀仍 `Sbie ...` |
| `SandMan.exe` | 未改 | **`sandboxie-plus.com` / "Sandboxie Manager" 1.16.5** | 全部 "Sandboxie-Plus" |
| `QSbieAPI.dll` | 未改 | **`sandboxie-plus.com` / "Sandboxie API for Qt"** | — |
| `SbieCtrl.exe` | 未改 | `my.163.com` / "My Control" | 仍 "Sandboxie Control" |
| `CrashMsg.dll` | Crash | `my.163.com` / "My Messages and Text" | 消息正文全 "Sandboxie"/"SbieDrv" |

> 改名脚本只覆盖了 `Sandboxie/` 主工程，**漏了 `SandboxiePlus/` 的 `SandMan.exe` 与 `QSbieAPI.dll` 的版本资源**。

---

## 3. 二进制层交叉验证（汇总）

| 项 | 结果 |
|---|---|
| 构建来源 | 全部 `D:\a\SandboxieCrack\SandboxieCrack\...`（GitHub Actions，仓库名 `SandboxieCrack`） |
| 例外 | `wtsapi32.dll` ← `D:\a\sbiekey\sbiekey\...`（另一个工程） |
| 构建时间 | 2026-09-01 06:41–06:45（`wtsapi32.dll` 为 2026-03-29，独立流水线） |
| 驱动签名 | `CN="WDKTestCert runneradmin,134327185649426872"` 自签、无时间戳、2036 到期、根不受信任 |
| 其他签名 | 全部**无** Authenticode |
| 导出表 | x64 `CrashDll` 142 个（其中 `CrashApi_*` 53）；x86 223 个（`CrashApi_*` 38，带 `@n` 修饰） |
| 新增导出 | `CrashApi_ActivateLicense*` / `QueryLicense*`（自有体系，忽略） |
| 残留导出 | 无任何 `SbieApi_` 前缀 |

---

## 4. 完整攻击链（一图流）

```
① 源码级：把驱动验签整条链路拔掉
   verify.c  ×4  ─┐
   util.c    ×2   ├─→ Verify_CertInfo = {永久, 最高级, 全功能, active}
   driver.c  ×1   │    FeatureFlags   = 全部置位
   api.c     ×2  ─┘    → 5 分钟杀进程、盒子类型限制、加密限制 全部失效
   DriverAssistStart.cpp ─→ 官方无法吊销证书
   MountManager.cpp      ─→ 加密卷/内存盘不限
② UI 级：g_CertInfo 变常量 + 管理器签名不校验 + 不更新 + 去掉中文区域锁
③ 道具：Certificate.dat = CONTRIBUTOR / SIGNATURE: NONE（给界面看）
④ 签名级：WDKTestCert 自签驱动 + wtsapi32.dll 关 DSE 加载
⑤ 商业化：另起一套 License.dat 卡密（本报告不展开）
```

---

## 附录 A：全树 diff 的 106 个文件如何分类

**A. 破解改动（8 个，已全部在 §1.2 列表）**

```
Sandboxie/core/drv/verify.c                (+16 / -108 行，净删 439 行)
Sandboxie/core/drv/api.c                   (+2 / -7)
Sandboxie/core/drv/util.c                  (+3 / -0)
Sandboxie/core/drv/driver.c                (+1 / -1)
Sandboxie/core/svc/DriverAssistStart.cpp   (+0 / -14)
Sandboxie/core/svc/MountManager.cpp        (+0 / -1)
SandboxiePlus/SandMan/SandMan.cpp           (202 行差异，其中证书段整段删除 ~130 行)
SandboxiePlus/SandMan/OnlineUpdater.cpp    (+2 / -6)
```

**B. 基线漂移（98 个，非二开改动）**

- 21 个 `sandman_*.ts` 翻译文件（上游在 tag 前又跑了一轮 lupdate）
- `Sandboxie/core/drv/syscall_win32.c` / `syscall_64.c` / `syscall.h` / `my_winnt.h`（Windows 版本偏移表更新）
- `SandboxiePlus/SandMan/Forms/SettingsWindow.ui`（`cmbGrouping` 等新控件）
- `Sandboxie/core/drv/process.c`（`Process_NotifyProcess_Create` 增删 `Name/NameLength` 参数、`Win32kFilter` 块）
- `Sandboxie/core/dll/net.c`（BindIP 可用性检查）、`config.c`、`key.c`、`proc.c`、`str_util.c`
- `Sandboxie/apps/control/*`（`BorderGuard`、`Box`、`BoxPage` 小改）
- `Sandboxie/install/SbieSettings.ini`（15 行新配置项）
- `Sandboxie/msgs/*`（4–5 行文案）
- 其余 `.rc` / `.h` / `.vcxproj` / `.pri` / `.cmd` / `README` / `CHANGELOG` 的零散差异

> 判断依据：这些改动**不含** `return TRUE` / 注释掉的校验 / 证书与签名相关逻辑，且方向上是"本地比官方旧"。

---

## 附录 B：复现命令

```bash
# 0) 取官方源码并全树 diff
curl -L -o official.zip https://codeload.github.com/sandboxie-plus/Sandboxie/zip/refs/tags/v1.16.5

# 1) 只看破解点（会打印所有含 return TRUE / Cert / Sign 的增删行）
python .workbuddy-ai/tmp/rename_audit.py          # 改名三态审计（429 条字面量）
python .workbuddy-ai/tmp/pe_dump.py  <Manager>    # PE 元数据/签名/导入导出

# 2) 证书解析是否被删（应为空）
strings -a  CrashDrv.sys | grep -E "^(SIGNATURE|TYPE|LEVEL|OPTIONS)$"

# 3) 黑名单是否被删（应为空）
strings -a  CrashSvc.exe | grep CertBlockList

# 4) 驱动签名
```

```powershell
Get-AuthenticodeSignature .\CrashDrv.sys | Format-List Status, SignerCertificate
```

---

*复核时间：2026-11-01 · 方法：官方 v1.16.5 全量源码全树 diff + Manager 二进制静态取证（PE/字符串/导出表/签名）+ 本机服务注册表核对。未运行被测程序。*
