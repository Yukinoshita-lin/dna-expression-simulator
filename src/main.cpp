// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// main.cpp — 窗口、布局、交互、动画与页面骨架
#include "app.h"
#include "ui.h"
#include "genetics.h"

#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

static const UINT_PTR TIMER_ANIM = 1;
static const UINT_PTR TIMER_EDIT = 2;
static const int EDIT_ID = 1001;
static const size_t MAX_BASES = 30000;

static App gApp;
static ULONG_PTR gGdiToken = 0;
static bool gEditFocused = false;
static int gPressedBtn = 0;
static bool gTrackingLeave = false;
static bool gEditPending = false;

// ------------------------------------------------------------------ 示例
static const char* kSample1 =
    "GATCGGATCCAC"
    "ATGAAGGTGGCCATCCTGAGCGACGAGTGGAACTACTTCCAGCGCGGTTAA"
    "GCTAGCTAGGCTA";

static const char* kSample2 =
    "GCTAGCTAGGCT"
    "TTAGTCCAACTTACCGTACGGCAT"
    "GATCGATCGGAT";

static const char* kSample3 =
    "CGAATCTGCGTTTTT"
    "ATGAATGGATCTCCCGAACACATATGTCTGCGTGAAACACTAGCTTCTATCTGTCCCTCTGCCGTGGTCGCAATCCCGTTAGTTTTTGTCTCTTAGCTGGTGTTCATACT";

// ------------------------------------------------------------------ DPI
static void enableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx) {
            if (setCtx((HANDLE)-4)) return;   // PER_MONITOR_AWARE_V2
            if (setCtx((HANDLE)-3)) return;   // PER_MONITOR_AWARE
        }
    }
    SetProcessDPIAware();
}

static UINT dpiOfWindow(HWND hwnd) {
    using GetDpiFn = UINT(WINAPI*)(HWND);
    static GetDpiFn fn = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) fn = (GetDpiFn)GetProcAddress(user32, "GetDpiForWindow");
    }
    if (fn) return fn(hwnd);
    HDC dc = GetDC(hwnd);
    UINT dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(hwnd, dc);
    return dpi ? dpi : 96;
}

static UINT dpiOfSystem() {
    HDC dc = GetDC(nullptr);
    UINT dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi ? dpi : 96;
}

// ------------------------------------------------------------------ 小工具
static RECT mk(float x, float y, float w, float h) {
    RECT r;
    r.left = (LONG)std::lround(x);
    r.top = (LONG)std::lround(y);
    r.right = (LONG)std::lround(x + w);
    r.bottom = (LONG)std::lround(y + h);
    return r;
}

static float rectW(const RECT& r) { return (float)(r.right - r.left); }
static float rectH(const RECT& r) { return (float)(r.bottom - r.top); }

static RECT insetRect(const RECT& r, float d) {
    return mk((float)r.left + d, (float)r.top + d, rectW(r) - 2 * d, rectH(r) - 2 * d);
}

static bool hit(const RECT& r, POINT p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static int hitButton(App& a, POINT p) {
    for (auto it = a.buttons.rbegin(); it != a.buttons.rend(); ++it) {
        if (it->enabled && hit(it->rc, p)) return it->id;
    }
    return 0;
}

static int hitCodon(App& a, POINT p) {
    if (a.tab != 3 || !a.codonCellsValid) return -1;
    for (int i = 0; i < 64; ++i)
        if (hit(a.codonCellRects[i], p)) return i;
    return -1;
}

static RECT expandRect(const RECT& r, float d) {
    RECT o = r;
    o.left -= (LONG)d; o.top -= (LONG)d; o.right += (LONG)d; o.bottom += (LONG)d;
    return o;
}

// ------------------------------------------------------------------ 模型
void setStatus(App& a, const std::wstring& s) {
    a.status = s;
    a.statusAt = GetTickCount64();
}

std::wstring getEditText(App& a) {
    if (!a.hEdit) return std::wstring();
    int len = GetWindowTextLengthW(a.hEdit);
    if (len <= 0) return std::wstring();
    std::vector<wchar_t> buf((size_t)len + 1, 0);
    GetWindowTextW(a.hEdit, buf.data(), len + 1);
    return std::wstring(buf.data(), (size_t)len);
}

void setEditText(App& a, const std::wstring& text) {
    if (a.hEdit) SetWindowTextW(a.hEdit, text.c_str());
}

std::wstring formatFasta(const std::string& seq) {
    std::wstring out = L">sequence | " + std::to_wstring(seq.size()) + L" bp | coding strand 5'-3'\r\n";
    for (size_t i = 0; i < seq.size(); i += 60) {
        out += ui::utf8(seq.substr(i, 60));
        out += L"\r\n";
    }
    return out;
}

std::wstring nowDateTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute);
    return std::wstring(buf);
}

static std::wstring fastaWithHeader(const std::string& seq, const std::wstring& header) {
    std::wstring out = header + L"\r\n";
    for (size_t i = 0; i < seq.size(); i += 60) {
        out += ui::utf8(seq.substr(i, 60));
        out += L"\r\n";
    }
    return out;
}

static void stopPlaying(App& a) {
    a.playing = false;
    if (a.hwnd) KillTimer(a.hwnd, TIMER_ANIM);
}

void recomputeFromEdit(App& a) {
    gEditPending = false;
    if (a.hwnd) KillTimer(a.hwnd, TIMER_EDIT);
    a.staticDirty = true;
    std::string raw = ui::narrow(getEditText(a));
    a.parsed = dna::parseSequence(raw);
    a.errorText.clear();
    a.warnings.clear();
    a.hasSeq = false;
    a.tr = dna::TranslationResult{};
    a.stats = dna::SequenceStats{};
    a.coding.clear();
    a.templ.clear();
    a.mrna.clear();
    stopPlaying(a);

    if (a.parsed.errorCode == "invalid") {
        std::wstring names;
        for (size_t i = 0; i < a.parsed.invalidChars.size(); ++i) {
            if (i) names += L"、";
            names += (wchar_t)a.parsed.invalidChars[i];
        }
        a.errorText = L"序列包含不支持的字母：" + names + L"。仅支持 A / T / G / C / U / N。";
    } else if (a.parsed.errorCode == "empty") {
        a.errorText.clear();
    } else if (a.parsed.sequence.size() > MAX_BASES) {
        a.errorText = L"序列过长：最多支持 30,000 bp。";
    }

    if (!a.parsed.ok || a.parsed.sequence.empty() || a.parsed.sequence.size() > MAX_BASES) {
        if (a.parsed.errorCode == "empty") {
            setStatus(a, L"就绪 · 输入或导入 DNA 序列开始模拟");
        }
        return;
    }

    a.hasSeq = true;
    a.coding = a.revcomp ? dna::reverseComplement(a.parsed.sequence) : a.parsed.sequence;
    a.stats = dna::computeStats(a.coding);
    a.templ = dna::complement(a.coding);
    a.mrna = dna::transcribe(a.coding);
    a.tr = dna::translate(a.coding, a.frame);

    if (a.parsed.convertedU)
        a.warnings.push_back(L"检测到 U 碱基，已按 DNA 的 T 处理（转录结果中显示为 U）。");
    if (a.parsed.hasN)
        a.warnings.push_back(L"序列含 N（未知碱基），翻译时对应位置以 X 表示。");
    if (a.revcomp)
        a.warnings.push_back(L"当前使用反向互补链作为编码链（反向链基因）。");
    if (!a.tr.hasStart)
        a.warnings.push_back(L"未找到起始密码子 ATG，翻译从当前阅读框起始处开始。");
    if (a.tr.hasStart && !a.tr.hasStop)
        a.warnings.push_back(L"未找到终止密码子，蛋白质序列可能不完整。");
    if (a.tr.hasStart && a.tr.hasStop && a.tr.protein.empty())
        a.warnings.push_back(L"该 ORF 在起始密码子后立即终止，未产生多肽。");
    if (a.warnings.size() > 4) a.warnings.resize(4);

    float len = (float)a.coding.size();
    a.animDuration[0] = a.animDuration[1] = std::min(16.0f, std::max(6.0f, 5.0f + len * 0.055f));
    a.animDuration[2] = std::min(20.0f, std::max(6.0f, 4.5f + (float)(a.tr.codons.size() + 1) * 0.5f));
}

