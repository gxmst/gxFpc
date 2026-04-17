@echo off
setlocal enabledelayedexpansion

echo [+] Universal Build Script starting...

:: SPECIFIC PATH FOR VS 2026 INSIDERS (As found)
set "TARGET_PATH=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

if exist "!TARGET_PATH!" (
    echo [+] SUCCESS: Valid environment found at !TARGET_PATH!
    call "!TARGET_PATH!"
) else (
    echo [!] Searching for alternatives...
    :: Fallback to common paths if the specific one fails
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "TARGET_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if exist "F:\vs\VC\Auxiliary\Build\vcvars64.bat" set "TARGET_PATH=F:\vs\VC\Auxiliary\Build\vcvars64.bat"
    
    if exist "!TARGET_PATH!" (
        echo [+] Found fallback environment: !TARGET_PATH!
        call "!TARGET_PATH!"
    ) else (
        echo [!] ERROR: Could not find vcvars64.bat anywhere.
        echo [!] Please ensure the "Desktop development with C++" workload is installed.
        pause
        exit /b
    )
)

echo [+] Compiling FragPunkExternal.cpp with C++17...
cl.exe /EHsc /O2 /MT /std:c++17 FragPunkExternal.cpp /Fe:FragPunkExternal.exe

if %errorlevel% neq 0 (
    echo [!] Compilation failed. Error Code: %errorlevel%
) else (
    echo [x] SUCCESS: FragPunkExternal.exe is created.
)

pause
