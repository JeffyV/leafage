# Manager（CrashManager 沙盒）二次开发差异完整分析

> 对比对象
> - **基准（本项目）**：`D:\Coding\WorkSpace\SandboxieCrack` —— Sandboxie-Plus **5.71.5 / 1.16.5** 源码树（对应上游 tag `v1.16.5`）
> - **被分析对象**：`D:\Jeffy\桌面\CrashManager沙盒\Manager` —— 已编译的发行目录（60 个根文件 + 5 个子目录）
>
> 分析方法：PE 结构/版本资源/导入导出表解析、字符串与 PDB 路径取证、与上游默认配置逐行 diff、注册表服务状态核对、配套破解工程源码比对。所有结论均有可复现证据。

---

## 0. 结论速览

| 维度 | 结论 |
|---|---|
| 代码基线 | 与本项目**同一份 5.71.5 源码**，未做上游合并或功能分支 |
| 主要改造 | ① 全套组件**改名换牌**（Sbie*/Sandboxie* → Crash*）② 植入**自有商业授权系统**（设备码 + 租赁 + TPM/HWID 绑定）③ 用**代理 DLL + BYOVD 关 DSE** 让测试签名驱动能加载 |
| 改名完整度 | 文件/服务/驱动/设备/导出 API **改得比较干净**；但 UI 文案、窗口类、互斥体、INI 名、部分消息文本**完全没改** |
| 品牌一致性 | **混乱**：文件名 Crash*、版本资源 `my.163.com`/"My"，但界面仍是 "Sandboxie-Plus"、关于框仍是 xanasoft |
| 信任链 | 驱动为 **WDKTestCert 自签测试证书**，其余全部**未签名**；靠关 DSE 加载，非正常签名路径 |
| 授权机制 | 上游 supporter certificate 被**伪造为 CONTRIBUTOR / SIGNATURE: NONE** 绕过；叠加一套自建 `License.dat` 激活体系（明文 HTTP） |
| 配置层 | `SbieSettings.ini` 与上游**逐字节一致**；`Templates.ini` 仅 4 行改名；`Sandboxie.ini` 是**运行期实例配置**（300 个预建沙盒 + 磁盘序列号伪造） |
| 完整性缺陷 | MSIX 未签名且指向不存在的 `SbieShell.exe`；`Sandboxie.ini` 含 **13 个二进制根本不认识的配置键**（无效配置） |

一句话：**这不是"二次开发"，是"改名 + 加壳式授权 + 破解上游许可"的发行版**。真正新增的代码量很小（授权模块 + 一个外部代理 DLL），绝大部分是字符串替换和资源重写。

---

## 1. 基线确认：Manager 到底从哪来的

所有自研组件的 PDB 路径都指向同一个 CI 构建根：

```
D:\a\SandboxieCrack\SandboxieCrack\Sandboxie\Bin\x64\SbieRelease\CrashDrv.pdb
D:\a\SandboxieCrack\SandboxieCrack\Sandboxie\Bin\x64\SbieRelease\CrashSvc.pdb
D:\a\SandboxieCrack\SandboxieCrack\SandboxiePlus\Bin\x64\Release\SandMan.pdb
D:\a\SandboxieCrack\SandboxieCrack\SandboxieTools\x64\Release\ImBox.pdb
```