void applySequence(App& a, const std::string& seq) {
    a.frame = 0;
    a.revcomp = false;
    setEditText(a, formatFasta(seq));
    recomputeFromEdit(a);
    for (int i = 0; i < 3; ++i) a.progress[i] = 0;
    buildLayout(a);
    InvalidateRect(a.hwnd, nullptr, FALSE);
}

void loadSample(App& a, int idx) {
    a.frame = 0;
    a.revcomp = false;
    std::string seq;
    std::wstring header;
    std::wstring msg;
    if (idx == 1) {
        seq = kSample1;
        header = L">sample_1 | 标准基因 | 76 bp";
        msg = L"已载入示例 1 · 标准基因（含 5'/3' UTR）";
    } else if (idx == 2) {
        seq = kSample2;
        header = L">sample_2 | 反向链基因 | 48 bp";
        msg = L"已载入示例 2 · 在翻译页开启「反向互补」即可找到完整 ORF";
    } else {
        seq = kSample3;
        header = L">sample_3 | 长肽链 | 125 bp";
        msg = L"已载入示例 3 · 31 aa 长肽链（合成序列）";
    }
    setEditText(a, fastaWithHeader(seq, header));
    recomputeFromEdit(a);
    for (int i = 0; i < 3; ++i) a.progress[i] = 0;
    buildLayout(a);
    setStatus(a, msg);
    InvalidateRect(a.hwnd, nullptr, FALSE);
}

void updateAnimation(App& a) {
    if (!a.playing) return;
    if (a.tab < 0 || a.tab > 2) { stopPlaying(a); return; }
    ULONGLONG now = GetTickCount64();
    double dt = (now >= a.lastTick) ? (double)(now - a.lastTick) / 1000.0 : 0.0;
    if (dt > 0.10) dt = 0.10;   // 防止窗口拖动/系统挂起后动画跳跃
    a.lastTick = now;
    float& p = a.progress[a.tab];
    p += (float)(dt / (double)std::max(0.5f, a.animDuration[a.tab]));
    if (p >= 1.0f) {
        p = 1.0f;
        stopPlaying(a);
    }
    InvalidateRect(a.hwnd, &a.rcRight, FALSE);
}

static void startPlaying(App& a) {
    if (!a.hasSeq || a.tab > 2) return;
    float& p = a.progress[a.tab];
    if (p >= 1.0f) p = 0.0f;
    a.playing = true;
    a.lastTick = GetTickCount64();
    SetTimer(a.hwnd, TIMER_ANIM, 10, nullptr);
    InvalidateRect(a.hwnd, &a.rcRight, FALSE);
}

