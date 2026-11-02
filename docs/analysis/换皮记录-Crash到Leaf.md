# 换皮记录：Crash\* → Leaf\*（2026-11-02 第二轮）

> 范围：把上一轮 `Sbie*` → `Crash*` 的全部品牌位点，整体推到 `Leaf*`。
> 依据：本目录《Manager 逆向对比-二进制取证》确立的改名清单（组件、服务、驱动、导出前缀、文件名）。
> 不变项：`SbieDll_*` 导出族、`SbieAPI` Qt 类、`Sandboxie` 产品文案、`SbieRelease`/`SbieDebug` 构建配置名、
> 二次开发的卡密/License 模块（按前一轮约定排除）。

---

## 1. 结果总览

| 项 | 数量 |
|---|---|
| 内容被修改的文件 | **266** |
| 标识符/字面量替换总数 | **2503** |
| 路径改名（目录 + 文件） | **11** |
| 品牌 token 残留 | **0** |
| 新旧品牌混用文件 | **0** |

## 2. 映射表

### 2.1 内容替换（按应用顺序，长 token 在前）

| 旧 | 新 | 命中 | 说明 |
|---|---|---|---|
| `CrashApi_` | `LeafApi_` | 1433 | 导出 API 前缀（x86 .def + 全树调用点） |
| `CrashDll` | `LeafDll` | 329 | 注入 DLL |
| `CrashSvc` | `LeafSvc` | 292 | 服务程序 / RPC 端口前缀 |
| `CrashDrv` | `LeafDrv` | 150 | 驱动与服务名 |
| `CrashMsg` | `LeafMsg` | 62 | 消息库 |
| `CrashRpcSs` | `LeafRpcSs` | 55 | COM 宿主 |
| `CrashHostDll` | `LeafHostDll` | 34 | 宿主注入 DLL |
| `CrashCrypto` | `LeafCrypto` | 32 | COM 宿主 |
| `CrashDcomLaunch` | `LeafDcomLaunch` | 29 | COM 宿主 |
| `CrashWUAU` | `LeafWUAU` | 28 | COM 宿主 |
| `CrashBITS` | `LeafBITS` | 24 | COM 宿主 |
| `CrashShellExt` | `LeafShellExt` | 18 | Shell 扩展 |
| `crashsvc` | `leafsvc` | 9 | 小写服务名 |
| `CrashManager` | `LeafManager` | 2 | 注释里的项目代称 |
| `Crash sandbox` | `Leaf sandbox` | 2 | 脚本提示语 |
| `L"Crash"` | `L"Leaf"` | 2 | 服务名拼装前缀（`ipc_start.c` / `ProcessServer.cpp`） |
| `crashdrv` | `leafdrv` | 1 | 小写驱动名 |
| `Crash* build` | `Leaf* build` | 1 | 注释 |

### 2.2 路径改名

```
Sandboxie/CrashHostDll/                          -> Sandboxie/LeafHostDll/
  ├ CrashHostDll.cpp/.h/.rc/.vcxproj             ->  LeafHostDll.*
SandboxiePlus/SbieShell/CrashShellExt/           -> SandboxiePlus/SbieShell/LeafShellExt/
  ├ CrashShellExt.vcxproj(.filters)              ->  LeafShellExt.vcxproj(.filters)
Sandboxie/core/drv/CrashDrv.inf                  -> LeafDrv.inf
Sandboxie/core/drv/CrashDrv_x32.ddf              -> LeafDrv_x32.ddf
Sandboxie/core/drv/CrashDrv_x64.ddf              -> LeafDrv_x64.ddf
```

## 3. 明确不动的部分

误改这些会改变语义或破坏兼容性，全部保留原样：

| 保留项 | 出现位置 | 理由 |
|---|---|---|
| `crashed` / `crashes` / `crash` / `crashing` | 全树 400+ 处 | 普通英文词 |
| `Attach Crash Dumps`、`Crash dump` | UI 文案、`.ts` 全部语言、ISSUE 模板 | Windows 崩溃转储术语 |
| `Dump_CrashHandlerExceptionFilter`、`MyCrashHandlerExceptionFilter` | `dump.c`、`DebugHelpers.cpp` | 异常过滤器函数名，非品牌 |
| `SystemCrashDumpInformation` 等 | `win32_ntddk.h`、`my_winnt.h` | 系统枚举值 |
| `opera_crashreporter`、`mono_crash` | 进程兼容名单 | 第三方标识符 |
| `SbieDll_*`（x64 77 个 / x86 46 个导出） | `sbiedll.h` 等 | 上一轮已确认 Manager 未改，保持 1:1 |
| `SbieRelease` / `SbieDebug` 配置名、`Sandboxie` 目录与文案 | 全树 | 与 PDB 路径和发行件一致 |

