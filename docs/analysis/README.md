# 分析文档索引

本目录存放对 `CrashManager` 发行件（`D:\Jeffy\桌面\CrashManager沙盒\Manager`）的逆向分析、
本仓库改名改造的源码记录，以及配套的审计产物与验证截图。

**这些文件只用于记录，不参与任何构建**：`Sandboxie/*.sln`、`SandboxiePlus/*.pro`、
`Installer/*.iss` 均未引用本目录，MSBuild / qmake / Inno Setup 的输入不受影响。

---

## 阅读顺序

三份「差异分析」是同一课题的三个版本，后者修正前者，**请按顺序读，并以第三版为准**。

| 顺序 | 文件 | 定位 |
|---|---|---|
| 1 | [`Manager二次开发差异分析.md`](Manager二次开发差异分析.md) | 第一版。以 PE 结构、版本资源、导入导出表、配置 diff 为主，覆盖面最广 |
| 2 | [`Manager二次开发差异-深度复核.md`](Manager二次开发差异-深度复核.md) | 第二版。取得官方 v1.16.5 全量源码后做全树 diff，替代并补强第一版；文首自带勘误，标明 3 处结论已被第三版推翻 |
| 3 | [`Manager逆向对比-二进制取证.md`](Manager逆向对比-二进制取证.md) | 第三版。改以已发布二进制为唯一事实源（`.pdata` 函数枚举 + capstone 反汇编 + IAT 交叉引用），**权威版本** |
| 4 | [`源码改造记录-1比1.md`](源码改造记录-1比1.md) | 执行记录。依据第三版结论做的改名 + 版本资源改造，含 221 个源文件 / 2205 处替换的明细与验证方法 |
| 5 | [`1比1还原与三项改造-2026-11-01.md`](1比1还原与三项改造-2026-11-01.md) | 收尾轮。把上一轮过度改名的 4 个标识符 / 12 处回退到 1:1，并补齐 `Certificate.dat` / `License.dat` / `wtsapi32.dll` 三项附加物 |
| 6 | [`换皮记录-Crash到Leaf.md`](换皮记录-Crash到Leaf.md) | **最新一轮**。把 `Crash*` 全量推到 `Leaf*`：266 个文件 / 2503 处替换 / 11 处路径改名，含保护清单与验证结果 |

> 文中出现的 `D:\Coding\WorkSpace\SandboxieCrack` 是改造当时的工程路径，本仓库即由该目录演进而来。
>
> ⚠️ **命名时点提醒**：前 5 篇文档写于 `Crash*` 时代，文中的 `CrashSvc` / `CrashDll` / `CrashDrv` 等
> 在当前代码里已全部是 `LeafSvc` / `LeafDll` / `LeafDrv`。这些文档作为历史记录保持原貌未改，
> 对照代码时请按《换皮记录-Crash到Leaf》的映射表换算。

## assets/

审计与验证的原始产物，供复核文档结论使用，不参与构建。

| 文件 | 说明 |
|---|---|
| `rename_audit.json` | 改名三态审计原始数据，377 条含 `sbie`/`sandboxie` 的字面量逐条判定（`KEPT` 193 / `ABSENT` 178 / `BOTH` 6，`RENAMED` 为 0），对应《源码改造记录-1比1》§6.1 |
| `leaf_rename_audit.json` | `Crash*` → `Leaf*` 换皮的逐文件命中明细（266 个文件 / 2503 处），对应《换皮记录-Crash到Leaf》 |
| `沙箱运行确认-2026-11-02.png` | 改造后二进制的运行验证：`LeafRpcSs.exe` / `LeafCrypto.exe` 等进程在沙盘内「正在运行」（截图摄于改名前的 `Crash*` 时期） |
| `纯净单沙盘-2026-11-02.png` | 默认配置验证：`Sandboxie.ini` 只有 `DefaultBox` 一个沙盘，无历史残留 |
