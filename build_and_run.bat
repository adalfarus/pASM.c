@echo off
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
if errorlevel 1 (
    echo [ERROR] Failed to generate build files.
    exit /b 1
)
mingw32-make
if errorlevel 1 (
    echo [ERROR] Build process failed.
    exit /b 1
)
.\pASMc.exe ../diff_caesar.p -ng
.\pASMc.exe ../diff_caesar.p
cd ..
pause