- `D:\a\` 是 GitHub Actions Windows runner 的固定工作目录；仓库名 = `SandboxieCrack`，与本地目录同名。
- 说明 Manager 是**从本项目的改名分支在 CI 上整体重编译**得到的，不是对官方二进制打补丁。
- 唯一例外：`wtsapi32.dll` 来自**另一个工程** `D:\a\sbiekey\sbiekey\x64\Release\wtsapi32.pdb`（源码在本机 `D:\Coding\WorkSpace\SbieKey`，远端 `git@github.com:JeffyV/1sbiekey.git`）。
- 本地这份 `SandboxieCrack` 源码里**搜不到任何 `Crash*` 字样**（已用全库 grep 验证），所以改名是在另一份分支/脚本里完成的，当前目录是"干净的上游参照物"。

构建时间戳：除 `wtsapi32.dll`（2026-03-29）外，其余全部为 **2026-09-01 06:41–06:45**，一次 CI 产出。

---

## 2. 文件级对照表

### 2.1 被改名的 11 个组件

| 上游产物（本项目） | Manager 实际文件 | 说明 |
|---|---|---|
| `SbieDll.dll` | `CrashDll.dll`（x64 + `32\`） | 导出前缀 `SbieApi_` → `CrashApi_` |
| `SbieSvc.exe` | `CrashSvc.exe`（x64 + `32\`） | 服务名 `CrashSvc` |
| `SbieDrv.sys` | `CrashDrv.sys` | 驱动服务名 `CrashDrv`，设备 `\Device\CrashDrv` |
| `SbieMsg.dll` | `CrashMsg.dll` | 消息库（3.3 MB） |
| `SboxHostDll.dll` | `CrashHostDll.dll` | Host 注入 DLL，导出 `InjectDllMain` |
| `SbieShellExt.dll` | `CrashShellExt.dll` | WinRT 右键菜单扩展 |
| `SandboxieBITS.exe` | `CrashBITS.exe` | 沙盒内 COM 服务宿主 |
| `SandboxieCrypto.exe` | `CrashCrypto.exe` | 同上（CryptSvc） |
| `SandboxieDcomLaunch.exe` | `CrashDcomLaunch.exe` | 同上（DCOM） |
| `SandboxieRpcSs.exe` | `CrashRpcSs.exe` | 同上（RPC） |
| `SandboxieWUAU.exe` | `CrashWUAU.exe` | 同上（wuauserv） |

### 2.2 未改名的组件（但**全部重编译过**）

`SandMan.exe`、`SbieCtrl.exe`、`SbieIni.exe`、`Start.exe`、`KmdUtil.exe`、`UpdUtil.exe`、`ImBox.exe`、`QSbieAPI.dll`、`MiscHelpers.dll`、`UGlobalHotkey.dll`、`qtsingleapp.dll`

### 2.3 关键文件指纹

| 文件 | 大小 | SHA256(前16) | PE 时间戳 | 签名 |
|---|---|---|---|---|
| `CrashDrv.sys` | 267,712 | a50f124cd878cf68 | 2026-09-01 06:42 | **有（自签测试证书）** |
| `CrashSvc.exe` | 451,584 | b96e80c918d6fa84 | 2026-09-01 06:42 | 无 |
| `CrashDll.dll` | 945,664 | 1308171ad08ed215 | 2026-09-01 06:42 | 无 |
| `32\CrashDll.dll` | 763,392 | bd7ebafa05c397b7 | 2026-09-01 06:41 | 无 |
| `CrashMsg.dll` | 3,322,368 | 8376cae3c75e4071 | 2026-09-01 06:41 | 无 |
| `CrashHostDll.dll` | 133,632 | 0451873d279ded27 | 2026-09-01 06:42 | 无 |
| `CrashShellExt.dll` | 65,024 | c47020b922c843be | 2026-09-01 06:45 | 无 |
| `SandMan.exe` | 3,920,896 | 6d05ac75237b57f5 | 2026-09-01 06:45 | 无 |
| `SbieCtrl.exe` | 3,394,560 | 2d846725e1e17f2e | 2026-09-01 06:42 | 无 |
| `QSbieAPI.dll` | 697,344 | 0221ef0772f32ce4 | 2026-09-01 06:43 | 无 |
| `MiscHelpers.dll` | 862,720 | bc7bcc46d17ac145 | 2026-09-01 06:43 | 无 |
| `UpdUtil.exe` | 169,472 | 73111cd633800cd5 | 2026-09-01 06:45 | 无 |
| `ImBox.exe` | 178,176 | 99c51cdbfa28c7f2 | 2026-09-01 06:45 | 无 |
| `wtsapi32.dll` | 162,304 | 817229042b1b2802 | **2026-03-29** | 无 |

> 注：`ImBox.exe` 是上游自带的"加密沙盒容器"工具（ImDisk + DiskCryptor 加密卷），**不是**二开新增。
> Qt 6.8.3 / MSVC 14.44 / 7-Zip 23.01 / OpenSSL 3.4.0 均为官方运行时，未改动。

---

## 3. 改名体系（完整映射）

### 3.1 已成功改名的标识符

| 类别 | 上游 | Manager |
|---|---|---|
| 服务 | `SbieSvc` | `CrashSvc` |
| 驱动服务 | `SbieDrv` | `CrashDrv` |
| 驱动设备 | `\Device\SbieDrv` | `\Device\CrashDrv` |
| RPC 端点 | `\RPC Control\SbieSvcPort` | `\RPC Control\CrashSvcPort` |
| 用户态 DLL | `SbieDll.dll` | `CrashDll.dll` |
| Host 注入 | `SboxHostDll.dll` | `CrashHostDll.dll` |
| 消息库 | `SbieMsg.dll` | `CrashMsg.dll` |
| 导出 API | `SbieApi_QueryConf` …（全量） | `CrashApi_QueryConf` …（x64 53 个 / x86 38 个） |
| COM 代理宿主 | `SandboxieRpcSs.exe` 等 | `CrashRpcSs.exe` 等 |
| Shell 扩展注册 | `HKCU\...\Sandboxie-Plus\SbieShellExt\Lang` | `...\Sandboxie-Plus\CrashShellExt\Lang` |

配套改动：`Templates.ini` 里 4 行同步改名（见 §6.1），否则沙盒内 RPC 与 Host 注入会断。

### 3.2 没改干净的残留（重要）

| 残留项 | 出现在 | 后果 |
|---|---|---|
| 消息文本仍写 `SbieDrv` / `SbieSvc` / `Sandboxie` | `CrashMsg.dll` | 用户看到 "The Sandboxie driver (SbieDrv) is not available"，与实际文件名对不上，排障误导 |
| 界面全部仍叫 **Sandboxie-Plus** | `SandMan.exe` | 关于框、向导、证书页、更新检查、所有超链接指向 sandboxie-plus.com |
| 窗口类 `SandboxieControlWndClass`、互斥体 `Sandboxie_SingleInstanceMutex_Control` | `QSbieAPI.dll`、`SbieCtrl.exe` | 与官方版**同机冲突**（单实例互斥、窗口消息互通） |
| INI 文件名仍为 `Sandboxie.ini` | 驱动/服务/管理器 | 与官方版共用同一配置文件名，便携模式同目录会互相覆盖 |
| `QSbieAPI.dll` 版本资源仍是 `sandboxie-plus.com` / "Sandboxie API for Qt" | — | 品牌露馅 |
| `SandMan.exe` 版本资源仍是 1.16.5 / sandboxie-plus.com | — | 品牌露馅（只有 Crash* 系列改成了 `my.163.com`/"My"） |
| `SbieCtrl.exe`、`SbieIni.exe`、`Start.exe`、`KmdUtil.exe` 文件名未改 | — | 与"Crash"命名体系不一致 |

**判断**：改名是通过"源码里替换 SANDBOXIE/SBIE 宏 + 改 .def 导出名 + 改 .rc 资源"完成的，**没有做字符串级全量替换**，所以只覆盖了"会互相干扰"的部分（文件名、服务名、设备名、导出名、RPC 端口），而纯展示文本一律放过。

### 3.3 版本资源重写

所有 `Crash*` 组件被统一改写为：

```
CompanyName      : my.163.com
ProductName      : My
FileDescription  : My User Mode DLL / My Service / My Kernel Mode Driver / My Messages and Text / My Control / My Start / My COM Services (BITS|CryptSvc|DCOM|RPC|wuauserv) / My configuration file utility
InternalName     : CrashDll / CrashSvc / CrashDrv / CrashMsg / Control / Start / kmdutil …
OriginalFilename : CrashDll.dll / CrashSvc.exe / …
FileVersion      : 5.71.5
LegalCopyright   : Copyright © 2020-2025        ← 上游版权声明被截断
```

`SandMan.exe` 和 `QSbieAPI.dll` **没有**这套资源（仍是 sandboxie-plus.com / 1.16.5），说明改名脚本只处理了 `Sandboxie/` 主工程，漏了 `SandboxiePlus/` 的两个核心模块。

---

## 4. 新增功能一：自建商业授权系统

这是本次二开**唯一实质性新增代码**（其余都是改名）。

### 4.1 新增导出 API

`CrashDll.dll` 在原有 53 个 `CrashApi_*` 之外，多出 5 个上游不存在的接口：

```
CrashApi_ActivateLicense          // 设备码激活
CrashApi_ActivateLicenseBound     // 硬件绑定激活
CrashApi_ActivateLicenseLease     // 租赁（订阅）激活
CrashApi_QueryLicense             // 查询授权状态
CrashApi_QueryLicenseChallenge    // 取激活挑战/设备码
```

`QSbieAPI.dll` 里对应新增了 Qt 封装 `CSbieAPI::ActivateLicense()` / `QueryLicense()`，`SandMan.exe` 里有配套的激活对话框。

### 4.2 授权文件与协议

在 `CrashSvc.exe` 中提取到的完整协议要素：

| 要素 | 值 |
|---|---|
| 授权文件 | `<安装目录>\License.dat`（写入时用 `License.dat.tmp` 原子替换） |
| 文件头魔数 | `Sandboxie product activation` |
| 激活码格式 | `SBXD2-…`，字符集 `0123456789ABCDEFGHJKMNPQRSTVWXYZ`（Crockford Base32，去易混字符） |
| 请求标识 | `SandboxieCrashLicenseRequestKeyV1` |
| 授权服务器 | `http://223.247.195.3:8848/api/sandbox-license/v1/lease`（**明文 HTTP + 裸 IP**） |
| 覆盖方式 | 环境变量 `SANDBOX_LICENSE_SERVER_URL`，或 INI 键 `LicenseServerUrl` |
| 协议头 | `POST` + `Content-Type: application/octet-stream` + `Sandboxie-License/1.0` + `Cache-Control: no-store` |
| 密钥存储 | `ncrypt.dll` + `Microsoft Platform Crypto Provider`（**TPM 2.0**）/ `Microsoft Software Key Storage Provider` |
| 过期处理 | `"%s" expire` |

