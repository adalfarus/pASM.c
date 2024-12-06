# pASM.c
 A reimplementation of an emulator in c
 
## Compatibility
- Windows 10 & 11 -> Guaranteed
- Windows 7 and lower -> May work
- Posix / macOS -> Will work with some patches, mostly already cross platform

## How to run
- Download msys2 and install it in "C:\_privat\bins" or you'll have to change all references to msys2
- Open the 64-bit ucrt msys2 console
- (These may or may not work for you)
- Run the following commands, pressing ENTER for every prompt and re.-opening the terminal if it closes:
  - ``pacman -Sy``
  - ``pacman -Su``
  - ``pacman -S mingw-w64-ucrt-x86_64-gtk4``
  - ``pacman -S mingw-w64-ucrt-x86_64-toolchain base-devel``
  - ``pacman -S mingw-w64-ucrt-x86_64-cmake``
  - ``pacman -S mingw-w64-ucrt-x86_64-make``
  - ``pacman -S mingw-w64-ucrt-x86_64-gcc``
- Put the following paths in your PATH environment variable:
  - "C:\_privat\bins\msys64\mingw64\bin"
  - "C:\_privat\bins\msys64\ucrt64\bin"
  - "C:\_privat\bins\msys64\usr\bin"
- Open a Windows terminal and navigate to the folder with this file
- Run ``call build_and_run.bat``
