# pack/ —— 绿色包附加物（对照 Manager 发行目录的三项 + 一个配件）

对照物：`D:\Jeffy\桌面\CrashManager沙盒\Manager`（64 个文件）。
Manager 比 `copy_build.cmd` 的产物多出 4 类东西，本目录负责把它们补齐：

| 文件 | 来源 | 说明 |
|---|---|---|
| `Certificate.dat` | 本目录（已提交） | 与 Manager 完全逐字节相同：`NAME: Administrator` / `TYPE: CONTRIBUTOR` / `SIGNATURE: NONE`（55 B，CRLF，无尾换行）。驱动侧 `KphValidateCertificate` 已硬编码 `0xF000E401`，**不读这个文件**；UI 侧 `g_CertInfo` 也硬编码，故它只是"给界面看的道具"，保持 1:1 反而不引入差异。 |
| `License.dat` | 本目录（已提交） | **自建**，不是 Manager 那份 DPAPI 密文。Manager 的 `License.dat` = `CryptProtectData` 密文（provider GUID `DF9D8CD0-1501-11D1-8C7A-00C04FC297EB`，description `Sandboxie product activation`），只能在该开发者的机器/账户上解出；本产品**没有任何卡密代码**（无 `SBXD2-`、无 `LicenseServerUrl`、无 5 个 `CrashApi_*License*` 导出），因此本文件是纯标记，格式自定：<br>`MY-LICENSE/1` + 纯文本字段，`ACTIVATION: offline`。任何组件都不会读它，联网授权链路为零。 |
| `wtsapi32.dll` | **仓库内 `SbieKey\` 源码编译**（CI 每轮自动产出，见下） | 代理 DLL：转发 69 个 WTS 导出 + MinHook 挂 `NtLoadDriver` + 剥目标驱动签名 + RTCore64 BYOVD 临时关 DSE。绿色包若要在一台没开测试签名的机器上加载自签的 `CrashDrv.sys`，就必须带它。 |
| `addons/SbieHide/*.dll` | SbieHide 官方发行件（CI 下载，见下） | Manager 里带着 `addons\SbieHide\SbieHide_x64.dll`(132,608 B) / `SbieHide_x86.dll`(96,256 B)，`Sandboxie.ini` 用 `InjectDll64=\addons\SbieHide\SbieHide.x64.dll` 引用它。 |

## wtsapi32.dll 怎么来

**源码已内联到本仓库 `SbieKey\`**（从独立工程 `JeffyV/1sbiekey` 拷入，只保留编译所需文件），
工程文件是 `SbieKey\wtsapi32.vcxproj`，CI 在 `Merge build output` 之前编译并投放：

```yaml
- name: Build WTS proxy - x64
  run: msbuild /t:build SbieKey\wtsapi32.vcxproj /p:Configuration=Release /p:Platform=x64 -maxcpucount:8
- name: Stage WTS proxy into pack
  # 拷 SbieKey\Bin\x64\Release\wtsapi32.dll -> Installer\pack\wtsapi32.dll
```

本地有 VS2022 时也可以一键（不需要那个独立仓库了）：

```cmd
Installer\pack\build_wtsapi32.cmd x64
```

产物落在 `SbieKey\Bin\x64\Release\wtsapi32.dll`（`/SbieKey/Bin/` 已进 .gitignore，运行产物不入库）。
`copy_build.cmd` 末尾把 `pack\wtsapi32.dll` 拷进绿色包。

已按本产品改造（见 `SbieKey\mainlib.cpp` 的 `kTargetDriverServices`）：命中 `CrashDrv` 时
先对 `ImagePath` 指向的 `CrashDrv.sys` 剥掉 PE Security 目录、把 `ksecdd!BCryptVerifySignature`
调用改成 `xor eax,eax; ret`、重算校验和，再临时清 `ci!g_CiOptions` 完成加载，
`NtLoadDriver` 返回后立刻恢复 DSE（剥签见 `PatchDriver()`，DSE 开关见 `killdse.cpp` 的 `DSEAction()`）。
原版只认 `SbieDrv` / `LukDrv`，不改的话 `CrashDrv` 只能吃"无条件关 DSE"那一半，
签名目录仍会留在盘上（可被静态检出）。

## addons 怎么来

CI（`.github/workflows/build.yml`）在绿色包阶段取
`sandboxie-plus/SbieHide` 的最新 release 压缩包，抽出 `SbieHide_x64.dll` / `SbieHide_x86.dll`
放进 `Installer\pack\addons\SbieHide\`。取不到时该步骤不致命，但会在日志里留一条警告。

## 打包顺序

`copy_build.cmd` 结尾会把本目录里**存在**的文件按 1:1 的名字拷进绿色包：
`Certificate.dat`、`License.dat`、`wtsapi32.dll`，以及整个 `addons\` 子树。
缺哪个就跳过哪个（不影响编译），所以本目录可以只放一部分。