### 4.3 硬件绑定在驱动层

`CrashDrv.sys` 里出现上游没有的 SMBIOS 字符串：

```
SYSTEM SERIAL NUMBER / BASE BOARD SERIAL NUMBER / SERIAL NUMBER
SYSTEM PRODUCT NAME / CHASSIS ASSET TAG
TO BE FILLED BY O.E.M. / TO BE FILLED BY OEM / DEFAULT STRING
```

说明"设备指纹"是**内核态采集**的（比用户态 WMI 更难伪造），配合 `ActivateLicenseBound` 做一机一码。

### 4.4 授权与功能限制

`SandMan.exe` 中的新增 UI 文案（原文）：

- `This copy must be activated before Sandboxie can be used. Send the device code to the issuer, then paste the activation code below.`
- `Activation was rejected. Check the code and the license-server network connection. Error: %1`
- `The service accepted the request but the driver did not report an active license.`
- `Your license permits at most %1 sandboxes. Delete an existing sandbox or use a license with a higher limit.`

即：**不激活不能用；沙盒数量按授权档位限制**（默认配置里预建了 300 个沙盒，对应高档位）。

### 4.5 与上游 supporter certificate 的关系

上游的 `Certificate.dat` 被伪造为：

```
NAME: Administrator
TYPE: CONTRIBUTOR
SIGNATURE: NONE
```

