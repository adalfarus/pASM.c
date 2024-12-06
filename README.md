# pASM.c
 A reimplementation of an emulator in c
 
## Compatibility
- Windows 10 & 11 -> Guaranteed
- Windows 7 and lower -> May work
- Posix / macOS -> Will work with some patches, mostly already cross platform

## How to run
- Download msys2 and install it in "C:\_privat\bins" or you'll have to change all references to msys2
- Open the 64-bit console
- Run the following commands:
  - ``pacman -Syu``
  - If prompted to restart the terminal, close it and reopen the terminal and re-run the previous command
  - ``pacman -S mingw-w64-ucrt-x86_64-gtk4``
  - ``pacman -S mingw-w64-ucrt-x86_64-toolchain base-devel``
  - ``pacman -S mingw-w64-ucrt-x86_64-cmake``
  - ``pacman -S make``
  - ``pacman -S mingw-w64-ucrt-x86_64-gcc``
- Put the following paths in your PATH environment variable:
  - "C:\_privat\bins\msys64\mingw64\bin"
  - "C:\_privat\bins\msys64\ucrt64\bin"
  - "C:\_privat\bins\msys64\usr\bin"
- Open a Windows terminal and navigate to the folder with this file
- Run ``call build_and_run.bat``
