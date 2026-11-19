// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

#include "ui.h"
#include "genetics.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace ui {

Color BG(255, 243, 246, 252);
Color CARD(255, 255, 255, 255);
Color BORDER(255, 228, 234, 243);
Color BORDER_SOFT(255, 238, 242, 248);
Color TXT(255, 23, 35, 59);
Color SUB(255, 94, 108, 132);
Color MUTED(255, 147, 160, 180);
Color ACCENT(255, 47, 107, 240);
Color ACCENT_DARK(255, 31, 84, 204);
Color ACCENT_SOFT(255, 234, 241, 255);
Color GREEN(255, 18, 161, 106);
Color GREEN_SOFT(255, 232, 248, 241);
Color AMBER(255, 224, 154, 46);
Color AMBER_SOFT(255, 255, 246, 228);
Color RED(255, 222, 75, 85);
Color RED_SOFT(255, 253, 238, 240);
Color PURPLE(255, 122, 90, 248);
Color TEAL(255, 14, 165, 164);
Color HEADER_A(255, 10, 24, 48);
Color HEADER_B(255, 19, 44, 82);
Color NEW_STRAND(255, 16, 163, 107);
Color NEW_SOFT(255, 235, 250, 244);

Color mix(Color a, Color b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    auto lerp = [&](BYTE x, BYTE y) -> BYTE {
        return (BYTE)std::lround(x + (y - x) * t);
    };
    return Color(a.GetA(), lerp(a.GetR(), b.GetR()), lerp(a.GetG(), b.GetG()),
                 lerp(a.GetB(), b.GetB()));
}

Color alpha(Color c, int a) {
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return Color((BYTE)a, c.GetR(), c.GetG(), c.GetB());
}

std::wstring utf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

namespace {

struct FontCache {
    std::map<std::string, std::unique_ptr<FontFamily>> families;
    std::map<std::string, std::unique_ptr<Font>> fonts;

    FontFamily* family(const wchar_t* name) {
        std::string key = narrow(name);
        auto it = families.find(key);
        if (it != families.end()) return it->second.get();

        auto fam = std::make_unique<FontFamily>(name);
        if (fam->GetLastStatus() != Ok || !fam->IsAvailable()) {
            fam = std::make_unique<FontFamily>(L"Segoe UI");
            if (fam->GetLastStatus() != Ok || !fam->IsAvailable()) {
                fam = std::make_unique<FontFamily>(L"Arial");
            }
        }
        FontFamily* ptr = fam.get();
        families[key] = std::move(fam);
        return ptr;
    }

    Font* get(float px, int style, bool mono) {
        if (px < 5) px = 5;
        int ipx = (int)std::lround(px * 10.0f);
        char key[96];
        std::snprintf(key, sizeof(key), "%s|%d|%d", mono ? "M" : "UI", ipx, style);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second.get();

        FontFamily* fam = mono ? family(L"Cascadia Mono") : family(L"Microsoft YaHei UI");
        if (mono && (fam->GetLastStatus() != Ok)) fam = family(L"Consolas");
        int fs = FontStyleRegular;
        if (style == 1) fs = FontStyleBold;
        if (style == 2) fs = FontStyleItalic;
        auto f = std::make_unique<Font>(fam, px, fs, UnitPixel);
        if (f->GetLastStatus() != Ok) {
            FontFamily* fb = family(L"Segoe UI");
            f = std::make_unique<Font>(fb, px, fs, UnitPixel);
        }
        Font* ptr = f.get();
        fonts[key] = std::move(f);
        return ptr;
    }
};

FontCache& cache() {
    static FontCache c;
    return c;
}

} // namespace

void clearFontCache() {
    cache().fonts.clear();
    cache().families.clear();
}

Font* font(float px, bool bold, bool mono) { return cache().get(px, bold ? 1 : 0, mono); }
Font* fontSemi(float px, bool mono) { return cache().get(px, 1, mono); }

float textWidth(Graphics& g, const std::wstring& s, Font* f) {
    if (s.empty() || !f) return 0;
    RectF box;
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    g.MeasureString(s.c_str(), (INT)s.size(), f, PointF(0, 0), &fmt, &box);
    return box.Width;
}

RectF toRectF(const RECT& r) {
    return RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
}

RectF insetRect(const RectF& r, float d) {
    return RectF(r.X + d, r.Y + d, r.Width - 2 * d, r.Height - 2 * d);
}

