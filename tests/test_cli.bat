@echo off
REM SPDX-License-Identifier: Apache-2.0
REM Copyright 2026 Yukinoshita-lin
setlocal
if not exist "%~dp0..\DNAExpressionSim.exe" (
  echo [ERROR] DNAExpressionSim.exe not found. Run build.bat first.
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0test_cli.ps1" -ExePath "%~dp0..\DNAExpressionSim.exe"
