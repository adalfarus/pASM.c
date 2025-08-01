# pASM.c
 A reimplementation of an emulator in c.

 [Windows10Theme](https://github.com/B00merang-Project/Windows-10) by [
B00MERANG Project](https://github.com/B00merang-Project)
 
## Compatibility
- Windows 10 & 11 -> Guaranteed
- Windows 7 and lower -> May work
- GNU+Linux / Posix (macOS) -> Will work in GUI mode

## How to run
- Download msys2 and install it in "C:\_privat\bins" or you'll have to change all references to msys2
- Open the 64-bit mingw msys2 console
- Run the following commands, pressing ENTER for every prompt and re.-opening the terminal if it closes:
  - ``pacman -Sy``
  - ``pacman -Su``
  - ``pacman -S mingw-w64-x86_64-toolchain``
  - ``pacman -S mingw-w64-x86_64-cmake``
  - ``pacman -S mingw-w64-x86_64-libtiff``
  - ``pacman -S mingw-w64-x86_64-freetype``
  - ``pacman -S mingw-w64-x86_64-gtk4``
  - ``pacman -S mingw-w64-x86_64-make`` This installs mingw32-make
- Put the following paths in your accounts PATH environment variable:
  - "C:\_privat\bins\msys64\mingw64\bin"
- Open a Windows terminal and navigate to the folder with this file
- Run ``call build_and_run.bat``
