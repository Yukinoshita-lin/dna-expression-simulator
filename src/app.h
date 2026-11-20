// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// app.h — 应用状态、布局与跨模块接口
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtypes.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "genetics.h"

namespace gd = Gdiplus;

enum ButtonId : int {
    BTN_NONE = 0,
    BTN_IMPORT = 1,
    BTN_EXPORT_DNA,
    BTN_EXPORT_PROTEIN,
    BTN_SAMPLE1,
    BTN_SAMPLE2,
    BTN_SAMPLE3,
    BTN_RANDOM,
    BTN_CLEAR,
    BTN_TAB_REP,
    BTN_TAB_TRA,
    BTN_TAB_TRL,
    BTN_TAB_CODE,
    BTN_PLAY,
    BTN_RESET,
    BTN_FRAME1,
    BTN_FRAME2,
    BTN_FRAME3,
    BTN_REVCOMP,
    BTN_RNA_TABLE,
    BTN_CODON_BASE = 300
};

enum ButtonKind {
    BK_PRIMARY,
    BK_GREEN,
    BK_SECONDARY,
    BK_SMALL,
    BK_GHOST,
    BK_TAB,
    BK_CHIP,
    BK_ICON
};

struct Button {
    RECT rc{};
    int id = 0;
    ButtonKind kind = BK_SECONDARY;
    std::wstring label;
    bool enabled = true;
    bool active = false;
};

struct App {
    HWND hwnd = nullptr;
    HWND hEdit = nullptr;
    HBRUSH editBrush = nullptr;
    HFONT editFont = nullptr;

    int dpi = 96;
    float s = 1.0f;
    int cw = 1280;
    int ch = 820;

    int tab = 0;
    bool playing = false;
    float progress[3] = {0.f, 0.f, 0.f};
    ULONGLONG lastTick = 0;
    bool draggingSlider = false;

    int frame = 0;
    bool revcomp = false;
    bool rnaTable = false;
    int selectedCodon = -1;

    int hoverId = 0;
    int hoverCodon = -1;
    bool hoveringSlider = false;

    dna::ParseResult parsed;
    dna::SequenceStats stats;
    dna::TranslationResult tr;
    std::string coding;
    std::string templ;
    std::string mrna;
    bool hasSeq = false;
    std::wstring errorText;             // 红色错误（解析失败）
    std::vector<std::wstring> warnings; // 琥珀色提示

    std::wstring status;
    ULONGLONG statusAt = 0;

    std::vector<Button> buttons;
    RECT rcHeader{};
    RECT rcFooter{};
    RECT rcLeft{};
    RECT rcTabs{};
    RECT rcRight{};
    RECT rcEditFrame{};
    RECT rcEdit{};
    RECT sliderRc{};
    RECT playRc{};
    RECT resetRc{};
    bool sliderVisible = false;

    RECT codonCellRects[64]{};
    RECT codonTableRc{};
    RECT codonHeaderRc{};
    RECT codonDetailRc{};
    int codonRowH = 0;
    bool codonCellsValid = false;

    RECT rcLeftTitle{};
    RECT rcTitleChip{};
    RECT rcDivider{};
    RECT rcStatsHeader{};
    RECT rcTiles[3]{};
    RECT rcCompBar{};
    RECT rcCompLabels{};
    RECT rcWarn{};
    RECT rcExport{};
    RECT rcCaption{};
    RECT rcRow1[2]{};
    RECT rcRow2[5]{};
    bool warnVisible = false;

    // 动画相关
    float animDuration[3] = {8.f, 8.f, 8.f};

    // 绘制缓存：静态层（页头/左栏/卡片底/标签底）+ 双缓冲
    HDC backDC = nullptr;
    HDC staticDC = nullptr;
    HBITMAP backDib = nullptr;
    HBITMAP staticDib = nullptr;
    HGDIOBJ backOld = nullptr;
    HGDIOBJ staticOld = nullptr;
    gd::Graphics* backGfx = nullptr;
    gd::Graphics* staticGfx = nullptr;
    int bufW = 0;
    int bufH = 0;
    bool staticDirty = true;
    RECT paintClip{0, 0, 0, 0};

    float px(float v) const { return v * s; }
};

// main.cpp
void buildLayout(App& a);
void drawApp(App& a, gd::Graphics& g);
void recomputeModel(App& a);
void recomputeFromEdit(App& a);
void setStatus(App& a, const std::wstring& s);
void setEditText(App& a, const std::wstring& text);
std::wstring getEditText(App& a);
std::wstring formatFasta(const std::string& seq);
std::wstring nowDateTime();
void loadSample(App& a, int idx);
void applySequence(App& a, const std::string& seq);
void updateAnimation(App& a);

// views.cpp
void drawViews(App& a, gd::Graphics& g, const RECT& content);
void drawButton(gd::Graphics& g, App& a, const Button& b);

// fileio.cpp
bool importDnaFile(App& a);
bool exportDnaFile(App& a);
bool exportProteinFile(App& a);
bool writeDnaFile(App& a, const std::wstring& path, std::wstring* err, bool overwrite);
bool writeProteinFile(App& a, const std::wstring& path, std::wstring* err, bool overwrite);
std::wstring fileNameOf(const std::wstring& path);
