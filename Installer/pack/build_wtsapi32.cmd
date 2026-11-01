@echo off
REM Build the WTS proxy (wtsapi32.dll) from the vendored sources in ..\..\SbieKey
REM and drop the result into pack\, so copy_build.cmd ships it in the green package.
REM Needs Visual Studio 2022 (MSVC + Windows SDK + MASM) - CI does this automatically.
REM Usage:  Installer\pack\build_wtsapi32.cmd [x64]

setlocal
set CFG=%1
if "%CFG%"=="" set CFG=x64

set PROJ=%~dp0..\..\SbieKey\wtsapi32.vcxproj
set OUT=%~dp0..\..\SbieKey\Bin\%CFG%\Release\wtsapi32.dll

set VCVARS=
for %%R in (
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if not defined VCVARS if exist %%R set VCVARS=%%R

if not defined VCVARS (
    echo ERROR: vcvars64.bat not found - install the MSVC build tools, or let CI build it.
    exit /b 1
)
call %VCVARS% >nul || exit /b 1

msbuild "%PROJ%" /t:build /p:Configuration=Release /p:Platform=%CFG% /m || exit /b 1

if not exist "%OUT%" (
    echo ERROR: build finished but %OUT% is missing.
    exit /b 1
)
copy /y "%OUT%" "%~dp0wtsapi32.dll" || exit /b 1

echo.
echo Built:  %OUT%
echo Copied: %~dp0wtsapi32.dll
dir /b "%~dp0wtsapi32.dll"
endlocal
