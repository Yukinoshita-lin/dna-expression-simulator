// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// fileio.cpp — 文件对话框、UTF-8/UTF-16 读取与 TXT 导出
#include "app.h"
#include "ui.h"
#include "genetics.h"

#include <commdlg.h>
#include <string>
#include <vector>

static bool readAllBytes(const std::wstring& path, std::vector<char>& data, std::wstring& err) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = L"无法打开文件（错误码 " + std::to_wstring(GetLastError()) + L"）";
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(h);
        err = L"文件为空或超过 16 MB";
        return false;
    }
    data.resize((size_t)size.QuadPart);
    size_t got = 0;
    while (got < data.size()) {
        DWORD n = 0;
        DWORD want = (DWORD)std::min<size_t>(data.size() - got, 1u << 20);
        if (!ReadFile(h, data.data() + got, want, &n, nullptr) || n == 0) break;
        got += n;
    }
    CloseHandle(h);
    data.resize(got);
    return true;
}

static bool writeAllBytes(const std::wstring& path, const std::string& text, bool bom,
                          std::wstring& err) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = L"无法写入文件（错误码 " + std::to_wstring(GetLastError()) + L"）";
        return false;
    }
    bool ok = true;
    if (bom) {
        const unsigned char b[3] = {0xEF, 0xBB, 0xBF};
        DWORD written = 0;
        ok = WriteFile(h, b, 3, &written, nullptr) && written == 3;
    }
    if (ok) {
        DWORD written = 0;
        ok = WriteFile(h, text.data(), (DWORD)text.size(), &written, nullptr) &&
             written == text.size();
    }
    CloseHandle(h);
    if (!ok) err = L"写入文件时发生错误";
    return ok;
}

static std::string crlf(const std::string& s) {
    std::string out;
    out.reserve(s.size() + s.size() / 20);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r')) out += "\r\n";
        else out.push_back(s[i]);
    }
    return out;
}

std::wstring fileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

static std::string decodeFileText(std::vector<char>& data) {
    if (data.size() >= 2) {
        unsigned char b0 = (unsigned char)data[0], b1 = (unsigned char)data[1];
        if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)) {
            bool le = (b0 == 0xFF);
            std::vector<wchar_t> w;
            size_t n = (data.size() - 2) / 2;
            w.resize(n);
            for (size_t i = 0; i < n; ++i) {
                unsigned char lo = (unsigned char)data[2 + i * 2];
                unsigned char hi = (unsigned char)data[3 + i * 2];
                wchar_t ch = le ? (wchar_t)(lo | (hi << 8)) : (wchar_t)((lo << 8) | hi);
                w[i] = ch;
            }
            return ui::narrow(std::wstring(w.begin(), w.end()));
        }
    }
    size_t off = 0;
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF)
        off = 3;
    return std::string(data.begin() + (ptrdiff_t)off, data.end());
}

static bool openSequenceDialog(HWND owner, std::wstring& path) {
    wchar_t buf[4096] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 4096;
    ofn.lpstrFilter =
        L"序列文件 (*.txt;*.fasta;*.fa;*.fna)\0*.txt;*.fasta;*.fa;*.fna\0"
        L"所有文件 (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"导入 DNA 序列";
    if (!GetOpenFileNameW(&ofn)) return false;
    path = buf;
    return true;
}

static bool saveSequenceDialog(HWND owner, const wchar_t* title, const wchar_t* defaultName,
                               std::wstring& path) {
    wchar_t buf[4096] = L"";
    wcsncpy(buf, defaultName, 4095);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 4096;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = title;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = buf;
    return true;
}

bool importDnaFile(App& a) {
    std::wstring path;
    if (!openSequenceDialog(a.hwnd, path)) return false;
    std::vector<char> bytes;
    std::wstring err;
    if (!readAllBytes(path, bytes, err)) {
        MessageBoxW(a.hwnd, err.c_str(), L"导入失败", MB_OK | MB_ICONERROR);
        return false;
    }
    std::string text = decodeFileText(bytes);
    if (text.empty()) {
        MessageBoxW(a.hwnd, L"文件内容为空。", L"导入失败", MB_OK | MB_ICONWARNING);
        return false;
    }
    dna::ParseResult parsed = dna::parseSequence(text);
    std::wstring shown;
    if (parsed.ok) {
        std::wstring header = L">imported | " + fileNameOf(path) + L" | " +
                              std::to_wstring(parsed.sequence.size()) + L" bp";
        shown = header + L"\r\n";
        for (size_t i = 0; i < parsed.sequence.size(); i += 60)
            shown += ui::utf8(parsed.sequence.substr(i, 60)) + L"\r\n";
        setStatus(a, L"已导入 " + fileNameOf(path) + L" · " +
                        std::to_wstring(parsed.sequence.size()) + L" bp");
    } else {
        shown = ui::utf8(text);
        setStatus(a, L"已读取 " + fileNameOf(path) + L" · 序列需要修正（见左侧提示）");
    }
    setEditText(a, shown);
    return true;
}

bool exportDnaFile(App& a) {
    if (!a.hasSeq) {
        MessageBoxW(a.hwnd, L"当前没有有效的 DNA 序列。", L"无法导出", MB_OK | MB_ICONWARNING);
        return false;
    }
    std::wstring path;
    if (!saveSequenceDialog(a.hwnd, L"导出 DNA 序列", L"DNA_sequence.txt", path)) return false;
    std::wstring err;
    if (!writeDnaFile(a, path, &err)) {
        MessageBoxW(a.hwnd, err.c_str(), L"导出失败", MB_OK | MB_ICONERROR);
        return false;
    }
    setStatus(a, L"DNA 序列已导出：" + path);
    InvalidateRect(a.hwnd, nullptr, FALSE);
    return true;
}

bool exportProteinFile(App& a) {
    if (!a.hasSeq || a.tr.protein.empty()) {
        MessageBoxW(a.hwnd,
                    L"当前没有可导出的多肽。请先输入序列，并确认存在有效的阅读框。",
                    L"无法导出", MB_OK | MB_ICONWARNING);
        return false;
    }
    std::wstring path;
    if (!saveSequenceDialog(a.hwnd, L"导出蛋白质序列", L"protein_sequence.txt", path)) return false;
    std::wstring err;
    if (!writeProteinFile(a, path, &err)) {
        MessageBoxW(a.hwnd, err.c_str(), L"导出失败", MB_OK | MB_ICONERROR);
        return false;
    }
    setStatus(a, L"蛋白质序列已导出：" + path);
    InvalidateRect(a.hwnd, nullptr, FALSE);
    return true;
}

bool writeDnaFile(App& a, const std::wstring& path, std::wstring* err) {
    if (!a.hasSeq) {
        if (err) *err = L"没有有效序列";
        return false;
    }
    std::string text = dna::buildDnaExport(a.coding, a.stats);
    return writeAllBytes(path, crlf(text), false, *err);
}

bool writeProteinFile(App& a, const std::wstring& path, std::wstring* err) {
    if (!a.hasSeq || a.tr.protein.empty()) {
        if (err) *err = L"没有可导出的多肽";
        return false;
    }
    std::string text = dna::buildProteinExport(a.coding, a.frame, a.revcomp, a.tr, a.stats,
                                               ui::narrow(nowDateTime()));
    return writeAllBytes(path, crlf(text), true, *err);
}
