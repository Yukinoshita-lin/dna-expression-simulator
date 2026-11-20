// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

#include "../src/textfile.h"

#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

static int gPass = 0;
static int gFail = 0;

static void check(bool condition, const char* name) {
    if (condition) {
        ++gPass;
        std::printf("  ok   %s\n", name);
    } else {
        ++gFail;
        std::printf("  FAIL %s\n", name);
    }
}

int main() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec) /
                   ("dna_textfile_tests_" + std::to_string(GetCurrentProcessId()));
    check(!ec && fs::create_directory(dir, ec) && !ec, "创建独立测试目录");

    fs::path target = dir / L"序列.txt";
    std::wstring err;
    std::vector<char> data;

    check(textfile::writeAtomic(target.wstring(), "ATG\n", false, false, err),
          "原子写入新文件（Unicode 路径）");
    check(textfile::readAll(target.wstring(), data, err), "完整读取新文件");
    check(std::string(data.begin(), data.end()) == "ATG\n", "读取内容一致");

    check(!textfile::writeAtomic(target.wstring(), "CCC", false, false, err),
          "禁止覆盖时拒绝已有文件");
    check(textfile::readAll(target.wstring(), data, err) &&
              std::string(data.begin(), data.end()) == "ATG\n",
          "拒绝覆盖后原文件保持不变");

    check(textfile::writeAtomic(target.wstring(), "CCC", true, true, err),
          "允许覆盖时原子替换已有文件");
    check(textfile::readAll(target.wstring(), data, err) && data.size() == 6 &&
              (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
              (unsigned char)data[2] == 0xBF && std::string(data.begin() + 3, data.end()) == "CCC",
          "覆盖文件及 UTF-8 BOM 内容正确");

    check(textfile::writeAtomic(target.wstring(), "", false, true, err), "可以原子写入空文件");
    check(!textfile::readAll(target.wstring(), data, err), "空文件读取返回明确错误");
    check(!textfile::writeAtomic((dir / L"missing" / L"out.txt").wstring(), "ATG", false, true,
                                 err),
          "无效输出目录返回失败");
    check(!textfile::readAll(dir.wstring(), data, err), "目录不能作为文本文件读取");

    fs::remove(target, ec);
    fs::remove(dir, ec);
    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