`SIGNATURE: NONE` 在上游代码里是**校验失败**的形态（`verify.c` 会用内嵌公钥验签）。此构建能接受它，说明驱动源码里 `verify.c` 的验签分支被改过（或返回值被强制为成功，见 §5）。于是形成**双授权并存**：

- 上游证书体系 → 被伪造文件"绕过"，解锁全部 Privacy Enhanced / 高级盒子类型；
- 自建 `License.dat` → 真正卡住启动与沙盒数量，是商业收费点。

---

## 5. 新增组件二：`wtsapi32.dll`（BYOVD 加载器）

这是整个发行版**最危险、也最能说明性质**的部分。

### 5.1 为什么能进到系统服务里

`CrashSvc.exe` 的静态导入表里**有 `wtsapi32.dll`**（上游 `SbieSvc.exe` 同样有）。Windows 的 DLL 搜索顺序优先取"应用目录"，而 Manager 目录里就放了一个**自制的 `wtsapi32.dll`** —— 于是每次服务启动，这个 DLL 都以 **LocalSystem 权限**被加载进服务进程。这是典型的 **DLL 侧加载（side-loading）**。

它导出全部 69 个 WTS API（`WTSQueryUserToken`、`WTSEnumerateSessionsW` …），内部转发到 `%SystemRoot%\System32\wtsapi32.dll`，所以功能上完全无感。