// ------------------------------------------------------------------ 布局
void buildLayout(App& a) {
    a.buttons.clear();
    a.warnVisible = false;
    a.sliderVisible = false;
    a.codonCellsValid = false;

    const float s = a.s;
    const float W = (float)a.cw;
    const float H = (float)a.ch;
    const float margin = 18.f * s;
    const float gap = 16.f * s;
    const float headerH = 70.f * s;
    const float footerH = 26.f * s;

    a.rcHeader = mk(0, 0, W, headerH);
    a.rcFooter = mk(0, H - footerH, W, footerH);
    float cTop = headerH + margin;
    float cBottom = H - footerH - margin;
    float leftW = std::min(372.f * s, std::max(320.f * s, W * 0.285f));

    a.rcLeft = mk(margin, cTop, leftW, cBottom - cTop);
    float rx = (float)a.rcLeft.right + gap;
    float rightW = W - margin - rx;
    a.rcTabs = mk(rx, cTop, rightW, 42.f * s);
    float rightTop = (float)a.rcTabs.bottom + 12.f * s;
    a.rcRight = mk(rx, rightTop, rightW, cBottom - rightTop);

    // -------- 左栏 --------
    const float p = 18.f * s;
    float lx = (float)a.rcLeft.left + p;
    float lw = leftW - 2 * p;
    float ly = (float)a.rcLeft.top + 16.f * s;
    a.rcLeftTitle = mk(lx, ly, lw, 24.f * s);
    a.rcTitleChip = mk(lx + lw - 96.f * s, ly + 2.f * s, 96.f * s, 20.f * s);

    float editTop = ly + 32.f * s;
    float bottom = (float)a.rcLeft.bottom - 16.f * s;
    a.rcCaption = mk(lx, bottom - 14.f * s, lw, 14.f * s);
    float exportTop = bottom - 14.f * s - 6.f * s - 46.f * s;
    a.rcExport = mk(lx, exportTop, lw, 46.f * s);

    bool warn = !a.errorText.empty() || !a.warnings.empty();
    a.warnVisible = warn;
    float warnH = 42.f * s;
    if (warn) a.rcWarn = mk(lx, exportTop - 10.f * s - warnH, lw, warnH);

    float belowStats = (warn ? (float)a.rcWarn.top : exportTop) - 16.f * s;
    a.rcCompLabels = mk(lx, belowStats - 14.f * s, lw, 14.f * s);
    a.rcCompBar = mk(lx, (float)a.rcCompLabels.top - 8.f * s - 8.f * s, lw, 8.f * s);
    float tilesBottom = (float)a.rcCompBar.top - 12.f * s;
    float tileH = 52.f * s;
    float tileGap = 8.f * s;
    float tileW = (lw - 2 * tileGap) / 3.f;
    for (int i = 0; i < 3; ++i)
        a.rcTiles[i] = mk(lx + i * (tileW + tileGap), tilesBottom - tileH, tileW, tileH);
    a.rcStatsHeader = mk(lx, (float)a.rcTiles[0].top - 8.f * s - 16.f * s, lw, 16.f * s);
    float dividerY = (float)a.rcStatsHeader.top - 12.f * s;
    a.rcDivider = mk(lx, dividerY, lw, 1.f);

    float row2H = 26.f * s;
    float row2Top = dividerY - 12.f * s - row2H;
    float row2Gap = 6.f * s;
    float smallW = (lw - 4 * row2Gap) / 5.f;
    for (int i = 0; i < 5; ++i)
        a.rcRow2[i] = mk(lx + i * (smallW + row2Gap), row2Top, smallW, row2H);

    float row1H = 32.f * s;
    float row1Top = row2Top - 8.f * s - row1H;
    float halfW = (lw - 8.f * s) / 2.f;
    a.rcRow1[0] = mk(lx, row1Top, halfW, row1H);
    a.rcRow1[1] = mk(lx + halfW + 8.f * s, row1Top, halfW, row1H);

    float editBottom = row1Top - 10.f * s;
    a.rcEditFrame = mk(lx, editTop, lw, editBottom - editTop);
    a.rcEdit = mk(lx + 8.f * s, editTop + 7.f * s, lw - 16.f * s, (editBottom - editTop) - 14.f * s);

    auto addBtn = [&](RECT rc, int id, ButtonKind kind, const std::wstring& label, bool enabled,
                      bool active = false) {
        Button b;
        b.rc = rc;
        b.id = id;
        b.kind = kind;
        b.label = label;
        b.enabled = enabled;
        b.active = active;
        a.buttons.push_back(b);
    };

    addBtn(a.rcRow1[0], BTN_IMPORT, BK_SECONDARY, L"导入序列…", true);
    addBtn(a.rcRow1[1], BTN_EXPORT_DNA, BK_SECONDARY, L"导出 DNA…", a.hasSeq);
    addBtn(a.rcRow2[0], BTN_SAMPLE1, BK_SMALL, L"示例 1", true);
    addBtn(a.rcRow2[1], BTN_SAMPLE2, BK_SMALL, L"示例 2", true);
    addBtn(a.rcRow2[2], BTN_SAMPLE3, BK_SMALL, L"示例 3", true);
    addBtn(a.rcRow2[3], BTN_RANDOM, BK_SMALL, L"随机", true);
    addBtn(a.rcRow2[4], BTN_CLEAR, BK_SMALL, L"清空", true);
    addBtn(a.rcExport, BTN_EXPORT_PROTEIN, BK_GREEN, L"导出蛋白质序列 (.txt)",
           a.hasSeq && !a.tr.protein.empty());

    // -------- 标签栏 --------
    float padT = 3.f * s, gapT = 4.f * s;
    float tabW = (rectW(a.rcTabs) - 2 * padT - 3 * gapT) / 4.f;
    const wchar_t* tabNames[4] = {L"复制", L"转录", L"翻译", L"密码子表"};
    for (int i = 0; i < 4; ++i) {
        RECT rc = mk((float)a.rcTabs.left + padT + i * (tabW + gapT),
                     (float)a.rcTabs.top + padT, tabW, rectH(a.rcTabs) - 2 * padT);
        addBtn(rc, BTN_TAB_REP + i, BK_TAB, tabNames[i], true, a.tab == i);
    }

    // -------- 右栏操作区 --------
    RECT inner = insetRect(a.rcRight, 22.f * s);
    if (a.tab < 3) {
        float innerLeft = (float)inner.left;
        float innerRight = (float)inner.right;
        float innerBottom = (float)inner.bottom;
        a.playRc = mk(innerLeft, innerBottom - 34.f * s, 34.f * s, 34.f * s);
        a.resetRc = mk((float)a.playRc.right + 10.f * s, (float)a.playRc.top + 2.f * s,
                       30.f * s, 30.f * s);
        float sliderLeft = (float)a.resetRc.right + 20.f * s;
        float sliderW = innerRight - 50.f * s - sliderLeft;
        a.sliderRc = mk(sliderLeft, (float)a.playRc.top + 4.f * s, sliderW, 26.f * s);
        a.sliderVisible = true;
        bool canPlay = a.hasSeq && (a.tab != 2 || !a.tr.codons.empty());
        addBtn(a.playRc, BTN_PLAY, BK_ICON, L"", canPlay, a.playing);
        addBtn(a.resetRc, BTN_RESET, BK_ICON, L"", canPlay);
    }
    if (a.tab == 2) {
        float ty = (float)inner.top + 54.f * s;
        float fx = (float)inner.left + 56.f * s;
        for (int i = 0; i < 3; ++i) {
            RECT rc = mk(fx + i * (36.f * s + 5.f * s), ty, 36.f * s, 26.f * s);
            addBtn(rc, BTN_FRAME1 + i, BK_CHIP, L"+" + std::to_wstring(i + 1), true,
                   a.frame == i);
        }
        float revX = fx + 3 * (36.f * s + 5.f * s) + 14.f * s;
        addBtn(mk(revX, ty, 100.f * s, 26.f * s), BTN_REVCOMP, BK_CHIP, L"反向互补", true,
               a.revcomp);
    }
    if (a.tab == 3) {
        RECT rc = mk((float)inner.right - 116.f * s, (float)inner.top + 1.f * s, 116.f * s,
                     26.f * s);
        addBtn(rc, BTN_RNA_TABLE, BK_CHIP, L"RNA 显示 (U)", true, a.rnaTable);

        float detailH = 52.f * s;
        a.codonDetailRc = mk((float)inner.left, (float)inner.bottom - detailH, rectW(inner),
                             detailH);
        float tableTop = (float)inner.top + 54.f * s;
        float tableBottom = (float)a.codonDetailRc.top - 12.f * s;
        a.codonTableRc = mk((float)inner.left, tableTop, rectW(inner), tableBottom - tableTop);
        float headerH2 = 24.f * s;
        a.codonHeaderRc = mk((float)inner.left, tableTop, rectW(inner), headerH2);
        float prefixW = 56.f * s;
        float colGap = 5.f * s;
        float cellW = (rectW(inner) - prefixW - 4 * colGap) / 4.f;
        float rowGap = 2.f * s;
        float tableH = tableBottom - tableTop;
        float rowH = (tableH - headerH2 - 15 * rowGap) / 16.f;
        if (rowH < 15.f * s) rowH = 15.f * s;
        a.codonRowH = (int)std::lround(rowH);
        for (int f = 0; f < 16; ++f) {
            float cy = tableTop + headerH2 + f * (rowH + rowGap);
            for (int c = 0; c < 4; ++c) {
                float cx = (float)inner.left + prefixW + colGap + c * (cellW + colGap);
                a.codonCellRects[f * 4 + c] = mk(cx, cy, cellW, rowH);
            }
        }
        a.codonCellsValid = true;
    }

    // -------- 编辑框位置 --------
    if (a.hEdit) {
        RECT cur;
        GetWindowRect(a.hEdit, &cur);
        MapWindowPoints(HWND_DESKTOP, a.hwnd, (LPPOINT)&cur, 2);
        if (cur.left != a.rcEdit.left || cur.top != a.rcEdit.top || cur.right != a.rcEdit.right ||
            cur.bottom != a.rcEdit.bottom) {
            MoveWindow(a.hEdit, a.rcEdit.left, a.rcEdit.top, (int)rectW(a.rcEdit),
                       (int)rectH(a.rcEdit), TRUE);
            RECT m;
            m.left = (LONG)(10.f * s);
            m.top = (LONG)(8.f * s);
            m.right = (LONG)(rectW(a.rcEdit) - 10.f * s);
            m.bottom = (LONG)(rectH(a.rcEdit) - 6.f * s);
            SendMessageW(a.hEdit, EM_SETRECT, 0, (LPARAM)&m);
        }
    }
}

