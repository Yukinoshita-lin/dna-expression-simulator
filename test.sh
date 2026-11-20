#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Yukinoshita-lin
set -e
cd "$(dirname "$0")"
command -v g++ >/dev/null || { echo "g++ not found"; exit 1; }
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Wpedantic"
g++ $CXXFLAGS src/genetics.cpp tests/test_genetics.cpp -o tests/test_genetics.exe
g++ $CXXFLAGS src/textfile.cpp tests/test_textfile.cpp -o tests/test_textfile.exe
tests/test_genetics.exe
tests/test_textfile.exe
