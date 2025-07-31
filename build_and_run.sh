#!/bin/bash
set -e
set -o pipefail
mkdir -p build
cd build
cmake -G "Unix Makefiles" ..

make
./pASMc ../diff_caesar.p -ng
./pASMc ../diff_caesar.p
cd ..
read -p "Press any key to exit ..."