## 4. 验证方法与结果

本地无 Visual Studio（无 MSVC/msbuild），因此采用静态校验替代编译：

| # | 检查 | 结果 |
|---|---|---|
| 1 | `.sln` 的 `Project()` 工程路径 + `.vcxproj/.filters` 的 `Include` 条目 + `.ddf` 源文件列表 → 目标是否存在 | **断链 0**（唯一一条 `msgs\SbieRelease\msgs.h` 是构建生成物，改造前即如此） |
| 2 | x86 导出表 30 个 `LeafApi_*` → 源码是否有对应声明 | **缺失 0** |
| 3 | `#include "..."` 可解析性 | 未解析数与改造前**同源**（`common/defines.h` 这类靠 `AdditionalIncludeDirectories` 解析，非真实断链） |
| 4 | 品牌 token 残留 | **0 处** |
| 5 | 单文件内新旧品牌混用 | **0 个** |
| 6 | 误伤英文词（`leafed`/`leafes`/`leafdump`/`LeafHandler`） | **0 处** |
| 7 | 破解标记完好（`eCertEternal`、`arg("Hacker")`、`FeatureFlags`、`MyIsTestSigning`） | **全部保留** |
| 8 | 运行时名称一致性（`LeafSvcPort` ↔ `Templates.ini`、`LeafHostDll.dll` ↔ `HostInjectDll`、`LeafDrv` ↔ `.inf` / `kmdutil` / `SbieKey`） | **一致** |

审计原始数据见 [`assets/leaf_rename_audit.json`](assets/leaf_rename_audit.json)（266 个文件逐条命中明细）。

## 5. 已知遗留（本次未处理，可按需继续）

1. **产品名仍是二次开发方的值**：`Sandboxie/common/my_version.h` 里 `MY_PRODUCT_NAME_STRING "My"`、
   `MY_COMPANY_NAME_STRING "my.163.com"`——这是上一轮为了 1:1 对齐 Manager 版本资源保留的，不属于 `Crash*` 前缀，故未动。
2. **UI 文案仍是 Sandboxie-Plus**：窗口标题、关于框、`sandman_*.ts` 的界面字符串沿用上游品牌，上一轮同样未改。
3. **内部 `Sbie*` 命名**：`SbieIni`、`SbieShell`、`QSbieAPI`、`SbieDefs.h`、`SBIESVC_PORT` 宏名等属上游遗留，
   与 Manager 发行件保持一致，改名会破坏 1:1 对照。
4. **未编译验证**：本地无 MSVC，需在 CI 跑一次 `Build x64 (leaf, manual)` 才能确认链接与签名链路（见 §6）。

## 6. CI 验证入口

新增 [`.github/workflows/build-leaf.yml`](../../.github/workflows/build-leaf.yml)，与 `build.yml` 同一条构建链路，
区别只在触发方式与产物：

| 项 | 说明 |
|---|---|
| 触发 | **仅 `workflow_dispatch`**，push 不触发 |
| 分支 | 默认 `master_leaf`，可在 Run workflow 对话框里改 |
| 前置守卫 | 构建前全树扫 `Crash(Api_\|Dll\|Svc\|Drv\|Msg\|HostDll\|ShellExt\|BITS\|WUAU\|Crypto\|RpcSs\|DcomLaunch)`，命中即失败（跳过 `.github`/`docs`/`.workbuddy-ai`，且不匹配 crash dump 类英文词） |
| 后置校验 | 产物目录不得出现 `Crash*` 命名的二进制 |
| 产物 | artifact `Leaf-green-x64`，保留 30 天；**不发 release**（发布仍归 `build.yml` 的 master 流水线） |

> GitHub 只在**默认分支**（本仓库是 `master`）上显示 Run workflow 按钮。该文件目前只在 `master_leaf`，
> 合并到 `master` 之前需要用命令行触发：
>
> ```bash
> gh workflow run build-leaf.yml --ref master_leaf
> ```

顺带修掉了 `build.yml` 的一个既有笔误：`retention-days: 30` 原先误写在最后一步 `run: |` 的脚本体里，
会被 PowerShell 当成命令执行（因 `continue-on-error` 静默失败），现已移回 `upload-artifact` 的 `with:` 块。