void fillRound(Graphics& g, const RectF& r, float radius, Color c) {
    if (r.Width <= 0 || r.Height <= 0) return;
    float rad = radius;
    float lim = std::min(r.Width, r.Height) / 2.0f;
    if (rad > lim) rad = lim;
    if (rad <= 0.1f) {
        SolidBrush b(c);
        g.FillRectangle(&b, r);
        return;
    }
    GraphicsPath p;
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

void strokeRound(Graphics& g, const RectF& r, float radius, Color c, float width) {
    if (r.Width <= 0 || r.Height <= 0) return;
    float rad = radius;
    if (rad > std::min(r.Width, r.Height) / 2.0f) rad = std::min(r.Width, r.Height) / 2.0f;
    if (rad < 0.1f) {
        Pen pen(c, width);
        g.DrawRectangle(&pen, r);
        return;
    }
    GraphicsPath p;
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
    Pen pen(c, width);
    g.DrawPath(&pen, &p);
}

void shadow(Graphics& g, const RectF& r, float radius, int spread, int a) {
    if (r.Width <= 0 || r.Height <= 0) return;
    // 单层柔和投影：远比逐层放大的近似高斯便宜，视觉上仍保留轻微层次
    float d = (float)std::max(1, spread / 3);
    Color c(255, 15, 23, 42);
    c = alpha(c, std::max(10, a));
    fillRound(g, RectF(r.X, r.Y + d * 0.6f, r.Width, r.Height + d * 0.5f), radius, c);
}

void text(Graphics& g, const std::wstring& s, const RectF& r, Font* f, Color c,
          StringAlignment ha, StringAlignment va, bool ellipsis) {
    if (!f || s.empty()) return;
    StringFormat fmt;
    fmt.SetAlignment(ha);
    fmt.SetLineAlignment(va);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    if (ellipsis) {
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    }
    SolidBrush b(c);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.DrawString(s.c_str(), (INT)s.size(), f, r, &fmt, &b);
}

void textUtf8(Graphics& g, const std::string& s, const RectF& r, Font* f, Color c,
              StringAlignment ha, StringAlignment va, bool ellipsis) {
    text(g, utf8(s), r, f, c, ha, va, ellipsis);
}

void line(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x1, y1, x2, y2);
}

void dashLine(Graphics& g, float x1, float y1, float x2, float y2, Color c, float w,
              DashStyle style) {
    Pen p(c, w);
    p.SetDashStyle(style);
    g.DrawLine(&p, x1, y1, x2, y2);
}

void arrowDown(Graphics& g, float x, float y1, float y2, Color c, float w) {
    line(g, x, y1, x, y2 - w * 2, c, w);
    SolidBrush b(c);
    PointF pts[3] = {{x - w * 3.2f, y2 - w * 3.2f}, {x + w * 3.2f, y2 - w * 3.2f}, {x, y2}};
    g.FillPolygon(&b, pts, 3);
}

void dot(Graphics& g, float cx, float cy, float r, Color c) {
    SolidBrush b(c);
    g.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}

Color baseColor(char b) {
    switch (b) {
        case 'A': return Color(255, 24, 160, 88);
        case 'T': return Color(255, 221, 59, 75);
        case 'G': return Color(255, 46, 107, 230);
        case 'C': return Color(255, 208, 132, 10);
        case 'U': return Color(255, 124, 92, 252);
        case 'N': return Color(255, 122, 134, 153);
        default:  return Color(255, 138, 148, 166);
    }
}

Color baseFill(char b, bool ghost) {
    if (ghost) return Color(255, 249, 250, 252);
    return mix(Color(255, 255, 255, 255), baseColor(b), 0.10f);
}

Color propColor(const char* property) {
    if (!property) return Color(255, 138, 148, 166);
    std::string p = property;
    if (p == "nonpolar") return Color(255, 214, 142, 36);
    if (p == "polar")    return Color(255, 21, 156, 113);
    if (p == "acidic")   return Color(255, 213, 80, 82);
    if (p == "basic")    return Color(255, 60, 118, 232);
    if (p == "stop")     return Color(255, 108, 117, 132);
    return Color(255, 138, 148, 166);
}

Color propFill(const char* property) {
    return mix(Color(255, 255, 255, 255), propColor(property), 0.09f);
}

void baseChip(Graphics& g, const RectF& r, char base, bool ghost, bool isNew, float fontPx,
              bool dashed) {
    Color bc = baseColor(base);
    Color fill = ghost ? Color(255, 249, 250, 252) : baseFill(base);
    Color border = ghost ? Color(255, 226, 232, 240) : alpha(bc, 72);
    if (isNew) {
        fill = NEW_SOFT;
        border = alpha(NEW_STRAND, 165);
    }
    fillRound(g, r, std::min(r.Width, r.Height) * 0.22f, fill);
    Pen pen(border, 1.0f);
    if (dashed) pen.SetDashStyle(DashStyleDash);
    if (r.Width > 4 && r.Height > 4) {
        auto path = [&](GraphicsPath& p) {
            float rad = std::min(r.Width, r.Height) * 0.22f;
            float d = rad * 2;
            p.AddArc(r.X, r.Y, d, d, 180, 90);
            p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
            p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
            p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
            p.CloseFigure();
        };
        GraphicsPath p;
        path(p);
        g.DrawPath(&pen, &p);
    }
    Color tc = ghost ? Color(255, 174, 184, 199) : bc;
    Font* f = font(fontPx, true, true);
    text(g, std::wstring(1, (wchar_t)base), r, f, tc, StringAlignmentCenter,
         StringAlignmentCenter, false);
}