// ------------------------------------------------------------------ 绘制：通用按钮
void drawButton(gd::Graphics& g, App& a, const Button& b) {
    using namespace ui;
    const float s = a.s;
    bool hovered = b.enabled && a.hoverId == b.id;
    RectF r = toRectF(b.rc);

    if (b.kind == BK_ICON) {
        if (b.id == BTN_PLAY) {
            Color bg = b.enabled ? (hovered ? mix(ACCENT, Color(255, 255, 255, 255), 0.10f)
                                            : ACCENT)
                                 : Color(255, 211, 220, 232);
            fillRound(g, r, r.Height / 2.f, bg);
            Color fg = b.enabled ? Color(255, 255, 255, 255) : Color(255, 248, 250, 253);
            playPauseIcon(g, RectF(r.X + 3 * s, r.Y + 3 * s, r.Width - 6 * s, r.Height - 6 * s),
                          a.playing, fg);
        } else if (b.id == BTN_RESET) {
            Color bg = hovered ? Color(255, 234, 240, 250) : Color(255, 243, 246, 251);
            fillRound(g, r, r.Height / 2.f, bg);
            strokeRound(g, r, r.Height / 2.f, Color(255, 229, 235, 244), 1.f);
            resetIcon(g, r, b.enabled ? Color(255, 86, 104, 136) : Color(255, 196, 204, 216));
        }
        return;
    }

    if (b.kind == BK_TAB) {
        if (b.active) {
            shadow(g, r, 9 * s, 4, 24);
            fillRound(g, r, 9 * s, Color(255, 255, 255, 255));
            text(g, b.label, r, font(12 * s, true), ACCENT, StringAlignmentCenter,
                 StringAlignmentCenter, true);
        } else {
            if (hovered) fillRound(g, r, 9 * s, Color(150, 255, 255, 255));
            text(g, b.label, r, font(11.5f * s, false), hovered ? TXT : SUB,
                 StringAlignmentCenter, StringAlignmentCenter, true);
        }
        return;
    }

    Color fill;
    Color border;
    Color txt;
    float radius = 8 * s;

    switch (b.kind) {
        case BK_GREEN:
            if (!b.enabled) { fill = Color(255, 240, 243, 248); border = Color(255, 231, 236, 244); txt = Color(255, 178, 188, 203); }
            else { fill = hovered ? mix(GREEN, Color(255, 255, 255, 255), 0.08f) : GREEN;
                   border = mix(fill, Color(0, 0, 0, 0), 0.f); txt = Color(255, 255, 255, 255); }
            radius = 10 * s;
            break;
        case BK_PRIMARY:
            fill = b.enabled ? (hovered ? mix(ACCENT, Color(255, 255, 255, 255), 0.10f) : ACCENT)
                             : Color(255, 228, 233, 242);
            border = fill;
            txt = b.enabled ? Color(255, 255, 255, 255) : Color(255, 170, 180, 196);
            break;
        case BK_SMALL:
            fill = b.enabled ? (hovered ? Color(255, 235, 241, 251) : Color(255, 245, 248, 252))
                             : Color(255, 248, 250, 253);
            border = Color(255, 236, 241, 249);
            txt = b.enabled ? (hovered ? ACCENT_DARK : Color(255, 71, 87, 112))
                            : Color(255, 180, 190, 205);
            radius = 7 * s;
            break;
        case BK_CHIP:
            if (b.active) {
                fill = ACCENT_SOFT;
                border = alpha(ACCENT, 80);
                txt = ACCENT;
            } else {
                fill = hovered ? Color(255, 239, 244, 252) : Color(255, 247, 249, 252);
                border = Color(255, 229, 235, 244);
                txt = Color(255, 86, 100, 124);
            }
            radius = 8 * s;
            break;
        case BK_SECONDARY:
        default:
            fill = b.enabled ? (hovered ? Color(255, 246, 250, 255) : Color(255, 255, 255, 255))
                             : Color(255, 249, 251, 254);
            border = b.enabled ? (hovered ? alpha(ACCENT, 90) : Color(255, 217, 225, 238))
                               : Color(255, 234, 239, 246);
            txt = b.enabled ? Color(255, 58, 74, 98) : Color(255, 176, 186, 201);
            break;
    }
    fillRound(g, r, radius, fill);
    if (b.kind != BK_GREEN) strokeRound(g, r, radius, border, 1.f);

    if (b.kind == BK_GREEN && b.enabled) {
        // 顶部高光
        fillRound(g, RectF(r.X + 1, r.Y + 1, r.Width - 2, r.Height * 0.45f), radius - 1,
                  Color(26, 255, 255, 255));
    }

    float fs = (b.kind == BK_GREEN) ? 12.2f * s : (b.kind == BK_SMALL ? 9.8f * s : 10.6f * s);
    text(g, b.label, r, font(fs, b.kind != BK_SMALL),
         txt, StringAlignmentCenter, StringAlignmentCenter, true);
}

// ------------------------------------------------------------------ 绘制：页头/页脚/左栏/标签
static void drawHeader(App& a, gd::Graphics& g) {
    using namespace ui;
    float s = a.s;
    RectF hdr = toRectF(a.rcHeader);
    LinearGradientBrush brush(hdr, HEADER_A, HEADER_B, LinearGradientModeHorizontal);
    g.FillRectangle(&brush, hdr);

    SolidBrush glow(Color(20, 64, 136, 255));
    g.FillEllipse(&glow, hdr.Width - 430 * s, -170 * s, 520 * s, 380 * s);
    SolidBrush glow2(Color(16, 29, 120, 220));
    g.FillEllipse(&glow2, hdr.Width - 240 * s, 18 * s, 320 * s, 260 * s);

    // 微弱的螺旋装饰
    Pen deco(Color(26, 110, 170, 255), 1.2f);
    float baseY = hdr.Height * 0.5f;
    for (int k = 0; k < 2; ++k) {
        GraphicsPath path;
        for (int i = 0; i <= 60; ++i) {
            float t = i / 60.f;
            float x = hdr.Width - 330 * s + t * 260 * s;
            float y = baseY + std::sin(t * 3.14159f * 3 + k * 1.2f) * 26 * s;
            if (i == 0) path.StartFigure();
            path.AddLine(x, y, x, y);
        }
        g.DrawPath(&deco, &path);
    }

    RectF icon(hdr.X + 20 * s, hdr.Y + (hdr.Height - 38 * s) / 2, 38 * s, 38 * s);
    dnaIcon(g, icon, Color(255, 118, 220, 255), Color(255, 128, 255, 218));

    RectF titleR(hdr.X + 72 * s, hdr.Y + 12 * s, hdr.Width - 400 * s, 26 * s);
    text(g, L"DNA 表达模拟器", titleR, font(19 * s, true), Color(255, 255, 255, 255),
         StringAlignmentNear, StringAlignmentCenter, false);
    RectF subR(hdr.X + 73 * s, hdr.Y + 39 * s, hdr.Width - 400 * s, 18 * s);
    text(g, L"复制 · 转录 · 翻译    |    DNA → mRNA → 蛋白质", subR, font(10.2f * s),
         Color(255, 147, 177, 214), StringAlignmentNear, StringAlignmentCenter, true);

    std::wstring tag = L"中心法则模拟  ·  v1.0";
    Font* tf = font(9.8f * s);
    float tw = textWidth(g, tag, tf);
    RectF pill(hdr.Width - tw - 48 * s, hdr.Y + (hdr.Height - 26 * s) / 2, tw + 24 * s, 26 * s);
    fillRound(g, pill, 13 * s, Color(49, 255, 255, 255));
    strokeRound(g, pill, 13 * s, Color(60, 255, 255, 255), 1.f);
    text(g, tag, pill, tf, Color(255, 196, 214, 239), StringAlignmentCenter,
         StringAlignmentCenter, true);
}

static void drawTabsBar(App& a, gd::Graphics& g) {
    using namespace ui;
    RectF bar = toRectF(a.rcTabs);
    fillRound(g, bar, 12 * a.s, Color(255, 232, 237, 246));
}

