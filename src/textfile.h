// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// textfile.h — 与界面无关的 Windows 文本文件读写
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>

namespace textfile {

// 读取完整文件；读取失败或短读时返回 false，不将部分内容当作成功结果。
bool readAll(const std::wstring& path, std::vector<char>& data, std::wstring& err);

// 在目标目录写临时文件，完整刷新后再替换目标。overwrite=false 时绝不覆盖已有文件。
bool writeAtomic(const std::wstring& path, const std::string& text, bool bom, bool overwrite,
                 std::wstring& err);

} // namespace textfile
