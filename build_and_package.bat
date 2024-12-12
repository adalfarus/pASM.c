@echo off
setlocal

REM Define paths
set BUILD_DIR=build
set PACKAGE_DIR=package
set GTK_SETTINGS_FILE=%PACKAGE_DIR%\etc\gtk-4.0\settings.ini

REM Create build directory
echo Building project...
mkdir %BUILD_DIR% >nul 2>&1
cd %BUILD_DIR%
cmake -G "MinGW Makefiles" .. >nul 2>&1
if errorlevel 1 (
	echo [ERROR] Failed to generate build files.
	exit /b 1
)
mingw32-make >nul 2>&1
if errorlevel 1 (
	echo [ERROR] Build process failed.
	exit /b 1
)
cd ..

REM Create package structure
mkdir %PACKAGE_DIR% >nul 2>&1
copy %BUILD_DIR%\pASMc.exe %PACKAGE_DIR%\pASMc.exe >nul 2>&1

mkdir %PACKAGE_DIR%\CTools >nul 2>&1
copy %BUILD_DIR%\CTools\libCTools.a %PACKAGE_DIR%\CTools\libCTools.a >nul 2>&1

echo Delete the build directory? (Y/n)
choice /c YN /n /m "Press Y to confirm or N to skip: "
if errorlevel 2 (
	echo Skipping deletion of the build directory.
) else (
	echo Deleting %BUILD_DIR%...
	rmdir /s /q %BUILD_DIR%
)

mkdir %PACKAGE_DIR%\etc >nul 2>&1
mkdir %PACKAGE_DIR%\etc\gtk-4.0 >nul 2>&1
echo [Settings] > %GTK_SETTINGS_FILE%
echo gtk-theme-name=Windows101 >> %GTK_SETTINGS_FILE%
echo gtk-font-name=Segoe UI 9 >> %GTK_SETTINGS_FILE%

REM Copy themes, icons, and glib schemas
echo Copying themes, icons and glib schemas...
mkdir %PACKAGE_DIR%\share >nul 2>&1
mkdir %PACKAGE_DIR%\share\themes >nul 2>&1
xcopy "Windows10Theme" "%PACKAGE_DIR%\share\themes\Windows10" /E /I /Y >nul 2>&1
xcopy "%MSYS2_PATH%\mingw64\share\icons" "%PACKAGE_DIR%\share\icons" /E /I /Y >nul 2>&1
xcopy "%MSYS2_PATH%\mingw64\share\glib-2.0" "%PACKAGE_DIR%\share\glib-2.0" /E /I /Y >nul 2>&1

REM Compile schemas
echo Compiling glib schemas...
glib-compile-schemas %PACKAGE_DIR%\share\glib-2.0\schemas
if errorlevel 1 (
	echo [ERROR] Failed to compile schemas.
	exit /b 1
)

REM Copy required DLLs
echo Copying required DLLs...
REM Doesn't find DLLs in %PACKAGE_DIR%\bin
set DLL_DIR=%PACKAGE_DIR%
mkdir %DLL_DIR% >nul 2>&1
for %%D in (
    libbrotlicommon.dll
    libbrotlidec.dll
    libbz2-1.dll
    libcairo-2.dll
    libcairo-gobject-2.dll
    libcairo-script-interpreter-2.dll
    libdatrie-1.dll
    libdeflate.dll
    libepoxy-0.dll
    libexpat-1.dll
    libffi-8.dll
    libfontconfig-1.dll
    libfreetype-6.dll
    libfribidi-0.dll
    libgcc_s_seh-1.dll
    libgdk_pixbuf-2.0-0.dll
    libgio-2.0-0.dll
    libglib-2.0-0.dll
    libgmodule-2.0-0.dll
    libgobject-2.0-0.dll
    libgraphene-1.0-0.dll
    libgraphite2.dll
    libgtk-4-1.dll
    libharfbuzz-0.dll
    libharfbuzz-subset-0.dll
    libiconv-2.dll
    libintl-8.dll
    libjbig-0.dll
    libjpeg-8.dll
    libLerc.dll
    liblzma-5.dll
    liblzo2-2.dll
    libpango-1.0-0.dll
    libpangocairo-1.0-0.dll
    libpangoft2-1.0-0.dll
    libpangowin32-1.0-0.dll
    libpcre2-8-0.dll
    libpixman-1-0.dll
    libpng16-16.dll
    libsharpyuv-0.dll
    libstdc++-6.dll
    libthai-0.dll
    libtiff-6.dll
    libwebp-7.dll
    libwinpthread-1.dll
    libzstd.dll
    zlib1.dll
) do (
    copy "%MSYS2_PATH%\mingw64\bin\%%D" %DLL_DIR% >nul 2>&1
    REM echo Copied %MSYS2_PATH%\mingw64\bin\%%D
    if errorlevel 1 (
        echo [WARNING] %%D not found.
    )
)

echo Build and packaging completed successfully!
pause