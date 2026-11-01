@echo off
REM ---------------------------------------------------------------------------
REM  Repair / install the Crash sandbox driver + service for THIS folder.
REM
REM  Why: the services may still point at an old folder (the registry keeps the
REM  first ImagePath forever, and "KmdUtil install" silently does nothing when
REM  the service already exists), so SandMan reports "could not connect to the
REM  driver". This script deletes and re-creates both services against the
REM  folder it lives in, then starts them and prints the result.
REM
REM  Usage (Admin):  fix_and_install.cmd            install / repair + start
REM                  fix_and_install.cmd testsign   ... and turn on test signing
REM  It self-elevates, so a plain double-click works too.
REM ---------------------------------------------------------------------------
setlocal EnableExtensions
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

net session >nul 2>&1
if errorlevel 1 (
    echo [*] Elevating...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList '%*' -Verb RunAs"
    exit /b
)

echo.
echo === Crash sandbox driver repair ===
echo folder: %HERE%
echo.

if not exist "%HERE%\KmdUtil.exe" (
    echo [x] KmdUtil.exe not found here - run this script from the package folder.
    goto :done
)
if not exist "%HERE%\CrashDrv.sys" (
    echo [x] CrashDrv.sys not found here - wrong folder?
    goto :done
)

echo [1/5] stopping SandMan / service
taskkill /f /im SandMan.exe      >nul 2>&1
taskkill /f /im SbieCtrl.exe     >nul 2>&1
"%HERE%\KmdUtil.exe" stop CrashSvc >nul 2>&1

echo [2/5] removing old service entries (old ImagePath is baked in, cannot be updated)
"%HERE%\KmdUtil.exe" delete CrashSvc >nul 2>&1
"%HERE%\KmdUtil.exe" delete CrashDrv >nul 2>&1

echo [3/5] registering against %HERE%
"%HERE%\KmdUtil.exe" install CrashDrv "%HERE%\CrashDrv.sys" type=kernel start=demand msgfile="%HERE%\CrashMsg.dll" altitude=86900
"%HERE%\KmdUtil.exe" install CrashSvc "%HERE%\CrashSvc.exe" type=own start=auto msgfile="%HERE%\CrashMsg.dll" display="Sandboxie Service" group=UIGroup

echo [4/5] checking the wtsapi32 side-loading shim (needed when test signing is off)
if exist "%HERE%\wtsapi32.dll" (
    echo       OK: wtsapi32.dll present
) else (
    echo       [!] wtsapi32.dll is MISSING.
    echo           An antivirus ^(Huorong / Defender^) most likely quarantined it,
    echo           it embeds the RTCore64 BYOVD. Without it a test-signed
    echo           CrashDrv.sys cannot load on a machine with DSE enabled.
    echo           Fix one of these ways:
    echo             A^) trust this folder in Huorong, then copy wtsapi32.dll
    echo                out of the release zip again;
    echo             B^) run:  fix_and_install.cmd testsign   ^(test signing on, reboot^)
)

echo [5/5] starting the service ^(this is what loads CrashDrv through the shim^)
"%HERE%\KmdUtil.exe" start CrashSvc
echo.

echo --- driver state ---
sc query CrashDrv
echo.
echo --- filter drivers ---
fltmc filters 2>nul | findstr /i "crashdrv" || echo (CrashDrv not in fltmc list yet)
echo.
echo --- last driver/service errors ---
powershell -NoProfile -Command "Get-WinEvent -FilterHashtable @{LogName='System';StartTime=(Get-Date).AddMinutes(-5)} -ErrorAction SilentlyContinue | Where-Object { $_.Message -match 'CrashDrv|Sandboxie' } | Select-Object -First 5 TimeCreated,Id,Message | Format-List"

if /i "%~1"=="testsign" (
    echo.
    echo [*] enabling test signing ^(reboot required^)
    bcdedit /set testsigning on
    echo     after the reboot just run SandMan.exe - no shim needed.
)

:done
echo.
pause
endlocal
