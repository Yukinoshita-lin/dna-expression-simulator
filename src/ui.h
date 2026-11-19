// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// ui.h — 主题、字体缓存与 GDI+ 绘制小工具
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
#include <memory>
#include <map>

namespace ui {

using namespace Gdiplus;

// ---- 主题色 ----
extern Color BG;            // 页面底色
extern Color CARD;          // 卡片
extern Color BORDER;
extern Color BORDER_SOFT;
extern Color TXT;
extern Color SUB;
extern Color MUTED;
extern Color ACCENT;
extern Color ACCENT_DARK;
extern Color ACCENT_SOFT;
extern Color GREEN;
extern Color GREEN_SOFT;
extern Color AMBER;
extern Color AMBER_SOFT;
extern Color RED;
extern Color RED_SOFT;
extern Color PURPLE;
extern Color TEAL;
extern Color HEADER_A;
extern Color HEADER_B;
extern Color NEW_STRAND;
extern Color NEW_SOFT;

Color mix(Color a, Color b, float t);
Color alpha(Color c, int a);

// ---- 字体 ----
Font* font(float px, bool bold = false, bool mono = false);
Font* fontSemi(float px, bool mono = false);
float textWidth(Graphics& g, const std::wstring& s, Font* f);

// ---- 基础绘制 ----
RectF toRectF(const RECT& r);
RectF insetRect(const RectF& r, float d);
void fillRound(Graphics& g, const RectF& r, float radius, Color c);
void strokeRound(Graphics& g, const RectF& r, float radius, Color c, float width = 1.0f);
void shadow(Graphics& g, const RectF& r, float radius, int spread = 7, int a = 16);
void text(Graphics& g, const std::wstring& s, const RectF& r, Font* f, Color c,
          StringAlignment ha = StringAlignmentNear, StringAlignment va = StringAlignmentCenter,
          bool ellipsis = true);
void textUtf8(Graphics& g, const std::string& s, const RectF& r, Font* f, Color c,
              StringAlignment ha = StringAlignmentNear, StringAlignment va = StringAlignmentCenter,
              bool ellipsis = true);
void line(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w = 1.0f);
void dashLine(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w = 1.0f,
              DashStyle style = DashStyleDash);
void arrowDown(Graphics& g, float x, float y1, float y2, Color c, float w = 1.6f);
void dot(Graphics& g, float cx, float cy, float r, Color c);

// ---- 生物语义配色 ----
Color baseColor(char b);
Color baseFill(char b, bool ghost = false);
Color propColor(const char* property);
Color propFill(const char* property);
void baseChip(Graphics& g, const RectF& r, char base, bool ghost, bool isNew,
              float fontPx, bool dashed = false);
void aaBead(Graphics& g, float cx, float cy, float d, char aa, float fontPx, bool faded = false);
void dnaIcon(Graphics& g, const RectF& box, Color strand, Color rung);
void playPauseIcon(Graphics& g, const RectF& c, bool playing, Color color);
void resetIcon(Graphics& g, const RectF& c, Color color);
void chevronRight(Graphics& g, float x, float y, float size, Color c);

// ---- 文本编码 ----
std::wstring utf8(const std::string& s);
std::string narrow(const std::wstring& s);

// 必须在 GdiplusShutdown 之前调用，释放字体缓存中的 GDI+ 对象
void clearFontCache();

} // namespace ui