### 5.2 它在 DllMain 里做了什么

配套源码在本机 `D:\Coding\WorkSpace\SbieKey\mainlib.cpp`，逻辑如下：

```
DllMain(DLL_PROCESS_ATTACH)
 ├─ InitProxy()                       // 装载真正的 system32\wtsapi32.dll，转发 69 个导出
 ├─ MinHook: 挂钩 ntdll!NtLoadDriver
 └─ 等待驱动加载调用
      NtLoadDriver(\Registry\Machine\System\CurrentControlSet\Services\<SbieDrv|LukDrv>)
       ├─ ① PatchDriver(ImagePath)   // 就地改写磁盘上的 .sys：
       │      · 清空 IMAGE_DIRECTORY_ENTRY_SECURITY（剥掉签名）
       │      · 把 .text 里对 ksecdd!BCryptVerifySignature 的间接调用改成
       │        31 C0 C3  (xor eax,eax; ret)  → 证书校验恒返回 0（成功）
       │      · CheckSumMappedFile() 重算 PE 校验和
       ├─ ② DSEAction(DisableDSE)    // 内嵌 RTCore64.sys 字节数组落地并加载
       │      · 用 MSI Afterburner 4.6.2 的 RTCore64 漏洞驱动（CVE-2019-16098）
       │        拿任意内核读写
       │      · 按系统版本定位 ci!g_CiEnabled (Vista/7) 或 ci!g_CiOptions (Win8+)
       │        并清零 → 关闭驱动签名强制
       ├─ ③ 原始 NtLoadDriver()      // 此时测试签名驱动可正常加载
       └─ ④ DSEAction(RestoreDSE)    // 恢复 g_CiOptions
```

关键字符串实证（`wtsapi32.dll` 内，UTF-16）：

```
RTCore64 v4.6.2 (from MSI Afterburner v4.6.2 Build 15658)
\\.\RTCore64        \RTCore64.sys       RTCore64
NTOSKRNL.EXE        CI.DLL              g_CiEnabled / g_CiOptions
\Registry\Machine\System\CurrentControlSet\Services\SbieDrv
SeLoadDriverPrivilege / SeDebugPrivilege / NtLoadDriver / NtUnloadDriver
```

### 5.3 与本次发行版的一处错配