static void drawFooter(App& a, gd::Graphics& g) {
    using namespace ui;
    float s = a.s;
    RectF lineR((float)a.rcFooter.left + 18 * s, (float)a.rcFooter.top,
                (float)a.cw - 36 * s, 1.f);
    SolidBrush lineBrush(Color(255, 222, 228, 238));
    g.FillRectangle(&lineBrush, lineR);
    bool fresh = GetTickCount64() - a.statusAt < 6000;
    dot(g, (float)a.rcFooter.left + 22 * s, (float)a.rcFooter.top + 13 * s, 2.6f * s,
        fresh ? ACCENT : Color(255, 190, 199, 214));
    RectF sr((float)a.rcFooter.left + 31 * s, (float)a.rcFooter.top, (float)a.cw * 0.68f,
             26 * s);
    text(g, a.status.empty() ? L"就绪" : a.status, sr, font(9.6f * s),
         fresh ? Color(255, 89, 105, 132) : MUTED, StringAlignmentNear,
         StringAlignmentCenter, true);
    RectF hint((float)a.cw * 0.62f, (float)a.rcFooter.top, (float)a.cw - (float)a.cw * 0.62f - 18 * s,
               26 * s);
    text(g, L"FASTA / TXT 导入导出 · C++ / Win32 / GDI+", hint, font(9.2f * s),
         Color(255, 161, 173, 192), StringAlignmentFar, StringAlignmentCenter, true);
}

