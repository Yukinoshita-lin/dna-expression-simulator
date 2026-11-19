@echo off
REM SPDX-License-Identifier: Apache-2.0
REM Copyright 2026 Yukinoshita-lin
setlocal
cd /d "%~dp0"
chcp 65001 >nul
if defined MINGW_BIN set "PATH=%MINGW_BIN%;%PATH%"
if exist "C:\mingw64\bin\g++.exe" set "PATH=C:\mingw64\bin;%PATH%"
if exist "C:\msys64\mingw64\bin\g++.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
where g++ >nul 2>nul
if errorlevel 1 (
  echo [ERROR] g++ not found. Install MinGW-w64 or set MINGW_BIN to its bin directory.
  exit /b 1
)
g++ -std=c++17 -O2 src\genetics.cpp tests\test_genetics.cpp -o tests\test_genetics.exe
if errorlevel 1 exit /b 1
tests\test_genetics.exe
