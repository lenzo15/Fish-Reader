@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal
cd /d "%~dp0" || exit /b 1
if not "%~1"=="" if /i not "%~1"=="test" if /i not "%~1"=="test-core" (
  echo Usage: build.cmd [test-core^|test]
  exit /b 2
)
if not defined UCRT64 set "UCRT64=C:\msys64\ucrt64"
if not exist "%UCRT64%\bin\g++.exe" (
  echo Missing MSYS2 UCRT64 compiler. Set UCRT64 to the toolchain prefix; see README.md.
  exit /b 1
)
set "PATH=%UCRT64%\bin;%PATH%"
if not exist "bin\obj" mkdir "bin\obj" || exit /b 1
set "flags=-std=c++17 -O2 -municode -finput-charset=UTF-8 -Wall -Wextra -Isrc"
set "libs=-ld2d1 -ldwrite -lxmllite -lshlwapi -lz -ldwmapi -lcomdlg32 -lole32 -luuid -lshell32 -lgdi32"
set "objs=bin\obj\book.o bin\obj\window.o bin\obj\render.o bin\obj\input.o bin\obj\shortcut_editor.o"

rem Core tests do not build, link or launch the UI, so a running reader is not affected.
if /i "%~1"=="test-core" goto testCore
if /i "%~1"=="test" goto testCore

windres -I . resources\windows\app.rc -O coff -o bin\obj\app.res || exit /b 1
for %%f in (src\reader\book.cpp src\ui\window.cpp src\ui\render.cpp src\ui\input.cpp src\ui\shortcut_editor.cpp) do (
  g++ %flags% -c "%%f" -o "bin\obj\%%~nf.o" || exit /b 1
)
g++ %flags% -s -static -mwindows src\main.cpp %objs% bin\obj\app.res -o bin\FishReader.exe %libs% || exit /b 1
exit /b 0

:testCore
g++ %flags% -static tests\core_tests.cpp src\reader\book.cpp -o bin\core-tests.exe -ldwrite -lxmllite -lshlwapi -lz -lole32 -luuid || exit /b 1
bin\core-tests.exe || exit /b 1
exit /b 0