`kTargetDriverServices = { "SbieDrv", "LukDrv" }` —— 代理 DLL 只对**官方 Sandboxie（SbieDrv）**和**Lucky 分支（LukDrv）**做"剥签名 + 校验短路"，**并不包含本产品的 `CrashDrv`**。

实际机器上的服务是（已核对注册表）：

```
CrashDrv   ImagePath=\??\C:\Users\Admin\Desktop\Manager\CrashDrv.sys   Type=1  Start=3
CrashSvc   ImagePath="C:\Users\Admin\Desktop\Manager\CrashSvc.exe"     Type=16 Start=2
SbieDrv    未安装        RTCore64   未安装（运行后已清理）
```

也就是说：
- **关 DSE 这一步对所有驱动生效**（它是无条件执行 `DSEAction` 的），所以 `CrashDrv` 仍能加载；
- 但**"短路 BCryptVerifySignature"这一步对 `CrashDrv` 不生效** → 该驱动的证书校验必须在**源码层**就被去掉，才能接受 `SIGNATURE: NONE` 的 `Certificate.dat`。

这也解释了为什么 `CrashDrv.sys` 里 `BCryptVerifySignature` 的导入还在、却仍能通过：**验签逻辑被改成恒真**，而不是靠运行时打补丁。

### 5.4 相关工程与部署脚本

`D:\Coding\WorkSpace\SbieKey`（远端 `JeffyV/1sbiekey`，分支 `lucky-integration`）里还有：

- `killdse.cpp/h`（DSE-Patcher，GPL，Kai Schtrom 2022）、`RTCore64.cpp`（BYOVD 内核读写）、`hook.c`/`trampoline.c`（MinHook 本体）、`proxy.c`（WTS 转发）
- `sbiekey.nsi`（NSIS 安装器）：解析安装目录 → **停服务** → **写 `Certificate.dat`** → **改 hosts 屏蔽 sandboxie 域名** → **关闭 Sandboxie 开机自启** → 再启动服务
- `docs/LUCKY_INTEGRATION.md`：完整的原理与验证文档（也自认"RTCore64 在微软漏洞驱动黑名单上，Win11 22H2+ 默认强制黑名单会失败"）

> 本机 hosts 当前是干净的（21 行，无 sandboxie 条目），说明屏蔽要么未执行、要么已被清理。

---

## 6. 配置层差异

### 6.1 `Templates.ini`：仅 4 行改动（其余 4545 行完全一致）

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

这是**改名必须配套**的改动，漏了会导致沙盒内进程无法与宿主通信。

### 6.2 `SbieSettings.ini`：0 差异

`diff` 归一化换行后**完全相同**（6521 行）。即配置项定义、默认值、说明文档全部沿用上游，没有为二开新增任何"正式"配置项。

### 6.3 `Sandboxie.ini`：这是**运行期实例配置**，不是默认配置

- 编码 UTF-16LE，706 KB，16,077 行，**305 个 section**
- 结构：`[GlobalSettings]` + 3 个 `[UserSettings_<SID>]` + `[DefaultBox]` + `[1]` … `[300]`
- 300 个数字沙盒：`[1]`–`[32]` 各自独立微调，`[33]`–`[300]` 完全同构（268 个共用一套模板），共 **33 种配置组合**

关键 `[GlobalSettings]`：

```ini
FileRootPath=D:\Sandbox\%SANDBOX%          ← 便携模式，沙盒根目录在 D 盘
KeyRootPath=\REGISTRY\USER\Sandbox_%USER%_%SANDBOX%
DefaultBox=DefaultBox
MarkOfTheWebBox=DefaultBox
OpenFilePath=D:\My\MyLauncher_x64r.exe     ← 放行宿主启动器
OpenFilePath=D:\myfz\source\HardwareSpoof.exe   ← 放行硬件伪造工具
OpenIpcPath=C:\Program Files (x86)\SogouInput*  ← 搜狗输入法
```

