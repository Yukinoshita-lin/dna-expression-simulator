@echo off
REM SPDX-License-Identifier: Apache-2.0
REM Copyright 2026 Yukinoshita-lin
setlocal
cd /d "%~dp0"
if defined MINGW_BIN set "PATH=%MINGW_BIN%;%PATH%"
if exist "C:\mingw64\bin\g++.exe" set "PATH=C:\mingw64\bin;%PATH%"
if exist "C:\msys64\mingw64\bin\g++.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
where g++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] g++ not found. Install MinGW-w64 or set MINGW_BIN to its bin directory.
  exit /b 1
)
where windres >nul 2>nul
if errorlevel 1 (
  echo [ERROR] windres not found. Install MinGW-w64 or set MINGW_BIN to its bin directory.
  exit /b 1
)
if not exist build mkdir build
echo [1/2] Compiling resources...
windres -i src\app.rc -o build\app_res.o --include-dir=src
if errorlevel 1 exit /b 1
echo [2/2] Building DNAExpressionSim.exe...
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -DUNICODE -D_UNICODE -mwindows src\main.cpp src\ui.cpp src\views.cpp src\genetics.cpp src\textfile.cpp src\fileio.cpp build\app_res.o -o DNAExpressionSim.exe -lgdiplus -lcomdlg32 -lcomctl32 -lshell32 -lgdi32 -luser32 -lwinmm -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b 1
echo.
echo Done: DNAExpressionSim.exe