void aaBead(Graphics& g, float cx, float cy, float d, char aa, float fontPx, bool faded) {
    const auto& info = dna::aminoAcid(aa);
    Color c = propColor(info.property);
    if (faded) c = alpha(c, 90);
    SolidBrush b(c);
    g.FillEllipse(&b, cx - d / 2, cy - d / 2, d, d);
    Pen p(alpha(Color(255, 255, 255, 255), faded ? 80 : 120), 1.2f);
    g.DrawEllipse(&p, cx - d / 2, cy - d / 2, d, d);
    Font* f = font(fontPx, true, false);
    std::wstring s(1, (wchar_t)(aa == '*' ? '#' : aa));
    text(g, s, RectF(cx - d / 2, cy - d / 2, d, d), f,
         alpha(Color(255, 255, 255, 255), faded ? 160 : 255), StringAlignmentCenter,
         StringAlignmentCenter, false);
}

void dnaIcon(Graphics& g, const RectF& box, Color strand, Color rung) {
    fillRound(g, box, box.Width * 0.28f, alpha(Color(255, 255, 255, 255), 16));
    strokeRound(g, box, box.Width * 0.28f, alpha(Color(255, 255, 255, 255), 26), 1.0f);
    float padX = box.Width * 0.26f;
    float amp = box.Height * 0.20f;
    float cy = box.Y + box.Height / 2;
    GraphicsPath a, b;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps;
        float x = box.X + padX + t * (box.Width - 2 * padX);
        float y = cy + std::sin(t * 6.28318f) * amp;
        if (i == 0) a.StartFigure();
        a.AddLine(x, y, x, y);
        float y2 = cy - std::sin(t * 6.28318f) * amp;
        if (i == 0) b.StartFigure();
        b.AddLine(x, y2, x, y2);
    }
    Pen p1(strand, 2.1f);
    p1.SetStartCap(LineCapRound);
    p1.SetEndCap(LineCapRound);
    Pen p2(rung, 1.35f);
    g.DrawPath(&p1, &a);
    g.DrawPath(&p1, &b);
    for (int k = 1; k < 6; ++k) {
        float t = (float)k / 6.0f;
        float x = box.X + padX + t * (box.Width - 2 * padX);
        float dy = std::sin(t * 6.28318f) * amp;
        g.DrawLine(&p2, x, cy + dy, x, cy - dy);
    }
}

void playPauseIcon(Graphics& g, const RectF& c, bool playing, Color color) {
    SolidBrush b(color);
    float cx = c.X + c.Width / 2, cy = c.Y + c.Height / 2;
    if (playing) {
        float w = c.Width * 0.11f, h = c.Height * 0.42f;
        fillRound(g, RectF(cx - w * 2.2f, cy - h / 2, w, h), w * 0.35f, color);
        fillRound(g, RectF(cx + w * 1.2f, cy - h / 2, w, h), w * 0.35f, color);
    } else {
        float h = c.Height * 0.44f, w = c.Width * 0.36f;
        PointF pts[3] = {{cx - w * 0.45f, cy - h / 2}, {cx - w * 0.45f, cy + h / 2},
                         {cx + w * 0.62f, cy}};
        g.FillPolygon(&b, pts, 3);
    }
}

void resetIcon(Graphics& g, const RectF& c, Color color) {
    Pen p(color, 1.8f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    float r = c.Width * 0.30f;
    float cx = c.X + c.Width / 2, cy = c.Y + c.Height / 2;
    g.DrawArc(&p, cx - r, cy - r, 2 * r, 2 * r, 40, 280);
    float ang = 40.0f * 3.14159f / 180.0f;
    float ax = cx + std::cos(ang) * r, ay = cy + std::sin(ang) * r;
    SolidBrush b(color);
    PointF tri[3] = {{ax + 2.5f, ay - 5.0f}, {ax - 4.0f, ay - 3.4f}, {ax + 1.0f, ay + 3.0f}};
    g.FillPolygon(&b, tri, 3);
}

void chevronRight(Graphics& g, float x, float y, float size, Color c) {
    Pen p(c, 1.7f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x, y - size / 2, x + size / 2, y);
    g.DrawLine(&p, x + size / 2, y, x, y + size / 2);
}

} // namespace ui
