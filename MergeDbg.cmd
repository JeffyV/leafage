@echo off

mkdir %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieRelease\LeafDrv.sys %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafSvc.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafDll.dll %~dp0\SandboxiePlus\x64\Debug\

rem copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\SbieCtrl.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\Start.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\kmdutil.exe %~dp0\SandboxiePlus\x64\Debug\
rem copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\SbieIni.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieRelease\LeafMsg.dll %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafHostDll.dll %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\install\Templates.ini %~dp0\SandboxiePlus\x64\Debug\

copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafBITS.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafCrypto.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafDcomLaunch.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafRpcSs.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\Sandboxie\Bin\x64\SbieDebug\LeafWUAU.exe %~dp0\SandboxiePlus\x64\Debug\

mkdir %~dp0\SandboxiePlus\x64\Debug\32\
copy /y %~dp0\Sandboxie\Bin\Win32\SbieDebug\LeafSvc.exe %~dp0\SandboxiePlus\x64\Debug\32\
copy /y %~dp0\Sandboxie\Bin\Win32\SbieDebug\LeafDll.dll %~dp0\SandboxiePlus\x64\Debug\32\

copy /y %~dp0\SandboxieTools\x64\Debug\ImBox.exe %~dp0\SandboxiePlus\x64\Debug\
copy /y %~dp0\SandboxieTools\x64\Debug\UpdUtil.exe %~dp0\SandboxiePlus\x64\Debug\