static void drawTile(gd::Graphics& g, App& a, const RECT& rc, const std::wstring& label,
                     const std::wstring& value, ui::Color accent, const std::wstring& tag) {
    using namespace ui;
    float s = a.s;
    RectF r = toRectF(rc);
    fillRound(g, r, 9 * s, Color(255, 249, 251, 254));
    strokeRound(g, r, 9 * s, Color(255, 235, 240, 248), 1.f);
    RectF lr(r.X + 11 * s, r.Y + 7 * s, r.Width - 22 * s, 14 * s);
    text(g, label, lr, font(9.3f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, true);
    if (!tag.empty()) {
        Font* tf = font(8.4f * s, true, true);
        float tw = textWidth(g, tag, tf) + 10 * s;
        RectF tr(r.X + r.Width - tw - 8 * s, r.Y + 6 * s, tw, 14 * s);
        fillRound(g, tr, 7 * s, ACCENT_SOFT);
        text(g, tag, tr, tf, ACCENT, StringAlignmentCenter, StringAlignmentCenter, false);
    }
    RectF vr(r.X + 11 * s, r.Y + 21 * s, r.Width - 22 * s, 24 * s);
    text(g, value, vr, font(14.6f * s, true, false), accent, StringAlignmentNear,
         StringAlignmentCenter, true);
}

static void drawLeftCard(App& a, gd::Graphics& g) {
    using namespace ui;
    float s = a.s;
    RectF card = toRectF(a.rcLeft);
    shadow(g, card, 14 * s, 9, 18);
    fillRound(g, card, 14 * s, CARD);
    strokeRound(g, card, 14 * s, BORDER, 1.f);

    // 标题
    text(g, L"DNA 序列", toRectF(a.rcLeftTitle), font(15.f * s, true), TXT,
         StringAlignmentNear, StringAlignmentCenter, true);
    RectF chip = toRectF(a.rcTitleChip);
    fillRound(g, chip, 10 * s, ACCENT_SOFT);
    text(g, L"编码链 5' → 3'", chip, font(8.8f * s, true), ACCENT, StringAlignmentCenter,
         StringAlignmentCenter, true);

    // 编辑区外观
    RectF ef = toRectF(a.rcEditFrame);
    fillRound(g, ef, 10 * s, Color(255, 251, 252, 254));
    strokeRound(g, ef, 10 * s,
                gEditFocused ? alpha(ACCENT, 150) : Color(255, 224, 231, 241),
                gEditFocused ? 1.5f : 1.f);

    // 序列分析
    text(g, L"序列分析", toRectF(a.rcStatsHeader), font(11.2f * s, true),
         Color(255, 74, 90, 116), StringAlignmentNear, StringAlignmentCenter, false);
    RectF hdrR = toRectF(a.rcStatsHeader);
    text(g, L"当前编码链", RectF(hdrR.X, hdrR.Y, hdrR.Width, hdrR.Height), font(8.9f * s),
         MUTED, StringAlignmentFar, StringAlignmentCenter, false);

    std::wstring lenVal = a.hasSeq ? (std::to_wstring(a.stats.length) + L" bp") : L"—";
    wchar_t gcBuf[32];
    if (a.hasSeq) swprintf(gcBuf, 32, L"%.1f%%", a.stats.gc * 100.0);
    else wcscpy(gcBuf, L"—");
    std::wstring aaVal = (a.hasSeq && !a.tr.protein.empty())
                             ? (std::to_wstring(a.tr.protein.size()) + L" aa")
                             : L"—";
    std::wstring frameTag = L"+" + std::to_wstring(a.frame + 1);
    drawTile(g, a, a.rcTiles[0], L"序列长度", lenVal, TXT, L"");
    drawTile(g, a, a.rcTiles[1], L"GC 含量", gcBuf, ACCENT, L"");
    drawTile(g, a, a.rcTiles[2], L"多肽长度", aaVal,
             a.hasSeq && !a.tr.protein.empty() ? GREEN : TXT, frameTag);

    // 碱基组成
    RectF bar = toRectF(a.rcCompBar);
    fillRound(g, bar, 4 * s, Color(255, 238, 242, 248));
    int counts[4] = {a.stats.countA, a.stats.countT, a.stats.countG, a.stats.countC};
    const char bases[4] = {'A', 'T', 'G', 'C'};
    int total = a.stats.length > 0 ? a.stats.length : 1;
    float x = bar.X;
    float gapPx = 2 * s;
    for (int i = 0; i < 4; ++i) {
        float w = (bar.Width - 4 * gapPx) * (float)counts[i] / (float)total;
        if (counts[i] > 0 && w < 2 * s) w = 2 * s;
        if (w > 0) {
            RectF seg(x, bar.Y, w, bar.Height);
            fillRound(g, seg, 3 * s, alpha(baseColor(bases[i]), 200));
            x += w + gapPx;
        }
    }
    RectF labels = toRectF(a.rcCompLabels);
    int slotsN = a.stats.countN > 0 ? 5 : 4;
    float slotW = labels.Width / slotsN;
    for (int i = 0; i < slotsN; ++i) {
        float sx = labels.X + i * slotW;
        char base = i < 4 ? bases[i] : 'N';
        int count = i < 4 ? counts[i] : a.stats.countN;
        std::wstring letter(1, (wchar_t)base);
        Font* lf = font(9.2f * s, true, true);
        text(g, letter, RectF(sx, labels.Y, slotW, labels.Height), lf, baseColor(base),
             StringAlignmentNear, StringAlignmentCenter, false);
        float lw2 = textWidth(g, letter, lf);
        text(g, std::to_wstring(count), RectF(sx + lw2 + 3 * s, labels.Y, slotW - lw2 - 3 * s,
                                               labels.Height),
             font(9.2f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, false);
    }
    // 若没有序列，仍显示 0 计数（保留布局稳定）

    // 警示条
    if (a.warnVisible) {
        RectF wr = toRectF(a.rcWarn);
        bool isError = !a.errorText.empty();
        Color accent = isError ? RED : AMBER;
        fillRound(g, wr, 9 * s, isError ? RED_SOFT : AMBER_SOFT);
        strokeRound(g, wr, 9 * s, alpha(accent, 70), 1.f);
        dot(g, wr.X + 16 * s, wr.Y + wr.Height / 2, 8 * s, alpha(accent, 210));
        text(g, L"!", RectF(wr.X + 8 * s, wr.Y + wr.Height / 2 - 8 * s, 16 * s, 16 * s),
             font(9.2f * s, true), Color(255, 255, 255, 255), StringAlignmentCenter,
             StringAlignmentCenter, false);
        std::wstring msg = isError ? a.errorText : a.warnings[0];
        if (!isError && a.warnings.size() > 1)
            msg += L"（共 " + std::to_wstring(a.warnings.size()) + L" 条提示）";
        StringFormat fmt;
        fmt.SetFormatFlags(0);
        fmt.SetTrimming(StringTrimmingEllipsisWord);
        fmt.SetLineAlignment(StringAlignmentCenter);
        RectF tr(wr.X + 30 * s, wr.Y + 4 * s, wr.Width - 38 * s, wr.Height - 8 * s);
        SolidBrush br(Color(255, 100, 76, 18));
        Font* wf = font(9.3f * s);
        std::wstring wmsg = msg;
        g.DrawString(wmsg.c_str(), (INT)wmsg.size(), wf, tr, &fmt, &br);
    }

    // 导出说明
    text(g, L"根据当前阅读框导出 · 自动写入统计与注释信息", toRectF(a.rcCaption),
         font(8.4f * s), MUTED, StringAlignmentCenter, StringAlignmentCenter, true);
}

// ------------------------------------------------------------------ 绘制入口
static void prepareGraphics(gd::Graphics& g) {
    g.SetSmoothingMode(gd::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(gd::PixelOffsetModeHalf);
    g.SetCompositingQuality(gd::CompositingQualityHighQuality);
    g.SetInterpolationMode(gd::InterpolationModeHighQualityBilinear);
    g.SetCompositingMode(gd::CompositingModeSourceOver);
}

static void destroyBuffers(App& a) {
    delete a.backGfx;
    a.backGfx = nullptr;
    delete a.staticGfx;
    a.staticGfx = nullptr;
    if (a.backDC) {
        if (a.backOld) SelectObject(a.backDC, a.backOld);
        if (a.backDib) DeleteObject(a.backDib);
        DeleteDC(a.backDC);
        a.backDC = nullptr;
        a.backDib = nullptr;
        a.backOld = nullptr;
    }
    if (a.staticDC) {
        if (a.staticOld) SelectObject(a.staticDC, a.staticOld);
        if (a.staticDib) DeleteObject(a.staticDib);
        DeleteDC(a.staticDC);
        a.staticDC = nullptr;
        a.staticDib = nullptr;
        a.staticOld = nullptr;
    }
    a.bufW = 0;
    a.bufH = 0;
}

// 使用 GDI DIB + BitBlt，而不是逐帧 GDI+ DrawImage（后者在 HDC 上慢一个数量级）
static void ensureBuffers(App& a, int w, int h) {
    if (a.backDC && a.bufW == w && a.bufH == h) return;
    destroyBuffers(a);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    a.backDC = CreateCompatibleDC(nullptr);
    a.staticDC = CreateCompatibleDC(nullptr);
    a.backDib = CreateDIBSection(a.backDC, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
    a.staticDib = CreateDIBSection(a.staticDC, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
    if (a.backDib) a.backOld = SelectObject(a.backDC, a.backDib);
    if (a.staticDib) a.staticOld = SelectObject(a.staticDC, a.staticDib);
    a.backGfx = new gd::Graphics(a.backDC);
    a.staticGfx = new gd::Graphics(a.staticDC);
    a.bufW = w;
    a.bufH = h;
    a.staticDirty = true;
}

// 静态层：页头、左栏、标签底、卡片底与不随动画变化的按钮。
static void drawStaticLayer(App& a, gd::Graphics& g) {
    using namespace ui;
    g.Clear(BG);

    drawHeader(a, g);
    drawTabsBar(a, g);
    drawLeftCard(a, g);

    RectF right = toRectF(a.rcRight);
    shadow(g, right, 14 * a.s, 9, 18);
    fillRound(g, right, 14 * a.s, CARD);
    strokeRound(g, right, 14 * a.s, BORDER, 1.f);
}

static bool rectsIntersect(const RECT& a, const RECT& b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

// 动态层：当前视图、全部按钮与页脚。播放时仅重绘与脏区相交的部分。
static void drawDynamicLayer(App& a, gd::Graphics& g) {
    if (rectsIntersect(a.rcRight, a.paintClip)) drawViews(a, g, a.rcRight);
    for (const auto& b : a.buttons) {
        if (rectsIntersect(b.rc, a.paintClip)) drawButton(g, a, b);
    }
    if (rectsIntersect(a.rcFooter, a.paintClip)) drawFooter(a, g);
}

void drawApp(App& a, gd::Graphics& g) {
    a.paintClip = {0, 0, a.bufW ? a.bufW : a.cw, a.bufH ? a.bufH : a.ch};
    prepareGraphics(g);
    drawStaticLayer(a, g);
    drawDynamicLayer(a, g);
}

// ------------------------------------------------------------------ 交互
static void cycleTab(App& a, int tab) {
    if (tab == a.tab) return;
    stopPlaying(a);
    a.tab = tab;
    a.hoverCodon = -1;
    buildLayout(a);
    InvalidateRect(a.hwnd, nullptr, FALSE);
}

static void flushPendingEdit(App& a) {
    if (!gEditPending) return;
    gEditPending = false;
    if (a.hwnd) KillTimer(a.hwnd, TIMER_EDIT);
    recomputeFromEdit(a);
    for (int i = 0; i < 3; ++i) a.progress[i] = 0.f;
    buildLayout(a);
    InvalidateRect(a.hwnd, nullptr, FALSE);
}

static void handleCommand(App& a, int id) {
    flushPendingEdit(a);
    switch (id) {
        case BTN_IMPORT:
            if (importDnaFile(a)) {
                a.frame = 0;
                a.revcomp = false;
                recomputeFromEdit(a);
                for (int i = 0; i < 3; ++i) a.progress[i] = 0;
                buildLayout(a);
                InvalidateRect(a.hwnd, nullptr, FALSE);
            }
            break;
        case BTN_EXPORT_DNA:
            exportDnaFile(a);
            break;
        case BTN_EXPORT_PROTEIN:
            exportProteinFile(a);
            break;
        case BTN_SAMPLE1: loadSample(a, 1); break;
        case BTN_SAMPLE2: loadSample(a, 2); break;
        case BTN_SAMPLE3: loadSample(a, 3); break;
        case BTN_RANDOM: {
            std::string seq = dna::randomGene(28);
            a.frame = 0;
            a.revcomp = false;
            setEditText(a, fastaWithHeader(seq, L">random_gene | " +
                                                std::to_wstring(seq.size()) + L" bp"));
            recomputeFromEdit(a);
            for (int i = 0; i < 3; ++i) a.progress[i] = 0;
            buildLayout(a);
            setStatus(a, L"已生成随机基因 · 29 aa 编码区（合成序列）");
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        }
        case BTN_CLEAR:
            stopPlaying(a);
            setEditText(a, L"");
            recomputeFromEdit(a);
            for (int i = 0; i < 3; ++i) a.progress[i] = 0;
            buildLayout(a);
            setStatus(a, L"已清空序列");
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        case BTN_TAB_REP: cycleTab(a, 0); break;
        case BTN_TAB_TRA: cycleTab(a, 1); break;
        case BTN_TAB_TRL: cycleTab(a, 2); break;
        case BTN_TAB_CODE:
            stopPlaying(a);
            cycleTab(a, 3);
            break;
        case BTN_PLAY:
            if (!a.hasSeq || a.tab > 2) break;
            if (a.playing) stopPlaying(a);
            else startPlaying(a);
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        case BTN_RESET:
            if (a.tab > 2) break;
            stopPlaying(a);
            a.progress[a.tab] = 0;
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        case BTN_FRAME1:
        case BTN_FRAME2:
        case BTN_FRAME3: {
            int f = id - BTN_FRAME1;
            if (a.frame != f) {
                a.frame = f;
                recomputeFromEdit(a);
                for (int i = 0; i < 3; ++i) a.progress[i] = 0;
                buildLayout(a);
                setStatus(a, L"阅读框 +" + std::to_wstring(f + 1) + L" · 已重新翻译");
                InvalidateRect(a.hwnd, nullptr, FALSE);
            }
            break;
        }
        case BTN_REVCOMP:
            a.revcomp = !a.revcomp;
            recomputeFromEdit(a);
            for (int i = 0; i < 3; ++i) a.progress[i] = 0;
            buildLayout(a);
            setStatus(a, a.revcomp ? L"已启用反向互补链" : L"已恢复正向编码链");
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        case BTN_RNA_TABLE:
            a.rnaTable = !a.rnaTable;
            buildLayout(a);
            InvalidateRect(a.hwnd, nullptr, FALSE);
            break;
        default:
            break;
    }
}

static void updateHover(App& a, POINT p) {
    int oldBtn = a.hoverId;
    int oldCodon = a.hoverCodon;
    bool oldSlider = a.hoveringSlider;
    a.hoverId = hitButton(a, p);
    a.hoverCodon = hitCodon(a, p);
    a.hoveringSlider = a.sliderVisible && hit(expandRect(a.sliderRc, 6 * a.s), p);
    bool cursorHand = (a.hoverId != 0) || (a.hoverCodon >= 0) || a.hoveringSlider ||
                      (a.tab == 3 && a.hoverCodon >= 0);
    SetCursor(LoadCursorW(nullptr, cursorHand ? IDC_HAND : IDC_ARROW));
    if (oldBtn != a.hoverId || oldCodon != a.hoverCodon || oldSlider != a.hoveringSlider)
        InvalidateRect(a.hwnd, nullptr, FALSE);
}

static void setProgressFromPoint(App& a, POINT p) {
    if (a.tab > 2 || !a.sliderVisible) return;
    float left = (float)a.sliderRc.left + 6 * a.s;
    float right = (float)a.sliderRc.right - 6 * a.s;
    float t = ((float)p.x - left) / std::max(1.f, right - left);
    t = std::max(0.f, std::min(1.f, t));
    a.progress[a.tab] = t;
    stopPlaying(a);
    InvalidateRect(a.hwnd, &a.rcRight, FALSE);
}

// ------------------------------------------------------------------ 窗口过程
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App& a = gApp;
    switch (msg) {
        case WM_CREATE: {
            a.hwnd = hwnd;
            RECT rc;
            GetClientRect(hwnd, &rc);
            a.cw = rc.right ? rc.right : 1280;
            a.ch = rc.bottom ? rc.bottom : 820;
            a.editBrush = CreateSolidBrush(RGB(251, 252, 254));
            a.hEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL |
                    WS_VSCROLL | WS_TABSTOP,
                0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)EDIT_ID, GetModuleHandleW(nullptr),
                nullptr);
            if (a.hEdit) {
                SendMessageW(a.hEdit, EM_SETLIMITTEXT, 60000, 0);
                SendMessageW(a.hEdit, EM_SETCUEBANNER, TRUE,
                             (LPARAM)L"在此粘贴或输入 DNA 序列（A/T/G/C/U/N）…");
                int h = -(int)std::lround(15.f * a.s);
                a.editFont = CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         FIXED_PITCH | FF_MODERN, L"Consolas");
                SendMessageW(a.hEdit, WM_SETFONT, (WPARAM)a.editFont, TRUE);
            }
            setStatus(a, L"就绪 · 支持直接输入、粘贴或导入 FASTA / TXT 序列");
            loadSample(a, 1);
            a.tab = 2;
            a.progress[2] = 0.506f;
            a.selectedCodon = dna::displayIndexOf("ATG");
            a.playing = false;
            setStatus(a, L"已载入示例 1 · 点击播放查看翻译动画，或直接编辑左侧序列");
            buildLayout(a);
            SetFocus(hwnd);
            return 0;
        }
        case WM_SIZE: {
            if (wp == SIZE_MINIMIZED) return 0;
            int nw = LOWORD(lp), nh = HIWORD(lp);
            if (nw > 0 && nh > 0) {
                a.cw = nw;
                a.ch = nh;
                buildLayout(a);
                a.staticDirty = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            UINT dpi = HIWORD(wp);
            if (dpi) {
                a.dpi = (int)dpi;
                a.s = (float)dpi / 96.0f;
                if (a.hEdit) {
                    if (a.editFont) DeleteObject(a.editFont);
                    int h = -(int)std::lround(15.f * a.s);
                    a.editFont = CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                             FIXED_PITCH | FF_MODERN, L"Consolas");
                    SendMessageW(a.hEdit, WM_SETFONT, (WPARAM)a.editFont, TRUE);
                }
                RECT* suggested = (RECT*)lp;
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                buildLayout(a);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)lp;
            UINT dpi = dpiOfWindow(hwnd);
            float sc = (float)dpi / 96.0f;
            RECT r = {0, 0, (LONG)std::lround(1160 * sc), (LONG)std::lround(720 * sc)};
            AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
            mm->ptMinTrackSize.x = r.right - r.left;
            mm->ptMinTrackSize.y = r.bottom - r.top;
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = std::max(1L, rc.right), h = std::max(1L, rc.bottom);
            RECT upd = ps.rcPaint;
            if (upd.right <= upd.left || upd.bottom <= upd.top) {
                EndPaint(hwnd, &ps);
                return 0;
            }
            a.paintClip = upd;

            ensureBuffers(a, w, h);
            if (a.backDC && a.staticDC && a.backDib && a.staticDib && a.backGfx && a.staticGfx) {
                if (a.staticDirty) {
                    prepareGraphics(*a.staticGfx);
                    drawStaticLayer(a, *a.staticGfx);
                    a.staticDirty = false;
                }

                int cw = upd.right - upd.left;
                int chh = upd.bottom - upd.top;
                BitBlt(a.backDC, upd.left, upd.top, cw, chh, a.staticDC, upd.left, upd.top,
                       SRCCOPY);

                int saved = SaveDC(a.backDC);
                IntersectClipRect(a.backDC, upd.left, upd.top, upd.right, upd.bottom);
                prepareGraphics(*a.backGfx);
                gd::Rect clipR(upd.left, upd.top, cw, chh);
                a.backGfx->SetClip(clipR);
                drawDynamicLayer(a, *a.backGfx);
                a.backGfx->ResetClip();
                RestoreDC(a.backDC, saved);

                BitBlt(hdc, upd.left, upd.top, cw, chh, a.backDC, upd.left, upd.top, SRCCOPY);
            } else {
                Gdiplus::Bitmap bmp(w, h, PixelFormat32bppPARGB);
                gd::Graphics g(&bmp);
                drawApp(a, g);
                gd::Graphics screen(hdc);
                screen.DrawImage(&bmp, 0, 0, w, h);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(32, 44, 66));
            SetBkColor(hdc, RGB(251, 252, 254));
            return (LRESULT)(a.editBrush ? a.editBrush : GetStockObject(WHITE_BRUSH));
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == EDIT_ID) {
                if (code == EN_CHANGE) {
                    gEditPending = true;
                    SetTimer(hwnd, TIMER_EDIT, 180, nullptr);
                    a.staticDirty = true;
                } else if (code == EN_SETFOCUS) {
                    gEditFocused = true;
                    a.staticDirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (code == EN_KILLFOCUS) {
                    gEditFocused = false;
                    a.staticDirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            handleCommand(a, id);
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_ANIM) updateAnimation(a);
            else if (wp == TIMER_EDIT) flushPendingEdit(a);
            return 0;
        case WM_MOUSEMOVE: {
            POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (!gTrackingLeave) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                gTrackingLeave = true;
            }
            if (a.draggingSlider) {
                setProgressFromPoint(a, p);
            } else {
                updateHover(a, p);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            gTrackingLeave = false;
            if (!a.draggingSlider) {
                if (a.hoverId || a.hoverCodon >= 0 || a.hoveringSlider) {
                    a.hoverId = 0;
                    a.hoverCodon = -1;
                    a.hoveringSlider = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (a.sliderVisible && hit(expandRect(a.sliderRc, 8 * a.s), p)) {
                a.draggingSlider = true;
                a.dragTab = a.tab;
                SetCapture(hwnd);
                setProgressFromPoint(a, p);
                return 0;
            }
            int id = hitButton(a, p);
            if (id) {
                gPressedBtn = id;
                return 0;
            }
            if (a.tab == 3) {
                int c = hitCodon(a, p);
                if (c >= 0) {
                    a.selectedCodon = c;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (a.draggingSlider) {
                a.draggingSlider = false;
                ReleaseCapture();
                return 0;
            }
            POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (gPressedBtn) {
                int id = hitButton(a, p);
                if (id == gPressedBtn) handleCommand(a, id);
                gPressedBtn = 0;
            }
            updateHover(a, p);
            return 0;
        }
        case WM_CAPTURECHANGED:
            a.draggingSlider = false;
            return 0;
        case WM_KEYDOWN: {
            if (GetFocus() == a.hEdit) break;
            if (wp == VK_SPACE) {
                handleCommand(a, BTN_PLAY);
                return 0;
            }
            if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_HOME || wp == VK_END) {
                if (a.tab > 2) return 0;
                float& p = a.progress[a.tab];
                if (wp == VK_LEFT) p = std::max(0.f, p - 0.02f);
                if (wp == VK_RIGHT) p = std::min(1.f, p + 0.02f);
                if (wp == VK_HOME) p = 0.f;
                if (wp == VK_END) p = 1.f;
                stopPlaying(a);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lp) == HTCLIENT) {
                if (a.hoverId || a.hoverCodon >= 0 || a.hoveringSlider || a.draggingSlider) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        }
        case WM_DESTROY:
            stopPlaying(a);
            destroyBuffers(a);
            if (a.editBrush) DeleteObject(a.editBrush);
            if (a.editFont) DeleteObject(a.editFont);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------------ 启动参数
struct Options {
    int tab = -1;
    int sample = -1;
    bool seqGiven = false;
    std::string seq;
    float progress = -1.f;
    int frame = -1;
    int revcomp = -1;
    bool rna = false;
    bool play = false;
    std::wstring exportProtein;
    std::wstring exportDna;
    bool exitAfter = false;
};

static Options parseCommandLine() {
    Options o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return o;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        auto starts = [&](const wchar_t* p) { return arg.rfind(p, 0) == 0; };
        if (arg == L"--play") o.play = true;
        else if (arg == L"--revcomp") o.revcomp = 1;
        else if (arg == L"--rna") o.rna = true;
        else if (arg == L"--exit") o.exitAfter = true;
        else if (starts(L"--tab=")) {
            std::wstring v = arg.substr(6);
            if (v == L"replication" || v == L"复制") o.tab = 0;
            else if (v == L"transcription" || v == L"转录") o.tab = 1;
            else if (v == L"translation" || v == L"翻译") o.tab = 2;
            else if (v == L"codons" || v == L"codon" || v == L"密码子表") o.tab = 3;
            else o.tab = std::max(0, std::min(3, _wtoi(v.c_str())));
        } else if (starts(L"--sample=")) {
            o.sample = _wtoi(arg.substr(9).c_str());
        } else if (starts(L"--seq=")) {
            o.seqGiven = true;
            o.seq = ui::narrow(arg.substr(6));
        } else if (starts(L"--progress=")) {
            o.progress = (float)_wtof(arg.substr(11).c_str());
        } else if (starts(L"--frame=")) {
            int f = _wtoi(arg.substr(8).c_str());
            o.frame = std::max(0, std::min(2, f - 1));
        } else if (starts(L"--export-protein=")) {
            o.exportProtein = arg.substr(17);
        } else if (starts(L"--export-dna=")) {
            o.exportDna = arg.substr(13);
        }
    }
    LocalFree(argv);
    return o;
}

static void applyOptions(App& a, const Options& o) {
    if (o.sample > 0) loadSample(a, std::min(3, o.sample));
    else if (o.seqGiven) applySequence(a, o.seq);
    if (o.frame >= 0) a.frame = o.frame;
    if (o.revcomp == 1) a.revcomp = true;
    if (o.rna) a.rnaTable = true;
    if (o.tab >= 0) a.tab = o.tab;
    recomputeFromEdit(a);
    if (o.progress >= 0) {
        float p = std::max(0.f, std::min(1.f, o.progress));
        for (int i = 0; i < 3; ++i) a.progress[i] = p;
    }
    buildLayout(a);

    std::wstring err;
    if (!o.exportProtein.empty()) {
        if (writeProteinFile(a, o.exportProtein, &err)) {
            setStatus(a, L"蛋白质序列已导出到 " + o.exportProtein);
        } else setStatus(a, L"导出失败：" + err);
    }
    if (!o.exportDna.empty()) {
        if (writeDnaFile(a, o.exportDna, &err)) {
            setStatus(a, L"DNA 序列已导出到 " + o.exportDna);
        } else setStatus(a, L"导出失败：" + err);
    }
    InvalidateRect(a.hwnd, nullptr, FALSE);
    if (o.play) startPlaying(a);
    if (o.exitAfter) PostMessageW(a.hwnd, WM_CLOSE, 0, 0);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    enableDpiAwareness();
    gd::GdiplusStartupInput gsi;
    if (gd::GdiplusStartup(&gGdiToken, &gsi, nullptr) != gd::Ok) return 1;
    timeBeginPeriod(1);   // 提高 WM_TIMER 精度，避免动画步进不均
    ui::font(12.f);   // 预热字体缓存

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"DNAExpressionSimWnd";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    UINT dpi = dpiOfSystem();
    gApp.dpi = (int)dpi;
    gApp.s = (float)dpi / 96.0f;

    int targetW = (int)std::lround(1260.f * gApp.s);
    int targetH = (int)std::lround(810.f * gApp.s);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int maxW = work.right - work.left - 40;
    int maxH = work.bottom - work.top - 40;
    targetW = std::min(targetW, maxW);
    targetH = std::min(targetH, maxH);
    RECT wr = {0, 0, targetW, targetH};
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int winW = wr.right - wr.left, winH = wr.bottom - wr.top;
    int x = work.left + std::max(0L, (work.right - work.left - winW) / 2L);
    int y = work.top + std::max(0L, (work.bottom - work.top - winH) / 2L);

    HWND hwnd = CreateWindowExW(0, L"DNAExpressionSimWnd",
                                L"DNA 表达模拟器 · DNA Expression Simulator",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, winW, winH,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        ui::clearFontCache();
        gd::GdiplusShutdown(gGdiToken);
        return 1;
    }
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    Options opts = parseCommandLine();
    applyOptions(gApp, opts);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ui::clearFontCache();
    timeEndPeriod(1);
    gd::GdiplusShutdown(gGdiToken);
    return 0;
}
