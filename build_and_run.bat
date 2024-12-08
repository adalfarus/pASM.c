@echo off
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
make
.\pASMc.exe ../diff_caesar.p -ng
.\pASMc.exe ../diff_caesar.p
pause