每个沙盒的典型配置（70 项）体现产品定位——**多开 / 反检测环境**：

```ini
UsePrivacyMode=y / UseSecurityMode=y / ConfidentialBox=y
BlockWindowEnum / BlockDriverQuery / HideSandboxTrace / HideSystemModules
HideFirmwareInfo / RandomRegUID / BlockScreenCapture / FakeAdminRights
BlockRemoteQuery / BlockRegistryTrace / HideLogicalDrives / NoTraceProcess
InjectDll64=\addons\SbieHide\SbieHide.x64.dll        ← 隐藏沙盒痕迹插件
DiskSerialNumber=HarddiskVolume0,D007-1D5F            ← 逐盒伪造磁盘序列号
DiskSerialNumber=HarddiskVolume1,FBB5-B0B8            ← （每个沙盒不同，1~9 卷）
BorderColor=#0423ee,off,6,192,in,6
```

### 6.4 ⚠ 13 个"无效配置键"

`Sandboxie.ini` 中以下键，在 `CrashDrv.sys` / `CrashDll.dll` / `CrashSvc.exe` / `SandMan.exe` / `CrashMsg.dll` 里**全部找不到**（ASCII/UTF-16 双查），也**不在上游 5.71.5 与最新 1.19.0 的 `SbieSettings.ini`** 中：

```
BlockDriverQuery  BlockLLMNR  BlockNetBIOS  BlockRegistryTrace
BlockRemoteQuery  BlockWindowEnum  DisableTaskQuery  HideLogicalDrives
HideProcessIntegrity  HideSandboxTrace  HideSystemModules
IsolateHostNetwork  NoTraceProcess
```

对照验证：同为"反检测"类的上游真键（`BlockScreenCapture`、`RandomRegUID`、`HideNonSystemProcesses`）都能在 `CrashDll.dll` 中找到 → 检索方法有效。

**结论**：这 13 项来自**另一个分支/另一个同类产品**的配置，本构建**静默忽略**。也就是说这份配置里的"反检测"能力有相当一部分是**心理安慰**，实际并未生效。

---

## 7. 完整性与可用性缺陷

| 问题 | 证据 | 影响 |
|---|---|---|
| MSIX 未签名 | `SbieShellPkg.msix` 仅 3 个文件（Manifest/BlockMap/ContentTypes），**无 `AppxSignature.p7x`** | Windows 无法安装未签名 MSIX |
| MSIX 指向不存在的宿主 | Manifest 里 `Executable="SbieShell.exe"`，目录中**无此文件** | 即使装上也无法启动 |
| MSIX 身份仍是上游 | `Identity Name="SandboxieShell"`，Publisher `CN=Tonalio GmbH`（上游发行者） | 与改名后的组件身份不匹配 |
| 无安装/卸载程序 | 目录内无 `Sandboxie-Plus.exe`/`uninst*` | 只能手工注册服务；升级/卸载靠外部脚本 |
| 无 PDB | 上游发行包会带 `SbieDrv.pdb`/`SbieDll.pdb` | 崩溃分析能力缺失 |
| 无 `Sandboxie-Plus.ini` | 便携模式本应生成 | 首次启动会重建 |
| `BoxGrouping` 引用越界 | 分组里列到 `304`、`DefaultBox`，实际只有 `1..300` | 界面上可能出现空分组 |

---

## 8. 信任链与安全评估

### 8.1 签名状况

```
CrashDrv.sys  →  签名存在，但签名者 = CN="WDKTestCert runneradmin,134327185649426872"
                 自签（Issuer 同 Subject）、无时间戳、有效期 2026-09-01 → 2036-09-01
                 校验结果：UnknownError（根证书不受信任）
其余全部文件   →  无任何 Authenticode 签名
```

上游官方发行版是 **EV 代码签名 + 微软 attestation 签名的驱动**，可在开启 Secure Boot / HVCI 的机器上正常加载。本构建完全不具备这个条件，只能靠 §5 的 BYOVD 关 DSE。

