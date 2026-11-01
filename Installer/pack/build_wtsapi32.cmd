@echo off
REM Build the WTS proxy (wtsapi32.dll) from ..\..\SbieKey and drop it into pack\.
REM Requires Visual Studio 2022 (MSVC + Windows SDK) on this machine.
REM Usage:  Installer\pack\build_wtsapi32.cmd [x64|Win32]

setlocal
set CFG=%1
if "%CFG%"=="" set CFG=x64

set SBIEKEY=%~dp0..\..\..\SbieKey
pushd "%SBIEKEY%"
if errorlevel 1 (
    echo ERROR: SbieKey source not found at "%SBIEKEY%"
    exit /b 1
)

set VCVARS=
for %%R in (
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if not defined VCVARS if exist %%R set VCVARS=%%R

if not defined VCVARS (
    echo ERROR: vcvars64.bat not found, install the MSVC build tools.
    popd
    exit /b 1
)

call %VCVARS% >nul || (popd & exit /b 1)

msbuild sbiekey.sln /t:build /p:Configuration=Release /p:Platform=%CFG% /m || (popd & exit /b 1)

if not exist "%CFG%\Release\wtsapi32.dll" (
    echo ERROR: build finished but %CFG%\Release\wtsapi32.dll is missing.
    popd
    exit /b 1
)
copy /y "%CFG%\Release\wtsapi32.dll" "%~dp0wtsapi32.dll" || (popd & exit /b 1)

echo.
echo Built: %SBIEKEY%\%CFG%\Release\wtsapi32.dll
echo Copied to: %~dp0wtsapi32.dll
dir /b "%~dp0wtsapi32.dll"
popd
endlocal
