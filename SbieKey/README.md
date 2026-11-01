# SbieKey/ —— wtsapi32.dll 侧加载代理（内联自 JeffyV/1sbiekey）

本目录只保留**编译 wtsapi32.dll 所需的源码**，让绿色包由一次 CI 构建产出，不再依赖外部
预编译产物。工程文件：`wtsapi32.vcxproj`（x64 Release → `Bin\x64\Release\wtsapi32.dll`，
`/SbieKey/Bin/` 已在 .gitignore 里）。

## 它做什么

```
CrashSvc.exe（LocalSystem 启动）静态导入 wtsapi32.dll
  └─ 程序目录下的这个 DLL 先被加载（DLL 侧加载）
       ├─ DllMain → InitProxy()：把 69 个 WTS 导出转发到 %SystemRoot%\System32\wtsapi32.dll
       ├─ MinHook 挂 ntdll!NtLoadDriver
       └─ 命中 kTargetDriverServices 里的驱动服务时（当前 = SbieDrv / LukDrv / CrashDrv）：
             ① PatchDriver(ImagePath)：清 PE Security 目录、
                把 ksecdd!BCryptVerifySignature 的调用点改成 `31 C0 C3`（xor eax,eax; ret）、
                CheckSumMappedFile 重算校验和（**不可逆**，改前会留 `*.sbiekey.bak`）
             ② DSEAction(DisableDSE)：内嵌 RTCore64.sys（MSI Afterburner 4.6.2，CVE-2019-16098）
                拿内核读写，把 ci!g_CiOptions 清零
             ③ 原 NtLoadDriver 真正加载驱动
             ④ DSEAction(RestoreDSE)：恢复 g_CiOptions
```

`CrashDrv` 这一项是本项目加的（原版只认 `SbieDrv` / `LukDrv`）：不加的话驱动仍能加载
（"关 DSE"那一步是无条件的），但盘上的 `CrashDrv.sys` 会留着 4032 字节的 WDKTestCert
测试签名目录，静态一看就知道是自签。

## 来源与构成

| 文件 | 说明 |
|---|---|
| `mainlib.cpp` | `DllMain` + `NtLoadDriver` 钩子 + `PatchDriver()`（本项目改过服务名表） |
| `proxy.c` / `proxyjmp.asm` | 69 个 WTS 导出的转发（asm 里是跳转桩，`#pragma comment(linker,"/EXPORT:...")` 声明导出） |
| `hook.c` / `trampoline.c` / `buffer.c` / `hde64.c` / `MinHook.h` | MinHook（BSD-2-Clause，Tsuda Kageyu） |
| `killdse.cpp/.h` | DSE 开关（`DSEAction`）、RTCore64 的加载/卸载 |
| `RTCore64.cpp/.h` | 内嵌的 RTCore64.sys 字节数组 |

未纳入本目录（属于安装器，与本绿色包无关）：`sbiekey.nsi`、`icon.ico`、`uires.dll`、
`msvcrt.lib`、`resource.h`（保留但不参与编译）。

## 上游独立工程

`D:\Coding\WorkSpace\SbieKey`（远端 `JeffyV/1sbiekey`）仍是这套源码的**开发主仓**
（NSIS 安装器、Lucky fork 集成、检验脚本都在那边）。本目录是它的**编译用子集**，
两边的 `mainlib.cpp` 需保持一致——改动请先改主仓，再同步过来。
