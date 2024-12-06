@echo off
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
make
.\pASMc.exe -ltrg
.\pASMc.exe ../diff_caesar.p
pause
