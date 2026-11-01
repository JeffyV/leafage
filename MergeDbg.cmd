@echo off

mkdir %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieRelease\CrashDrv.sys %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashSvc.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashDll.dll %~dp0\SandboxiePlus\x64\Debug\

rem copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\SbieCtrl.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\Start.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\kmdutil.exe %~dp0\SandboxiePlus\x64\Debug\
rem copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\SbieIni.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieRelease\CrashMsg.dll %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashHostDll.dll %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\install\Templates.ini %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashBITS.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashCrypto.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashDcomLaunch.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashRpcSs.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\CrashWUAU.exe %~dp0\SandboxiePlus\x64\Debug\

mkdir %~dp0\SandboxiePlus\x64\Debug\32\
copy /y %~dp0\Sandboxie\Bin\Win32\SbieDebug\CrashSvc.exe %~dp0\SandboxiePlus\x64\Debug\32\
copy /y %~dp0\Sandboxie\Bin\Win32\SbieDebug\CrashDll.dll %~dp0\SandboxiePlus\x64\Debug\32\

copy /y %~dp0\SandboxieTools\x64\Debug\ImBox.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\SandboxieTools\x64\Debug\UpdUtil.exe %~dp0\SandboxiePlus\x64\Debug\
