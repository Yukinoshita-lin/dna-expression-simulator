// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

#include "textfile.h"

#include <algorithm>
#include <filesystem>

namespace textfile {
namespace {

std::wstring windowsError(const wchar_t* action, DWORD code) {
    return std::wstring(action) + L"（错误码 " + std::to_wstring(code) + L"）";
}

bool writeBytes(HANDLE file, const unsigned char* bytes, size_t size, DWORD& error) {
    size_t offset = 0;
    while (offset < size) {
        DWORD written = 0;
        DWORD chunk = (DWORD)std::min<size_t>(size - offset, 1u << 20);
        if (!WriteFile(file, bytes + offset, chunk, &written, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (written == 0) {
            error = ERROR_WRITE_FAULT;
            return false;
        }
        offset += written;
    }
    return true;
}

} // namespace

bool readAll(const std::wstring& path, std::vector<char>& data, std::wstring& err) {
    data.clear();
    err.clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = windowsError(L"无法打开文件", GetLastError());
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        DWORD error = GetLastError();
        CloseHandle(h);
        err = windowsError(L"无法读取文件大小", error);
        return false;
    }
    if (size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(h);
        err = L"文件为空或超过 16 MB";
        return false;
    }

    data.resize((size_t)size.QuadPart);
    size_t got = 0;
    while (got < data.size()) {
        DWORD n = 0;
        DWORD want = (DWORD)std::min<size_t>(data.size() - got, 1u << 20);
        if (!ReadFile(h, data.data() + got, want, &n, nullptr)) {
            DWORD error = GetLastError();
            CloseHandle(h);
            data.clear();
            err = windowsError(L"读取文件时发生错误", error);
            return false;
        }
        if (n == 0) {
            CloseHandle(h);
            data.clear();
            err = L"文件读取不完整：实际内容少于打开时的大小";
            return false;
        }
        got += n;
    }
    CloseHandle(h);
    return true;
}

bool writeAtomic(const std::wstring& path, const std::string& text, bool bom, bool overwrite,
                 std::wstring& err) {
    err.clear();
    if (path.empty()) {
        err = L"输出路径为空";
        return false;
    }

    std::filesystem::path target(path);
    std::wstring directory = target.parent_path().wstring();
    if (directory.empty()) directory = L".";

    wchar_t tempPath[MAX_PATH]{};
    if (!GetTempFileNameW(directory.c_str(), L"dna", 0, tempPath)) {
        err = windowsError(L"无法在目标目录创建临时文件", GetLastError());
        return false;
    }

    HANDLE h = CreateFileW(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        DeleteFileW(tempPath);
        err = windowsError(L"无法写入临时文件", error);
        return false;
    }

    bool ok = true;
    DWORD error = ERROR_SUCCESS;
    if (bom) {
        const unsigned char marker[3] = {0xEF, 0xBB, 0xBF};
        ok = writeBytes(h, marker, sizeof(marker), error);
    }
    if (ok) ok = writeBytes(h, (const unsigned char*)text.data(), text.size(), error);
    if (ok && !FlushFileBuffers(h)) {
        ok = false;
        error = GetLastError();
    }
    CloseHandle(h);

    if (!ok) {
        DeleteFileW(tempPath);
        err = windowsError(L"写入文件时发生错误，原文件未被修改", error);
        return false;
    }

    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
    if (!MoveFileExW(tempPath, path.c_str(), flags)) {
        error = GetLastError();
        DeleteFileW(tempPath);
        if (!overwrite && (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS))
            err = L"目标文件已存在，未覆盖；命令行导出可使用 --force 显式覆盖";
        else
            err = windowsError(L"无法完成文件保存，原文件未被修改", error);
        return false;
    }
    return true;
}

} // namespace textfile