### 8.2 风险清单（客观陈述）

| 风险 | 说明 |
|---|---|
| **BYOVD** | 加载微软已列入黑名单的 `RTCore64.sys`（CVE-2019-16098）做任意内核读写；Win11 22H2+ 默认强制黑名单 + 开 HVCI 时**必然失败**，且会触发 Defender/EDR 告警 |
| **DLL 侧加载** | 用同名 `wtsapi32.dll` 劫持系统服务的 DLL 加载；任何能写安装目录的进程都可替换该 DLL → **以 LocalSystem 权限执行任意代码** |
| **测试签名驱动** | 驱动无有效签名，依赖关闭全局 DSE；关 DSE 期间系统整体处于"任意驱动可加载"状态 |
| **授权服务器明文 HTTP** | `http://223.247.195.3:8848`，无 TLS；激活码/设备码/租约可被中间人窃取或伪造 |
| **内核采集硬件指纹** | 读 SMBIOS 序列号做绑定，属于较敏感的硬件标识采集 |
| **GPLv3 合规** | 上游为 GPLv3。改名分发 + 移除/绕过许可证校验 + 新增闭源授权模块 + 未提供对应源码，**不符合 GPLv3 的源码提供与修改标注要求** |
| **同机冲突** | 窗口类/互斥体/INI 名未改，与官方 Sandboxie 同机安装会互相干扰（单实例互斥、配置互相覆盖） |

---

## 9. 与"上游升级"的耦合度（维护成本）

如果后续要把基线从 5.71.5 升到 5.73/5.74，需要重做的改动量：

| 改动面 | 数量 | 升级时是否自动继承 |
|---|---|---|
| 文件名/工程 TargetName | 11 个产物 | 否，需重跑改名脚本 |
| 导出符号 `.def`（`SbieApi_`→`CrashApi_`） | 2 个文件（x86/x64） | 否 |
| 服务名/设备名/端口（源码宏） | `SBIE`/`SANDBOXIE` 宏一处 + 若干硬编码 | 部分 |
| 版本资源 `.rc` | 11 个 | 否 |
| `Templates.ini` | 4 行 | 否（每次上游更新模板都要重打） |
| 授权模块（新增 5 个 API + 服务端协议 + 驱动 HWID 采集） | 需 rebase | **冲突高发区**（`SbieSvc`/`SboxDll.def`/`verify.c` 上游改动频繁） |
| 破解点（`verify.c` 验签恒真） | 1 处 | 每次上游改 `verify.c` 都要重打 |

**建议**（工程视角）：把改名做成构建期生成（脚本按映射表改 `.def`/`.rc`/宏，而不是手工 sed），把授权放用户态服务 + 服务端，别改内核；驱动签名走 EV/attestation 或微软 Partner Center，彻底去掉 BYOVD 依赖。

---

## 10. 附：可复现核对命令

```powershell
# 1) 版本资源 / 签名
Get-AuthenticodeSignature .\CrashDrv.sys | Format-List Status,SignerCertificate

# 2) 服务与驱动注册
Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\CrashDrv
Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\CrashSvc

# 3) 导出表对比（CrashApi_ 前缀）
dumpbin /exports CrashDll.dll | findstr CrashApi_

# 4) 字符串取证（授权服务器 / BYOVD）
strings -el wtsapi32.dll | findstr /i "RTCore64 g_CiOptions SbieDrv"
strings -el CrashSvc.exe | findstr /i "SBXD2 License.dat lease"

# 5) 配置差异（归一化换行）
git diff --no-index --strip-trailing-cr Sandboxie\install\Templates.ini ..\Manager\Templates.ini
```

---

*报告生成时间：2026-11-01。所有二进制结论均基于静态分析（PE 解析 + 字符串/PDB 取证 + 注册表状态核对），未运行被测程序。*
