#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Yukinoshita-lin
set -e
cd "$(dirname "$0")"
command -v g++ >/dev/null || { echo "g++ not found"; exit 1; }
command -v windres >/dev/null || { echo "windres not found"; exit 1; }
mkdir -p build
echo "[1/2] Compiling resources..."
windres -i src/app.rc -o build/app_res.o --include-dir=src
echo "[2/2] Building DNAExpressionSim.exe..."
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -mwindows \
  src/main.cpp src/ui.cpp src/views.cpp src/genetics.cpp src/fileio.cpp build/app_res.o \
  -o DNAExpressionSim.exe \
  -lgdiplus -lcomdlg32 -lcomctl32 -lshell32 -lgdi32 -luser32 -lwinmm \
  -static -static-libgcc -static-libstdc++
echo "Done: DNAExpressionSim.exe"
