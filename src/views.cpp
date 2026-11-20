// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// views.cpp — 复制 / 转录 / 翻译 / 密码子表 四个可视化视图
#include "app.h"
#include "ui.h"
#include "genetics.h"

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

using namespace ui;

// ------------------------------------------------------------------ 通用
static RectF innerRect(App& a) { return insetRect(toRectF(a.rcRight), 22.f * a.s); }

static std::wstring rnaOf(const std::string& dna) {
    std::wstring w;
    w.reserve(dna.size());
    for (char c : dna) w.push_back((wchar_t)(c == 'T' ? 'U' : c));
    return w;
}

static float smooth01(float t) {
    t = std::max(0.f, std::min(1.f, t));
    return t * t * (3.f - 2.f * t);
}

static std::wstring anticodonOf(const std::string& dna) {
    return rnaOf(dna::complement(dna));
}

static void viewHeader(App& a, Graphics& g, const wchar_t* title, const wchar_t* pill,
                       const std::wstring& rightText) {
    RectF inner = innerRect(a);
    float s = a.s;
    Font* tf = font(18.f * s, true);
    text(g, title, RectF(inner.X, inner.Y - 2.f * s, inner.Width * 0.55f, 28.f * s), tf, TXT,
         StringAlignmentNear, StringAlignmentCenter, false);
    float tw = textWidth(g, title, tf);
    Font* pf = font(9.f * s, true);
    RectF pr(inner.X + tw + 10.f * s, inner.Y + 4.f * s, 0, 20.f * s);
    pr.Width = textWidth(g, pill, pf) + 18.f * s;
    fillRound(g, pr, 10.f * s, ACCENT_SOFT);
    text(g, pill, pr, pf, ACCENT, StringAlignmentCenter, StringAlignmentCenter, false);
    if (!rightText.empty()) {
        text(g, rightText, RectF(inner.X, inner.Y, inner.Width, 26.f * s),
             font(9.4f * s), MUTED, StringAlignmentFar, StringAlignmentCenter, true);
    }
}

static void viewSubtitle(App& a, Graphics& g, const std::wstring& s) {
    RectF inner = innerRect(a);
    text(g, s, RectF(inner.X, inner.Y + 30.f * a.s, inner.Width, 18.f * a.s),
         font(10.4f * a.s), SUB, StringAlignmentNear, StringAlignmentCenter, true);
}

static std::wstring progressText(App& a, int tab) {
    if (!a.hasSeq) return L"";
    if (tab == 2) {
        int totalCodons = (int)a.tr.codons.size() + (a.tr.hasStop ? 1 : 0);
        float pos = a.progress[2] * (float)std::max(1, totalCodons);
        int read = std::min(totalCodons, (int)std::floor(pos));
        float frac = pos - (float)std::floor(pos);
        if (read >= totalCodons) frac = 0.f;
        int beads = std::min<int>(
            a.tr.protein.size(),
            read + ((read < (int)a.tr.protein.size() && frac >= 0.35f) ? 1 : 0));
        return L"肽链 " + std::to_wstring(beads) + L" / " +
               std::to_wstring(a.tr.protein.size()) + L" aa";
    }
    int n = (int)a.coding.size();
    int pos = (int)std::lround(a.progress[tab] * (float)n);
    return (tab == 0 ? L"复制叉 " : L"已转录 ") + std::to_wstring(pos) + L" / " +
           std::to_wstring(n) + (tab == 0 ? L" bp" : L" nt");
}

static void drawPlayback(App& a, Graphics& g) {
    if (!a.sliderVisible) return;
    float s = a.s;
    RectF inner = innerRect(a);
    RectF track = toRectF(a.sliderRc);
    float cy = track.Y + track.Height / 2.f;
    float left = track.X + 6.f * s;
    float right = track.X + track.Width - 6.f * s;
    float t = (a.tab >= 0 && a.tab < 3) ? a.progress[a.tab] : 0.f;
    bool enabled = a.hasSeq;

    RectF lineR(inner.X, (float)a.playRc.top - 14.f * s, inner.Width, 1.f);
    SolidBrush lb(Color(255, 236, 241, 248));
    g.FillRectangle(&lb, lineR);

    RectF bg(left, cy - 3.f * s, right - left, 6.f * s);
    fillRound(g, bg, 3.f * s, Color(255, 230, 236, 245));
    if (enabled && t > 0)
        fillRound(g, RectF(left, cy - 3.f * s, (right - left) * t, 6.f * s), 3.f * s, ACCENT);
    float thumbX = left + (right - left) * t;
    float thumbR = (a.hoveringSlider || a.draggingSlider ? 8.f : 7.f) * s;
    dot(g, thumbX, cy, thumbR + 1.5f * s, Color(255, 255, 255, 255));
    dot(g, thumbX, cy, thumbR, enabled ? ACCENT : Color(255, 199, 208, 222));

    wchar_t buf[16];
    swprintf(buf, 16, L"%d%%", (int)std::lround(t * 100.f));
    RectF pr((float)a.sliderRc.right + 12.f * s, cy - 10.f * s,
             (inner.X + inner.Width) - (float)a.sliderRc.right - 12.f * s, 20.f * s);
    text(g, buf, pr, font(9.2f * s, false, true), MUTED, StringAlignmentFar,
         StringAlignmentCenter, false);
}

static void drawEmptyState(App& a, Graphics& g, const RectF& area, const wchar_t* title,
                           const std::wstring& sub) {
    float s = a.s;
    float cy = area.Y + area.Height * 0.42f;
    RectF icon(area.X + area.Width / 2 - 31.f * s, cy - 74.f * s, 62.f * s, 62.f * s);
    fillRound(g, icon, 18.f * s, ACCENT_SOFT);
    dnaIcon(g, RectF(icon.X + 9.f * s, icon.Y + 9.f * s, icon.Width - 18.f * s,
                     icon.Height - 18.f * s),
            alpha(ACCENT, 190), alpha(ACCENT, 90));
    text(g, title, RectF(area.X, cy + 4.f * s, area.Width, 26.f * s), font(14.f * s, true),
         Color(255, 94, 108, 132), StringAlignmentCenter, StringAlignmentCenter, true);
    text(g, sub, RectF(area.X + 40.f * s, cy + 34.f * s, area.Width - 80.f * s, 38.f * s),
         font(10.2f * s), MUTED, StringAlignmentCenter, StringAlignmentNear, true);
}

static void endText(Graphics& g, App& a, const wchar_t* s, float x, float y, bool alignRight) {
    RectF r = alignRight ? RectF(x - 26.f * a.s, y, 26.f * a.s, 16.f * a.s)
                         : RectF(x + 4.f * a.s, y, 26.f * a.s, 16.f * a.s);
    text(g, s, r, font(8.4f * a.s, false, true), MUTED,
         alignRight ? StringAlignmentFar : StringAlignmentNear, StringAlignmentCenter, false);
}

static void pairDots(Graphics& g, App& a, float x, float chipW, float gap, int count,
                     float y1, float y2, int skipFrom = -1, int skipTo = -1,
                     int faintBefore = -1) {
    float s = a.s;
    for (int i = 0; i < count; ++i) {
        if (skipFrom >= 0 && i >= skipFrom && i < skipTo) continue;
        float cx = x + i * (chipW + gap) + chipW / 2.f;
        int al = (faintBefore >= 0 && i < faintBefore) ? 35 : 85;
        dot(g, cx, y1 + (y2 - y1) * 0.5f, 1.0f * s, Color((BYTE)al, 120, 134, 153));
    }
}

struct Win {
    int start = 0;
    int count = 0;
};

static Win seqWindow(int n, float p, int cap) {
    Win w;
    if (n <= 0) return w;
    cap = std::min(cap, n);
    if (cap <= 0) return w;
    int focus = (int)std::lround(std::min(1.f, std::max(0.f, p)) * (float)n);
    w.start = std::max(0, std::min(n - cap, focus - cap / 2));
    w.count = cap;
    return w;
}

static void infoNote(Graphics& g, App& a, const RectF& r, const std::wstring& text0,
                     const std::wstring& text1, Color accent) {
    float s = a.s;
    fillRound(g, r, 9.f * s, Color(255, 249, 251, 254));
    strokeRound(g, r, 9.f * s, Color(255, 235, 240, 248), 1.f);
    RectF bar(r.X + 1.f * s, r.Y + 6.f * s, 3.f * s, r.Height - 12.f * s);
    fillRound(g, bar, 1.5f * s, alpha(accent, 170));
    Font* f = font(9.4f * s);
    float tx = r.X + 14.f * s;
    if (!text0.empty())
        text(g, text0, RectF(tx, r.Y + 4.f * s, r.Width - 24.f * s, 18.f * s), f, SUB,
             StringAlignmentNear, StringAlignmentCenter, true);
    if (!text1.empty())
        text(g, text1, RectF(tx, r.Y + 22.f * s, r.Width - 24.f * s, 18.f * s), f, SUB,
             StringAlignmentNear, StringAlignmentCenter, true);
}

// ================================================================== 复制
static void drawReplication(App& a, Graphics& g) {
    float s = a.s;
    RectF inner = innerRect(a);
    viewHeader(a, g, L"DNA 复制", L"半保留复制", progressText(a, 0));
    viewSubtitle(a, g, L"解旋酶打开双链，DNA 聚合酶分别以两条母链为模板，合成互补的新链。");
    drawPlayback(a, g);

    RectF fig(inner.X, inner.Y + 58.f * s, inner.Width,
              (float)a.playRc.top - 18.f * s - (inner.Y + 58.f * s));
    if (!a.hasSeq) {
        drawEmptyState(a, g, fig, L"输入 DNA 序列以模拟复制",
                       L"支持 A/T/G/C/U/N，可直接粘贴或导入 FASTA / TXT。");
        return;
    }

    const int n = (int)a.coding.size();
    const float p = a.progress[0];
    const int focus = std::max(0, std::min(n, (int)std::lround(p * (float)n)));
    Win w = seqWindow(n, p, 20);

    float gap = 2.4f * s;
    float side = 36.f * s;
    float chipW = std::min(24.f * s,
                           (fig.Width - 2 * side - (w.count - 1) * gap) / (float)w.count);
    if (chipW < 8.f * s) chipW = 8.f * s;
    float totalRowW = w.count * chipW + (w.count - 1) * gap;
    float x0 = fig.X + (fig.Width - totalRowW) / 2.f;
    float rowH = 24.f * s;

    float blockH = 18.f * s + rowH + 13.f * s + rowH + 38.f * s + 20.f * s + 112.f * s +
                   16.f * s + 38.f * s;
    float y = fig.Y + std::max(0.f, (fig.Height - blockH) * 0.5f);

    text(g, L"亲代 DNA · 双链", RectF(x0, y, 300.f * s, 16.f * s),
         font(10.6f * s, true), Color(255, 68, 84, 108), StringAlignmentNear,
         StringAlignmentCenter, false);
    float yTop = y + 20.f * s;
    float yBot = yTop + rowH + 14.f * s;

    for (int i = 0; i < w.count; ++i) {
        int idx = w.start + i;
        float x = x0 + i * (chipW + gap);
        bool opened = idx < focus;
        baseChip(g, RectF(x, yTop, chipW, rowH), a.coding[idx], opened, false, 10.f * s, false);
        baseChip(g, RectF(x, yBot, chipW, rowH), a.templ[idx], opened, false, 10.f * s, false);
    }
    pairDots(g, a, x0, chipW, gap, w.count, yTop, yBot, -1, -1, focus - w.start);
    endText(g, a, w.start == 0 ? L"5'" : L"…", x0, yTop, true);
    endText(g, a, (w.start + w.count == n) ? L"3'" : L"…", x0 + w.count * (chipW + gap), yTop,
            false);
    endText(g, a, w.start == 0 ? L"3'" : L"…", x0, yBot, true);
    endText(g, a, (w.start + w.count == n) ? L"5'" : L"…", x0 + w.count * (chipW + gap), yBot,
            false);
    text(g, L"编码链", RectF(x0, yTop - 15.f * s, 70.f * s, 13.f * s), font(8.f * s), MUTED,
         StringAlignmentNear, StringAlignmentCenter, false);
    text(g, L"模板链", RectF(x0, yBot + rowH + 1.f * s, 70.f * s, 13.f * s), font(8.f * s),
         MUTED, StringAlignmentNear, StringAlignmentCenter, false);

    if (focus >= w.start && focus < w.start + w.count) {
        float xf = x0 + (focus - w.start) * (chipW + gap) - gap * 0.5f;
        dashLine(g, xf, yTop - 7.f * s, xf, yBot + rowH + 7.f * s,
                 Color(220, 224, 154, 46), 1.6f * s);
        Font* lf = font(8.2f * s, true);
        std::wstring lab = L"解旋酶";
        float tw = textWidth(g, lab, lf) + 14.f * s;
        RectF pill(xf - tw / 2.f, yTop - 24.f * s, tw, 16.f * s);
        fillRound(g, pill, 8.f * s, AMBER_SOFT);
        text(g, lab, pill, lf, AMBER, StringAlignmentCenter, StringAlignmentCenter, false);
    }

    float detach = p;
    detach = detach * detach * (3.f - 2.f * detach);

    float arrowY = yBot + rowH + 12.f * s;
    float dauY = arrowY + 36.f * s + 3.f * s * detach;
    float panelGap = 18.f * s;
    float panelW = (fig.Width - panelGap) / 2.f;
    float drift = 9.f * s * detach;
    float panelX[2] = {fig.X - drift, fig.X + panelW + panelGap + drift};

    float forkX = fig.X + fig.Width / 2.f;
    if (focus >= w.start && focus < w.start + w.count)
        forkX = x0 + (focus - w.start) * (chipW + gap) + chipW / 2.f;
    float branchY = arrowY + 16.f * s;
    int branchAlpha = 55 + (int)(150.f * detach);

    dashLine(g, forkX, arrowY, forkX, branchY, alpha(ACCENT, branchAlpha), 1.5f * s,
             DashStyleDash);
    SolidBrush stemBrush(alpha(ACCENT, branchAlpha));
    PointF stemTri[3] = {{forkX - 4.f * s, branchY - 6.f * s},
                         {forkX + 4.f * s, branchY - 6.f * s},
                         {forkX, branchY + 3.f * s}};
    g.FillPolygon(&stemBrush, stemTri, 3);

    for (int d = 0; d < 2; ++d) {
        float tx = panelX[d] + panelW / 2.f;
        float ty = dauY - 7.f * s;
        PointF p0(forkX, branchY);
        PointF p1(forkX, branchY + 16.f * s);
        PointF p2(tx, ty - 28.f * s);
        PointF p3(tx, ty);
        GraphicsPath path;
        path.AddBezier(p0, p1, p2, p3);
        Color c = (d == 0) ? ACCENT : NEW_STRAND;
        Pen pen(alpha(c, branchAlpha), 1.5f * s);
        pen.SetDashStyle(DashStyleDash);
        g.DrawPath(&pen, &path);
        SolidBrush ab(alpha(c, branchAlpha));
        PointF arrow[3] = {{tx - 5.f * s, ty - 7.f * s},
                           {tx + 5.f * s, ty - 7.f * s}, {tx, ty}};
        g.FillPolygon(&ab, arrow, 3);

        if (detach > 0.03f) {
            for (int kk = 0; kk < 2; ++kk) {
                float tt = std::fmod(detach * 1.6f + kk * 0.5f, 1.f);
                float u = 1.f - tt;
                float qx = u*u*u*p0.X + 3*u*u*tt*p1.X + 3*u*tt*tt*p2.X + tt*tt*tt*p3.X;
                float qy = u*u*u*p0.Y + 3*u*u*tt*p1.Y + 3*u*tt*tt*p2.Y + tt*tt*tt*p3.Y;
                Color pc = (kk == 0) ? NEW_STRAND : Color(255, 126, 146, 178);
                dot(g, qx, qy, (2.2f + 1.0f * tt) * s,
                    alpha(pc, 180 - (int)(90 * tt)));
            }
        }
    }
    text(g, L"子链分离", RectF(forkX + 10.f * s, branchY - 11.f * s, 80.f * s, 16.f * s),
         font(8.2f * s), alpha(NEW_STRAND, 170), StringAlignmentNear,
         StringAlignmentCenter, false);
    text(g, L"半保留复制", RectF(forkX - 90.f * s, branchY - 11.f * s, 80.f * s, 16.f * s),
         font(8.2f * s), MUTED, StringAlignmentFar, StringAlignmentCenter, false);

    for (int d = 0; d < 2; ++d) {
        float px = panelX[d];
        RectF panel(px, dauY, panelW, 112.f * s);
        fillRound(g, panel, 10.f * s, Color(255, 250, 251, 254));
        strokeRound(g, panel, 10.f * s, Color(255, 235, 240, 248), 1.f);
        text(g, d == 0 ? L"子代 DNA ①" : L"子代 DNA ②",
             RectF(panel.X + 12.f * s, panel.Y + 7.f * s, panel.Width * 0.6f, 15.f * s),
             font(9.6f * s, true), Color(255, 72, 88, 112), StringAlignmentNear,
             StringAlignmentCenter, false);
        text(g, L"旧链 + 新链",
             RectF(panel.X, panel.Y + 7.f * s, panel.Width - 12.f * s, 15.f * s),
             font(8.2f * s), MUTED, StringAlignmentFar, StringAlignmentCenter, false);

        float pad = 12.f * s;
        float relW = panel.Width - 2.f * pad - 26.f * s;
        float dGap = 1.6f * s;
        float dChip = std::min(17.f * s, relW / (float)w.count - dGap);
        if (dChip < 6.f * s) dChip = 6.f * s;
        float dx = panel.X + pad + 22.f * s;
        float dy1 = panel.Y + 36.f * s;
        float dy2 = dy1 + 28.f * s;

        for (int i = 0; i < w.count; ++i) {
            int idx = w.start + i;
            float x = dx + i * (dChip + dGap);
            bool gone = idx >= focus;
            if (d == 0) {
                baseChip(g, RectF(x, dy1, dChip, 20.f * s), a.coding[idx], gone, false,
                         7.8f * s, gone);
                baseChip(g, RectF(x, dy2, dChip, 20.f * s), a.templ[idx], gone, !gone,
                         7.8f * s, gone);
            } else {
                baseChip(g, RectF(x, dy1, dChip, 20.f * s), a.coding[idx], gone, !gone,
                         7.8f * s, gone);
                baseChip(g, RectF(x, dy2, dChip, 20.f * s), a.templ[idx], gone, false,
                         7.8f * s, gone);
            }
        }
        pairDots(g, a, dx, dChip, dGap, w.count, dy1, dy2, -1, -1, -1);
    }

    float legendY = dauY + 122.f * s;
    auto legendItem = [&](float x, const wchar_t* label, Color fill, Color border, bool dashed) {
        RectF sw(x, legendY + 3.f * s, 13.f * s, 11.f * s);
        fillRound(g, sw, 3.f * s, fill);
        Pen pen(border, 1.f);
        if (dashed) pen.SetDashStyle(DashStyleDash);
        g.DrawRectangle(&pen, sw.X, sw.Y, sw.Width, sw.Height);
        text(g, label, RectF(x + 19.f * s, legendY, 100.f * s, 17.f * s), font(8.4f * s), MUTED,
             StringAlignmentNear, StringAlignmentCenter, false);
    };
    legendItem(fig.X, L"亲代旧链", Color(255, 252, 253, 255), Color(255, 217, 225, 238), false);
    legendItem(fig.X + 112.f * s, L"新合成链", NEW_SOFT, alpha(NEW_STRAND, 160), false);
    legendItem(fig.X + 236.f * s, L"未复制（虚线）", Color(255, 250, 251, 253),
               Color(255, 214, 222, 234), true);

    RectF note(fig.X, legendY + 24.f * s, fig.Width, 32.f * s);
    if (note.GetBottom() <= fig.GetBottom()) {
        infoNote(g, a, note,
                 L"半保留复制：每个子代 DNA 都保留一条亲代旧链，并合成一条互补新链。",
                 L"碱基配对规则：A–T，C–G；两条新链的合成方向相反（5'→3'）。", GREEN);
    }
}

// ================================================================== 转录
static void drawTranscription(App& a, Graphics& g) {
    float s = a.s;
    RectF inner = innerRect(a);
    viewHeader(a, g, L"转录", L"DNA → mRNA", progressText(a, 1));
    viewSubtitle(a, g, L"RNA 聚合酶以模板链为模板，按 A–U、C–G 配对规则合成 mRNA。");
    drawPlayback(a, g);

    RectF fig(inner.X, inner.Y + 58.f * s, inner.Width,
              (float)a.playRc.top - 18.f * s - (inner.Y + 58.f * s));
    if (!a.hasSeq) {
        drawEmptyState(a, g, fig, L"输入 DNA 序列以模拟转录",
                       L"mRNA 与编码链序列相同（T→U），并与模板链互补。");
        return;
    }

    const int n = (int)a.coding.size();
    const float p = a.progress[1];
    const int focus = std::max(0, std::min(n, (int)std::lround(p * (float)n)));
    Win w = seqWindow(n, p, 24);

    float gap = 2.6f * s;
    float side = 36.f * s;
    float chipW = std::min(24.f * s,
                           (fig.Width - 2 * side - (w.count - 1) * gap) / (float)w.count);
    if (chipW < 8.f * s) chipW = 8.f * s;
    float totalRowW = w.count * chipW + (w.count - 1) * gap;
    float x0 = fig.X + (fig.Width - totalRowW) / 2.f;
    float rowH = 26.f * s;

    float blockH = 18.f * s + rowH + 16.f * s + rowH + 34.f * s + 22.f * s + rowH +
                   24.f * s + 40.f * s + 10.f * s + 68.f * s;
    float y = fig.Y + std::max(0.f, (fig.Height - blockH) * 0.45f);

    text(g, L"DNA 双链", RectF(x0, y, 200.f * s, 16.f * s), font(10.6f * s, true),
         Color(255, 68, 84, 108), StringAlignmentNear, StringAlignmentCenter, false);
    text(g, L"编码链 5'→3'  ·  模板链 3'→5'",
         RectF(fig.X, y, fig.Width - 36.f * s, 16.f * s), font(8.6f * s), MUTED,
         StringAlignmentFar, StringAlignmentCenter, true);

    float yTop = y + 20.f * s;
    float yBot = yTop + rowH + 16.f * s;

    if (focus >= w.start && focus <= w.start + w.count) {
        float bx = x0 + (focus - w.start) * (chipW + gap) - 4 * (chipW + gap);
        float bw = 9 * (chipW + gap);
        float lo = x0 - 4.f * s;
        float hi = x0 + w.count * (chipW + gap);
        if (bx < lo) bx = lo;
        if (bx + bw > hi) bx = hi - bw;
        RectF bubble(bx, yTop - 8.f * s, bw, (yBot + rowH) - yTop + 16.f * s);
        fillRound(g, bubble, 14.f * s, Color(30, 47, 107, 240));
        strokeRound(g, bubble, 14.f * s, Color(60, 47, 107, 240), 1.f);
        Font* lf = font(8.2f * s, true);
        std::wstring lab = L"RNA 聚合酶";
        float tw = textWidth(g, lab, lf) + 16.f * s;
        float px = x0 + (focus - w.start) * (chipW + gap) + chipW / 2.f;
        RectF pill(px - tw / 2.f, yTop - 28.f * s, tw, 18.f * s);
        fillRound(g, pill, 9.f * s, Color(255, 255, 255, 255));
        strokeRound(g, pill, 9.f * s, alpha(ACCENT, 60), 1.f);
        text(g, lab, pill, lf, ACCENT, StringAlignmentCenter, StringAlignmentCenter, false);
        SolidBrush tri(Color(255, 47, 107, 240));
        PointF pts[3] = {{px - 4.f * s, pill.Y + pill.Height - 2.f * s},
                         {px + 4.f * s, pill.Y + pill.Height - 2.f * s},
                         {px, pill.Y + pill.Height + 4.f * s}};
        g.FillPolygon(&tri, pts, 3);
    }

    for (int i = 0; i < w.count; ++i) {
        int idx = w.start + i;
        float x = x0 + i * (chipW + gap);
        baseChip(g, RectF(x, yTop, chipW, rowH), a.coding[idx], false, false, 10.5f * s, false);
        baseChip(g, RectF(x, yBot, chipW, rowH), a.templ[idx], false, false, 10.5f * s, false);
    }
    int skipA = std::max(0, focus - 4 - w.start);
    int skipB = std::max(0, focus + 5 - w.start);
    pairDots(g, a, x0, chipW, gap, w.count, yTop, yBot, skipA, skipB, -1);

    endText(g, a, w.start == 0 ? L"5'" : L"…", x0, yTop, true);
    endText(g, a, (w.start + w.count == n) ? L"3'" : L"…", x0 + w.count * (chipW + gap), yTop,
            false);
    endText(g, a, w.start == 0 ? L"3'" : L"…", x0, yBot, true);
    endText(g, a, (w.start + w.count == n) ? L"5'" : L"…", x0 + w.count * (chipW + gap), yBot,
            false);
    // mRNA 的 5' 端随聚合酶推进逐渐离开模板，形成下垂的新生链
    std::vector<float> ys(w.count);
    float baseGap = 10.f * s;
    float droopMax = 44.f * s;
    float lowBottom = yBot + rowH + baseGap + rowH;
    for (int i = 0; i < w.count; ++i) {
        int idx = w.start + i;
        bool made = idx < focus;
        float droop = baseGap;
        if (made) {
            float age = (float)(focus - idx);
            droop = std::min(droopMax, baseGap + age * 1.6f * s);
        }
        ys[i] = yBot + rowH + droop;
        lowBottom = std::max(lowBottom, ys[i] + rowH);
    }

    {
        std::vector<PointF> pts;
        for (int i = 0; i < w.count; ++i) {
            if (w.start + i > focus) continue;
            float cx = x0 + i * (chipW + gap) + chipW / 2.f;
            float cy = ys[i] + rowH / 2.f;
            pts.push_back(PointF(cx, cy));
        }
        if (pts.size() >= 2) {
            GraphicsPath back;
            back.AddCurve(pts.data(), (INT)pts.size());
            Pen bp(alpha(PURPLE, 100), 2.4f * s);
            bp.SetStartCap(LineCapRound);
            bp.SetEndCap(LineCapRound);
            g.DrawPath(&bp, &back);
        } else if (pts.size() == 1) {
            dot(g, pts[0].X, pts[0].Y, 2.2f * s, alpha(PURPLE, 120));
        }
    }

    for (int i = 0; i < w.count; ++i) {
        int idx = w.start + i;
        float x = x0 + i * (chipW + gap);
        bool made = idx < focus;
        baseChip(g, RectF(x, ys[i], chipW, rowH), a.mrna[idx], !made, false, 10.5f * s,
                 !made);
    }
    endText(g, a, w.start == 0 ? L"5'" : L"…", x0, ys[0], true);
    endText(g, a, (w.start + w.count == n) ? L"3'" : L"…",
            x0 + w.count * (chipW + gap), ys[w.count - 1], false);

    float dirY = lowBottom + 14.f * s;
    line(g, x0, dirY, x0 + 52.f * s, dirY, alpha(ACCENT, 140), 1.4f * s);
    SolidBrush tri(Color(255, 47, 107, 240));
    PointF pts[3] = {{x0 + 52.f * s, dirY - 4.f * s}, {x0 + 52.f * s, dirY + 4.f * s},
                     {x0 + 59.f * s, dirY}};
    g.FillPolygon(&tri, pts, 3);
    text(g, L"转录方向", RectF(x0 + 66.f * s, dirY - 8.f * s, 80.f * s, 16.f * s),
         font(8.2f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, false);

    if (p >= 0.999f) {
        Font* bf = font(8.6f * s, true);
        std::wstring done = L"转录完成 · mRNA 释放";
        float tw = textWidth(g, done, bf) + 20.f * s;
        RectF pill(fig.X + fig.Width - tw, dirY - 10.f * s, tw, 20.f * s);
        fillRound(g, pill, 10.f * s, GREEN_SOFT);
        text(g, done, pill, bf, GREEN, StringAlignmentCenter, StringAlignmentCenter, false);
    }

    RectF note(fig.X, dirY + 18.f * s, fig.Width, 50.f * s);
    if (note.GetBottom() <= fig.GetBottom()) {
        infoNote(g, a, note,
                 L"规则：mRNA 与模板链互补配对（A–U、T–A、C–G、G–C）。",
                 L"随着链延伸，5' 端逐渐离开模板；mRNA 与编码链相同，只是 T 换成了 U。",
                 PURPLE);
    }
}

// ================================================================== 翻译
static void drawProteinPanel(App& a, Graphics& g, RectF panel, int beads, bool flying,
                             float& outLastCx, float& outLastCy, float& outLastD) {
    float s = a.s;
    outLastCx = 0.f;
    outLastCy = 0.f;
    outLastD = 0.f;
    fillRound(g, panel, 11.f * s, Color(255, 249, 251, 254));
    strokeRound(g, panel, 11.f * s, Color(255, 235, 240, 248), 1.f);

    text(g, L"多肽链 · 蛋白质一级结构",
         RectF(panel.X + 14.f * s, panel.Y + 8.f * s, panel.Width * 0.65f, 18.f * s),
         font(10.8f * s, true), Color(255, 66, 82, 106), StringAlignmentNear,
         StringAlignmentCenter, false);
    std::wstring count = L"已合成 " + std::to_wstring(beads) + L" / " +
                         std::to_wstring(a.tr.protein.size()) + L" aa";
    text(g, count, RectF(panel.X, panel.Y + 8.f * s, panel.Width - 14.f * s, 18.f * s),
         font(8.8f * s), MUTED, StringAlignmentFar, StringAlignmentCenter, false);

    if (a.progress[2] >= 0.999f && a.tr.hasStop && !a.tr.protein.empty()) {
        Font* df = font(8.2f * s, true);
        std::wstring done = L"翻译完成 · 肽链释放";
        float tw = textWidth(g, done, df) + 18.f * s;
        RectF dp(panel.X + panel.Width - tw - 12.f * s, panel.Y + 26.f * s, tw, 16.f * s);
        fillRound(g, dp, 8.f * s, GREEN_SOFT);
        text(g, done, dp, df, GREEN, StringAlignmentCenter, StringAlignmentCenter, false);
    }

    float legendH = panel.Height > 150.f * s ? 18.f * s : 0.f;
    RectF area(panel.X + 14.f * s, panel.Y + 34.f * s, panel.Width - 28.f * s,
               panel.Height - 42.f * s - legendH);
    if (beads == 0) {
        dashLine(g, area.X + 20.f * s, area.Y + area.Height / 2.f,
                 area.X + area.Width - 20.f * s, area.Y + area.Height / 2.f,
                 Color(255, 204, 213, 226), 1.2f * s);
        text(g, L"核糖体尚未添加氨基酸…", area, font(9.6f * s), MUTED,
             StringAlignmentCenter, StringAlignmentCenter, false);
    } else {
        float d = std::min(30.f * s, area.Height * 0.5f);
        if (d < 20.f * s) d = 20.f * s;
        float bgap = 4.f * s;
        int perRow = std::max(1, (int)((area.Width + bgap) / (d + bgap)));
        int rowsMax = std::max(1, (int)((area.Height + bgap) / (d + bgap)));
        rowsMax = std::min(rowsMax, 3);
        int maxShown = perRow * rowsMax;
        int start = std::max(0, beads - maxShown);
        int shown = beads - start;
        int rows = (shown + perRow - 1) / perRow;
        float totalH = rows * d + (rows - 1) * bgap;
        float top = area.Y + (area.Height - totalH) / 2.f;
        if (start > 0) {
            text(g, L"…", RectF(panel.X + 4.f * s, area.Y + area.Height / 2.f - 8.f * s,
                                16.f * s, 16.f * s),
                 font(11.f * s, true), MUTED, StringAlignmentCenter, StringAlignmentCenter,
                 false);
        }
        float firstCx = 0, firstCy = 0, lastCx = 0, lastCy = 0;
        for (int k = 0; k < shown; ++k) {
            int row = k / perRow;
            int col = k % perRow;
            int inRow = std::min(perRow, shown - row * perRow);
            float rowW = inRow * d + (inRow - 1) * bgap;
            float rx = area.X + (area.Width - rowW) / 2.f + col * (d + bgap);
            float cy = top + row * (d + bgap) + d / 2.f;
            if (col > 0) line(g, rx - bgap, cy, rx, cy, Color(255, 205, 215, 229), 2.f * s);
            int pi = start + k;
            if (pi < (int)a.tr.protein.size() && !(flying && pi == beads - 1))
                aaBead(g, rx + d / 2.f, cy, d, a.tr.protein[pi], 12.5f * s);
            if (k == 0) { firstCx = rx + d / 2.f; firstCy = cy; }
            lastCx = rx + d / 2.f; lastCy = cy;
            outLastCx = lastCx;
            outLastCy = lastCy;
            outLastD = d;
        }
        Font* capf = font(8.2f * s, true);
        if (firstCx - d / 2.f - 38.f * s > panel.X + 4.f * s) {
            text(g, L"N端", RectF(firstCx - d / 2.f - 38.f * s, firstCy - 8.f * s, 34.f * s,
                                  16.f * s),
                 capf, MUTED, StringAlignmentFar, StringAlignmentCenter, false);
        } else {
            text(g, L"N端", RectF(firstCx - d / 2.f, area.Y - 2.f * s, 30.f * s, 12.f * s),
                 capf, MUTED, StringAlignmentNear, StringAlignmentCenter, false);
        }
        if (!flying) {
            if (lastCx + d / 2.f + 32.f * s < panel.X + panel.Width - 4.f * s) {
                text(g, L"C端", RectF(lastCx + d / 2.f + 5.f * s, lastCy - 8.f * s, 30.f * s,
                                      16.f * s),
                     capf, MUTED, StringAlignmentNear, StringAlignmentCenter, false);
            } else {
                text(g, L"C端", RectF(lastCx - d / 2.f, lastCy + d / 2.f + 2.f * s, 30.f * s,
                                      12.f * s),
                     capf, MUTED, StringAlignmentNear, StringAlignmentCenter, false);
            }
        }
    }

    if (legendH > 0) {
        float ly = panel.Y + panel.Height - 20.f * s;
        const char* props[5] = {"nonpolar", "polar", "acidic", "basic", "stop"};
        const wchar_t* names[5] = {L"非极性", L"极性", L"酸性", L"碱性", L"终止"};
        float lx = panel.X + 16.f * s;
        for (int i = 0; i < 5; ++i) {
            dot(g, lx + 4.f * s, ly + 7.f * s, 4.f * s, propColor(props[i]));
            text(g, names[i], RectF(lx + 12.f * s, ly, 54.f * s, 14.f * s), font(8.f * s),
                 MUTED, StringAlignmentNear, StringAlignmentCenter, false);
            lx += 72.f * s;
        }
    }
}

// 绘制当前 tRNA：氨基酸在 3' 端，反密码子在下方与 mRNA 密码子互补配对。
static void drawTrna(App& a, Graphics& g, const RectF& panel, const std::string& codonDna,
                     char aa, float x, float codonY, float codonW, float codonH,
                     float pairT, float leaveT, bool carrying, float yOffset) {
    float s = a.s;
    float visibility = 1.f - leaveT * 0.88f;
    if (visibility <= 0.03f) return;
    auto faded = [&](Color c, int alphaValue) {
        return alpha(c, (int)std::lround(alphaValue * visibility));
    };

    std::wstring anti = anticodonOf(codonDna);
    float antiH = 14.f * s;
    float antiY = codonY + codonH + 28.f * s + yOffset;
    float aaY = codonY - 35.f * s + yOffset;
    float halfW = codonW * 0.5f;

    // 简化的三叶草结构：左右/下方叶环 + 接纳茎，避免遮挡密码子本体。
    Color bodyFill = faded(ACCENT_SOFT, 170);
    Color bodyLine = faded(ACCENT, 110);
    RectF body(x - halfW - 7.f * s, antiY - 2.f * s, codonW + 14.f * s,
               antiH + 17.f * s);
    fillRound(g, body, 9.f * s, bodyFill);
    strokeRound(g, body, 9.f * s, bodyLine, 1.1f * s);
    line(g, x - 10.f * s, antiY + antiH + 7.f * s, x, antiY + antiH - 1.f * s,
         faded(ACCENT, 105), 1.2f * s);
    line(g, x + 10.f * s, antiY + antiH + 7.f * s, x, antiY + antiH - 1.f * s,
         faded(ACCENT, 105), 1.2f * s);
    line(g, x, antiY + antiH + 13.f * s, x, antiY + antiH - 1.f * s,
         faded(ACCENT, 105), 1.2f * s);
    dot(g, x - 11.f * s, antiY + antiH + 7.f * s, 5.2f * s, faded(ACCENT, 85));
    dot(g, x + 11.f * s, antiY + antiH + 7.f * s, 5.2f * s, faded(ACCENT, 85));
    dot(g, x, antiY + antiH + 14.f * s, 5.8f * s, faded(ACCENT, 95));

    // 反密码子按 3'→5' 方向展示；与同位置的 5'→3' 密码子逐碱基互补。
    float baseW = codonW / 3.f;
    for (int i = 0; i < 3; ++i) {
        float bx = x - halfW + i * baseW;
        float localPair = smooth01(pairT * 3.f - (float)i * 0.65f);
        if (localPair > 0.03f) {
            line(g, bx + baseW / 2.f, codonY + codonH - 1.f * s,
                 bx + baseW / 2.f, antiY - 1.f * s,
                 alpha(GREEN, (int)std::lround(145.f * localPair * visibility)),
                 1.25f * s);
        }
        RectF br(bx, antiY, baseW, antiH);
        text(g, std::wstring(1, anti[i]), br, font(8.8f * s, true, true),
             faded(baseColor((char)anti[i]), 235), StringAlignmentCenter,
             StringAlignmentCenter, false);
    }
    text(g, L"3'", RectF(x - halfW - 21.f * s, antiY, 18.f * s, antiH),
         font(7.2f * s, true), faded(MUTED, 230), StringAlignmentCenter,
         StringAlignmentCenter, false);
    text(g, L"5'", RectF(x + halfW + 3.f * s, antiY, 18.f * s, antiH),
         font(7.2f * s, true), faded(MUTED, 230), StringAlignmentCenter,
         StringAlignmentCenter, false);

    // 运载中的氨基酸挂在上端；移交后隐藏，由已有的飞入肽链动画接管。
    if (carrying) {
        const auto& info = dna::aminoAcid(aa);
        Color pc = propColor(info.property);
        GraphicsPath stem;
        stem.AddBezier(x, aaY + 9.f * s, x - 30.f * s, codonY - 4.f * s,
                       x - 30.f * s, antiY - 10.f * s, x - halfW + 2.f * s,
                       antiY + 5.f * s);
        Pen stemPen(faded(pc, 125), 1.5f * s);
        stemPen.SetDashStyle(DashStyleDash);
        g.DrawPath(&stemPen, &stem);
        aaBead(g, x, aaY, 24.f * s, aa, 10.f * s);
        std::wstring carryLabel = L"携带 " + utf8(info.code3);
        text(g, carryLabel,
             RectF(x + 13.f * s, aaY - 7.f * s, 58.f * s, 14.f * s),
             font(7.4f * s, true), faded(pc, 225), StringAlignmentNear,
             StringAlignmentCenter, true);
    } else {
        text(g, L"空载", RectF(x - halfW - 45.f * s, antiY + 1.f * s, 40.f * s, antiH),
             font(7.4f * s, true), faded(MUTED, 220), StringAlignmentFar,
             StringAlignmentCenter, true);
    }

    std::wstring labelText = L"tRNA 反密码子";
    RectF labelRect(x + halfW + 22.f * s, antiY - 1.f * s, 92.f * s, antiH + 2.f * s);
    if (labelRect.X + labelRect.Width > panel.X + panel.Width - 8.f * s)
        labelRect = RectF(x - halfW - 114.f * s, antiY - 1.f * s, 92.f * s, antiH + 2.f * s);
    text(g, labelText, labelRect, font(7.2f * s, true), faded(ACCENT, 225),
         StringAlignmentCenter, StringAlignmentCenter, true);
}

static void drawTranslation(App& a, Graphics& g) {
    float s = a.s;
    RectF inner = innerRect(a);
    int totalCodons = (int)a.tr.codons.size() + (a.tr.hasStop ? 1 : 0);
    int read = 0;
    int beads = 0;
    float pos = 0.f;
    float frac = 0.f;
    bool flying = false;
    float flyT = 1.f;
    if (a.hasSeq && totalCodons > 0) {
        pos = std::max(0.f, std::min((float)totalCodons,
                                     a.progress[2] * (float)totalCodons));
        read = std::min(totalCodons, (int)std::floor(pos));
        frac = pos - (float)std::floor(pos);
        if (read >= totalCodons) frac = 0.f;
        beads = std::min<int>(
            a.tr.protein.size(),
            read + ((read < (int)a.tr.protein.size() && frac >= 0.35f) ? 1 : 0));
        flying = a.progress[2] < 0.999f && read < (int)a.tr.protein.size() &&
                 frac >= 0.35f && frac < 0.85f;
        flyT = flying ? std::min(1.f, (frac - 0.35f) / 0.5f) : 1.f;
    }

    viewHeader(a, g, L"翻译", L"mRNA → 多肽链", progressText(a, 2));
    viewSubtitle(a, g, L"核糖体沿 mRNA 读取密码子，tRNA 搬运氨基酸，肽链从 N 端向 C 端延伸。");
    drawPlayback(a, g);

    text(g, L"阅读框", RectF(inner.X, inner.Y + 54.f * s, 50.f * s, 26.f * s),
         font(8.8f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, false);
    if (a.hasSeq) {
        std::wstring info;
        Color infoColor = MUTED;
        if (a.tr.hasStart && a.tr.hasStop)
            info = L"起始 ATG #" + std::to_wstring(a.tr.startIndex + 1) + L"  ·  终止 " +
                   rnaOf(a.tr.stopCodon) + L" #" + std::to_wstring(a.tr.stopIndex + 1);
        else if (!a.tr.hasStart) {
            info = L"未找到起始密码子 ATG";
            infoColor = AMBER;
        } else {
            info = L"未找到终止密码子";
            infoColor = AMBER;
        }
        text(g, info, RectF(inner.X, inner.Y + 56.f * s, inner.Width, 24.f * s),
             font(8.6f * s), infoColor, StringAlignmentFar, StringAlignmentCenter, true);
    }

    RectF content(inner.X, inner.Y + 92.f * s, inner.Width,
                  (float)a.playRc.top - 16.f * s - (inner.Y + 92.f * s));
    if (!a.hasSeq) {
        drawEmptyState(a, g, content, L"输入 DNA 序列以模拟翻译",
                       L"软件自动寻找起始密码子，并按照当前阅读框生成多肽链。");
        return;
    }
    if (a.tr.codons.empty()) {
        drawEmptyState(a, g, content, L"当前序列没有可翻译的密码子",
                       a.tr.hasStart ? L"ORF 过短或没有完整密码子。"
                                     : L"请检查序列或切换阅读框。");
        return;
    }

    float gap = 12.f * s;
    float stripH = 58.f * s;
    float availH = content.Height - stripH - 2 * gap;
    float proteinH = std::max(132.f * s, availH * 0.46f);
    if (proteinH > availH - 170.f * s) proteinH = std::max(110.f * s, availH - 170.f * s);
    float mrnaH = std::max(168.f * s, availH - proteinH);

    RectF proteinPanel(content.X, content.Y, content.Width, proteinH);
    float lastBeadCx = 0.f, lastBeadCy = 0.f, lastBeadD = 0.f;
    drawProteinPanel(a, g, proteinPanel, beads, flying, lastBeadCx, lastBeadCy, lastBeadD);

    RectF mrnaPanel(content.X, proteinPanel.GetBottom() + gap, content.Width, mrnaH);
    fillRound(g, mrnaPanel, 11.f * s, Color(255, 253, 254, 255));
    strokeRound(g, mrnaPanel, 11.f * s, Color(255, 235, 240, 248), 1.f);
    text(g, L"mRNA · 核糖体读取",
         RectF(mrnaPanel.X + 14.f * s, mrnaPanel.Y + 8.f * s, 260.f * s, 18.f * s),
         font(10.f * s, true), Color(255, 66, 82, 106), StringAlignmentNear,
         StringAlignmentCenter, false);
    text(g, L"5' → 3'",
         RectF(mrnaPanel.X, mrnaPanel.Y + 8.f * s, mrnaPanel.Width - 14.f * s, 18.f * s),
         font(8.6f * s, false, true), MUTED, StringAlignmentFar, StringAlignmentCenter, false);

    struct VisCodon {
        std::string dna;
        char aa;
        bool stop;
    };
    std::vector<VisCodon> list;
    for (size_t i = 0; i < a.tr.codons.size(); ++i)
        list.push_back({a.tr.codons[i], a.tr.protein[i], false});
    if (a.tr.hasStop) list.push_back({a.tr.stopCodon, '*', true});
    int total = (int)list.size();
    if (total <= 0) return;
    int ribo = std::max(0, std::min(read, total - 1));

    float areaW = mrnaPanel.Width - 48.f * s;
    float areaY = mrnaPanel.Y + 34.f * s;
    float areaH = mrnaPanel.Height - 44.f * s;
    float maxBlockW = 72.f * s;
    float cgap = 9.f * s;
    int maxN = std::max(4, std::min(10, (int)((areaW + cgap) / (maxBlockW + cgap))));
    float blockW = std::min(maxBlockW, (areaW - (maxN - 1) * cgap) / (float)maxN);
    if (blockW < 38.f * s) blockW = 38.f * s;
    float blockH = 26.f * s;
    float step = blockW + cgap;
    float centerX = mrnaPanel.X + mrnaPanel.Width / 2.f;
    float codonY = areaY + areaH * 0.50f;

    // 核糖体在密码子之间平滑滑动；长链时 mRNA 从核糖体下平滑滚过
    float glide = std::max(0.f, std::min(1.f, (frac - 0.55f) / 0.45f));
    glide = glide * glide * (3.f - 2.f * glide);
    float rpos = (float)ribo + (read < total ? glide : 0.f);
    rpos = std::min(rpos, (float)(total - 1));
    float cam;
    if (total <= maxN) cam = (total - 1) * 0.5f;
    else cam = std::max(maxN * 0.5f - 0.6f,
                        std::min((float)(total - 1) - maxN * 0.5f + 0.6f, rpos));

    int first = std::max(0, (int)std::floor(cam) - maxN / 2 - 2);
    int last = std::min(total - 1, (int)std::ceil(cam) + maxN / 2 + 2);

    float bigW = blockW + 20.f * s;
    float bigH = 30.f * s;
    float riboScreenX = centerX + (rpos - cam) * step;
    float codonCenterX = centerX + ((float)ribo - cam) * step;

    float trnaEnter = smooth01(frac / 0.20f);
    float trnaPair = smooth01((frac - 0.16f) / 0.17f);
    float trnaLeave = smooth01((frac - 0.48f) / 0.31f);
    bool trnaActive = a.progress[2] < 0.999f && ribo < (int)a.tr.protein.size() &&
                      !list[ribo].stop && frac > 0.01f && frac < 0.82f;
    float trnaX = codonCenterX + (1.f - trnaEnter) * (blockW * 1.75f + 36.f * s) -
                  trnaLeave * (blockW * 1.85f + 38.f * s);
    float trnaYOffset = (1.f - trnaEnter) * 8.f * s - trnaLeave * 5.f * s;

    GraphicsState savedClip = g.Save();
    gd::RectF codonClip(mrnaPanel.X + 10.f * s, codonY - 62.f * s,
                        mrnaPanel.Width - 20.f * s, blockH + 120.f * s);
    g.SetClip(codonClip, gd::CombineModeIntersect);

    RectF big(riboScreenX - bigW / 2.f, codonY - bigH - 8.f * s, bigW, bigH);
    RectF small(riboScreenX - bigW / 2.f, codonY + blockH + 4.f * s, bigW, 16.f * s);
    fillRound(g, big, 13.f * s, Color(52, 47, 107, 240));
    strokeRound(g, big, 13.f * s, Color(105, 47, 107, 240), 1.2f * s);
    fillRound(g, small, 8.f * s, Color(42, 47, 107, 240));
    strokeRound(g, small, 8.f * s, Color(90, 47, 107, 240), 1.2f * s);
    text(g, L"核糖体", big, font(7.8f * s, true), ACCENT, StringAlignmentCenter,
         StringAlignmentCenter, false);

    for (int ci = first; ci <= last; ++ci) {
        const auto& vc = list[ci];
        float x = centerX + (ci - cam) * step - blockW / 2.f;
        RectF card(x, codonY, blockW, blockH);
        bool cur = (ci == ribo);
        bool isStop = vc.stop;
        Color fill = isStop ? RED_SOFT : Color(255, 248, 250, 254);
        if (ci < read) fill = isStop ? RED_SOFT : Color(255, 243, 247, 253);
        if (cur) fill = isStop ? RED_SOFT : ACCENT_SOFT;
        fillRound(g, card, 8.f * s, fill);
        if (cur)
            strokeRound(g, card, 8.f * s, isStop ? alpha(RED, 150) : alpha(ACCENT, 170),
                        1.5f * s);
        else
            strokeRound(g, card, 8.f * s, Color(255, 233, 238, 246), 1.f);

        std::wstring rna = rnaOf(vc.dna);
        Font* cf = font(10.4f * s, true, true);
        float spacing = blockW / 3.f;
        for (int b = 0; b < 3; ++b) {
            wchar_t ch = rna[b];
            text(g, std::wstring(1, ch), RectF(x + b * spacing, codonY, spacing, blockH), cf,
                 baseColor((char)ch), StringAlignmentCenter, StringAlignmentCenter, false);
        }

        float adot = 15.f * s;
        float acx = x + blockW / 2.f;
        float acy = codonY + blockH + 22.f * s;
        bool newestFlying = flying && ci == read;
        bool trnaHere = trnaActive && ci == ribo;
        if (isStop) {
            if (read >= ci) {
                Font* sf = font(7.4f * s, true);
                RectF tag(acx - 13.f * s, acy - 6.f * s, 26.f * s, 12.f * s);
                fillRound(g, tag, 6.f * s, RED_SOFT);
                text(g, L"终止", tag, sf, RED, StringAlignmentCenter,
                     StringAlignmentCenter, false);
            } else {
                dot(g, acx, acy, 2.4f * s, Color(255, 207, 216, 229));
            }
        } else if (ci < beads) {
            if (!newestFlying) aaBead(g, acx, acy, adot, vc.aa, 7.6f * s);
        } else if (!trnaHere) {
            dot(g, acx, acy, 2.4f * s, Color(255, 207, 216, 229));
        }
        if (a.tr.hasStart && ci < (int)a.tr.codons.size() &&
            a.tr.codonPos[ci] == a.tr.startIndex) {
            Font* tagf = font(7.2f * s, true);
            RectF tag(x, codonY + blockH + 4.f * s, 28.f * s, 12.f * s);
            fillRound(g, tag, 5.f * s, GREEN_SOFT);
            text(g, L"起始", tag, tagf, GREEN, StringAlignmentCenter,
                 StringAlignmentCenter, false);
        }
    }

    if (trnaActive) {
        drawTrna(a, g, mrnaPanel, list[ribo].dna, list[ribo].aa, trnaX, codonY, blockW,
                 blockH, trnaPair, trnaLeave, frac < 0.35f, trnaYOffset);
    }

    // 密码子条边缘渐隐，遮住滑入/滑出的半张卡片
    {
        float fadeW = 24.f * s;
        float fy = codonY - 4.f * s;
        float fh = blockH + 52.f * s;
        RectF lf(mrnaPanel.X + 10.f * s, fy, fadeW, fh);
        LinearGradientBrush lgb(lf, Color(255, 253, 254, 255),
                                Color(0, 253, 254, 255), LinearGradientModeHorizontal);
        g.FillRectangle(&lgb, lf);
        RectF rf(mrnaPanel.X + mrnaPanel.Width - 10.f * s - fadeW, fy, fadeW, fh);
        LinearGradientBrush rgb(rf, Color(0, 253, 254, 255),
                                Color(255, 253, 254, 255), LinearGradientModeHorizontal);
        g.FillRectangle(&rgb, rf);
    }
    g.Restore(savedClip);

    // 新氨基酸沿弧线从核糖体飞入肽链，落位后由多肽链接管
    if (flying && read < (int)a.tr.protein.size() && lastBeadD > 0) {
        char aa = a.tr.protein[read];
        const auto& info = dna::aminoAcid(aa);
        Color pc = propColor(info.property);
        float t = flyT;
        float te = t * t * (3.f - 2.f * t);
        float sx = codonCenterX;
        float sy = codonY - bigH - 5.f * s;
        float ex = lastBeadCx;
        float ey = lastBeadCy;
        float cpx = (sx + ex) * 0.5f + 18.f * s;
        float cpy = std::min(sy, ey) - 44.f * s;
        auto bez = [&](float u) {
            float v = 1.f - u;
            return PointF(v * v * sx + 2 * v * u * cpx + u * u * ex,
                          v * v * sy + 2 * v * u * cpy + u * u * ey);
        };
        GraphicsPath trail;
        trail.StartFigure();
        PointF prev = bez(0);
        for (int q = 1; q <= 14; ++q) {
            PointF pt = bez(te * q / 14.f);
            trail.AddLine(prev, pt);
            prev = pt;
        }
        Pen tp(alpha(pc, 90), 1.6f * s);
        tp.SetDashStyle(DashStyleDash);
        g.DrawPath(&tp, &trail);

        PointF cur = bez(te);
        float d = lastBeadD * (0.55f + 0.45f * te);
        dot(g, cur.X, cur.Y, d * 0.72f, alpha(pc, 55));
        aaBead(g, cur.X, cur.Y, d, aa,
               std::max(7.f * s, 10.5f * s * (0.6f + 0.4f * te)));
    }

    RectF strip(content.X, mrnaPanel.GetBottom() + gap, content.Width, stripH);
    fillRound(g, strip, 9.f * s, Color(255, 249, 251, 254));
    strokeRound(g, strip, 9.f * s, Color(255, 233, 239, 247), 1.f);

    bool done = a.progress[2] >= 0.999f;
    std::wstring line1, line2;
    Color stripAccent = ACCENT;
    if (!a.tr.hasStart) {
        line1 = L"未找到起始密码子 ATG";
        line2 = L"已按当前阅读框直接翻译；可切换阅读框或反向互补后重试。";
        stripAccent = AMBER;
    } else if (done && a.tr.hasStop) {
        line1 = L"到达终止密码子 " + rnaOf(a.tr.stopCodon) + L" · 翻译结束";
        line2 = L"多肽链从核糖体释放，蛋白质一级结构生成完成。";
        stripAccent = GREEN;
    } else if (done && !a.tr.hasStop) {
        line1 = L"mRNA 已读完，但未找到终止密码子";
        line2 = L"多肽链可能不完整；请检查序列或更换阅读框。";
        stripAccent = AMBER;
    } else if (read == 0 && frac < 0.01f) {
        line1 = L"准备就绪 · 核糖体将扫描 mRNA";
        line2 = L"识别起始密码子 AUG 后，开始逐个读取密码子并添加氨基酸。";
    } else if (ribo < (int)list.size() && list[ribo].stop) {
        line1 = L"识别终止密码子 " + rnaOf(list[ribo].dna);
        line2 = L"终止密码子没有对应的 tRNA；释放因子促使多肽链释放。";
    } else if (ribo < (int)list.size() && ribo < (int)a.tr.protein.size()) {
        char aa = a.tr.protein[ribo];
        const auto& info = dna::aminoAcid(aa);
        std::wstring codonRna = rnaOf(list[ribo].dna);
        std::wstring antiRna = anticodonOf(list[ribo].dna);
        line1 = L"正在读取 " + codonRna + L" → " + utf8(info.nameZh) + L" " +
                utf8(info.code3) + L" / " + std::wstring(1, (wchar_t)aa);
        if (frac < 0.18f)
            line2 = L"携带氨基酸的 tRNA 正在进入核糖体 A 位点。";
        else if (frac < 0.35f)
            line2 = L"反密码子 3'-" + antiRna + L"-5' 与密码子 5'-" + codonRna +
                    L"-3' 互补配对。";
        else if (frac < 0.50f)
            line2 = L"氨基酸从 tRNA 移交给肽链，空载 tRNA 准备离开。";
        else if (frac < 0.82f)
            line2 = L"空载 tRNA 正在离开核糖体，为下一个 tRNA 腾出位点。";
        else
            line2 = L"核糖体沿 mRNA 向 3' 端移动，准备读取下一个密码子。";
    } else {
        line1 = L"翻译进行中";
        line2 = L"核糖体沿 mRNA 向 3' 端移动。";
    }

    float textX = strip.X + 16.f * s;
    if (total > 0) {
        RectF cbox(strip.X + 12.f * s, strip.Y + 14.f * s, 52.f * s, 30.f * s);
        std::string curDna = list[ribo].dna;
        bool stop = list[ribo].stop;
        fillRound(g, cbox, 7.f * s, stop ? RED_SOFT : ACCENT_SOFT);
        Font* cf = font(10.2f * s, true, true);
        std::wstring rna = rnaOf(curDna);
        for (int b = 0; b < 3; ++b)
            text(g, std::wstring(1, rna[b]),
                 RectF(cbox.X + b * 17.f * s, cbox.Y, 17.f * s, cbox.Height), cf,
                 baseColor(rna[b]), StringAlignmentCenter, StringAlignmentCenter, false);
        textX = cbox.X + cbox.Width + 12.f * s;
    }
    text(g, line1,
         RectF(textX, strip.Y + 7.f * s, strip.Width - (textX - strip.X) - 14.f * s, 18.f * s),
         font(10.2f * s, true), stripAccent, StringAlignmentNear, StringAlignmentCenter, true);
    text(g, line2,
         RectF(textX, strip.Y + 25.f * s, strip.Width - (textX - strip.X) - 14.f * s, 20.f * s),
         font(8.8f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, true);
}

// ================================================================== 密码子表
static void drawCodonTable(App& a, Graphics& g) {
    float s = a.s;
    RectF inner = innerRect(a);
    viewHeader(a, g, L"密码子表", L"64 个密码子", L"");
    viewSubtitle(a, g,
                 L"标准遗传密码 · 行 = 第 1·2 位碱基，列 = 第 3 位 · 点击密码子查看详情");
    text(g, a.rnaTable ? L"以 RNA（U）显示" : L"以 DNA（T）显示",
         RectF(inner.X, inner.Y + 30.f * s, inner.Width, 18.f * s), font(8.4f * s, false, true),
         MUTED, StringAlignmentFar, StringAlignmentCenter, false);

    if (!a.codonCellsValid) return;
    const auto& prefixes = dna::codonFamilyPrefixes();
    RectF header = toRectF(a.codonHeaderRc);
    text(g, L"密码子", RectF(header.X, header.Y, 56.f * s, header.Height),
         font(8.4f * s, true), MUTED, StringAlignmentCenter, StringAlignmentCenter, false);
    float prefixW = 56.f * s;
    float colGap = 5.f * s;
    float cellW = (inner.Width - prefixW - 4 * colGap) / 4.f;
    for (int c = 0; c < 4; ++c) {
        char base = "TCAG"[c];
        if (a.rnaTable && base == 'T') base = 'U';
        float cx = inner.X + prefixW + colGap + c * (cellW + colGap) + cellW / 2.f;
        Font* hbase = font(10.2f * s, true, true);
        const wchar_t* l3 = L"第 3 位";
        Font* lf = font(7.8f * s);
        float bw = 14.f * s;
        float l3w = textWidth(g, l3, lf);
        float total = bw + 6.f * s + l3w;
        float startX = cx - total / 2.f;
        text(g, std::wstring(1, (wchar_t)base),
             RectF(startX, header.Y, bw, header.Height), hbase,
             baseColor(base), StringAlignmentCenter, StringAlignmentCenter, false);
        text(g, l3, RectF(startX + bw + 6.f * s, header.Y, l3w + 4.f * s, header.Height),
             lf, MUTED, StringAlignmentNear, StringAlignmentCenter, false);
    }

    Font* codF = font(10.4f * s, true, true);
    Font* nameF = font(9.2f * s);
    Font* cntF = font(7.8f * s, true, true);
    for (int f = 0; f < 16; ++f) {
        const std::string& pref = prefixes[f];
        RECT first = a.codonCellRects[f * 4];
        float rowY = (float)first.top;
        float rowH = (float)(first.bottom - first.top);
        RectF prefRc(inner.X, rowY, prefixW - 2.f * s, rowH);
        fillRound(g, prefRc, 5.f * s, Color(255, 249, 251, 254));
        Font* pf = font(9.6f * s, true, true);
        float px = prefRc.X + prefRc.Width / 2.f;
        char b0 = pref[0], b1 = pref[1];
        if (a.rnaTable) { if (b0 == 'T') b0 = 'U'; if (b1 == 'T') b1 = 'U'; }
        text(g, std::wstring(1, (wchar_t)b0), RectF(px - 18.f * s, rowY, 16.f * s, rowH),
             pf, baseColor(b0), StringAlignmentCenter, StringAlignmentCenter, false);
        text(g, std::wstring(1, (wchar_t)b1), RectF(px + 2.f * s, rowY, 16.f * s, rowH),
             pf, baseColor(b1), StringAlignmentCenter, StringAlignmentCenter, false);
        for (int c = 0; c < 4; ++c) {
            int idx = f * 4 + c;
            std::string codon = dna::displayCodon(idx);
            char aa = dna::codonToAa(codon);
            const auto& info = dna::aminoAcid(aa);
            bool stop = (aa == '*');
            int usage = a.hasSeq ? dna::codonUsageInOrf(a.tr, codon) : 0;
            bool used = usage > 0;
            bool sel = a.selectedCodon == idx;
            bool hov = a.hoverCodon == idx;
            RectF cell = toRectF(a.codonCellRects[idx]);
            Color bg = Color(255, 253, 254, 255);
            if (used) bg = mix(bg, ACCENT_SOFT, 0.7f);
            if (hov) bg = mix(bg, Color(255, 238, 245, 255), 0.8f);
            if (sel) bg = ACCENT_SOFT;
            fillRound(g, cell, 5.f * s, bg);
            if (sel) strokeRound(g, cell, 5.f * s, ACCENT, 1.5f * s);
            else if (hov) strokeRound(g, cell, 5.f * s, alpha(ACCENT, 80), 1.f);
            else strokeRound(g, cell, 5.f * s, Color(255, 241, 244, 249), 1.f);
            RectF bar(cell.X + 1.f * s, cell.Y + 4.f * s, 2.4f * s, cell.Height - 8.f * s);
            fillRound(g, bar, 1.2f * s, alpha(propColor(info.property), 180));

            std::wstring shown = a.rnaTable ? rnaOf(codon) : utf8(codon);
            text(g, shown, RectF(cell.X + 9.f * s, cell.Y, 48.f * s, cell.Height), codF,
                 stop ? Color(255, 196, 70, 82) : TXT, StringAlignmentNear,
                 StringAlignmentCenter, false);
            if (cell.Width > 128.f * s) {
                text(g, utf8(info.nameZh),
                     RectF(cell.X + 57.f * s, cell.Y, cell.Width - 105.f * s, cell.Height),
                     nameF, stop ? Color(255, 196, 70, 82) : SUB, StringAlignmentNear,
                     StringAlignmentCenter, true);
            }
            if (usage > 1) {
                text(g, L"×" + std::to_wstring(usage),
                     RectF(cell.X + cell.Width - 62.f * s, cell.Y, 30.f * s, cell.Height),
                     cntF, ACCENT, StringAlignmentFar, StringAlignmentCenter, false);
            }
            float ad = 15.f * s;
            float acx = cell.X + cell.Width - 15.f * s;
            Color pc = propColor(info.property);
            dot(g, acx, cell.Y + cell.Height / 2.f, ad / 2.f, pc);
            text(g, std::wstring(1, (wchar_t)(stop ? '*' : aa)),
                 RectF(acx - ad / 2.f, cell.Y + cell.Height / 2.f - ad / 2.f, ad, ad),
                 font(8.f * s, true), Color(255, 255, 255, 255), StringAlignmentCenter,
                 StringAlignmentCenter, false);
        }
    }

    RectF detail = toRectF(a.codonDetailRc);
    fillRound(g, detail, 9.f * s, Color(255, 249, 251, 254));
    strokeRound(g, detail, 9.f * s, Color(255, 233, 239, 247), 1.f);
    if (a.selectedCodon >= 0 && a.selectedCodon < 64) {
        std::string codon = dna::displayCodon(a.selectedCodon);
        char aa = dna::codonToAa(codon);
        const auto& info = dna::aminoAcid(aa);
        bool stop = aa == '*';
        float x = detail.X + 14.f * s;
        text(g, utf8(codon), RectF(x, detail.Y, 60.f * s, detail.Height),
             font(16.f * s, true, true), stop ? RED : ACCENT, StringAlignmentNear,
             StringAlignmentCenter, false);
        text(g, L"/ " + rnaOf(codon), RectF(x + 52.f * s, detail.Y, 70.f * s, detail.Height),
             font(11.f * s, true, true), MUTED, StringAlignmentNear, StringAlignmentCenter,
             false);
        float ad = 30.f * s;
        float acx = x + 150.f * s;
        dot(g, acx, detail.Y + detail.Height / 2.f, ad / 2.f, propColor(info.property));
        text(g, std::wstring(1, (wchar_t)(stop ? '*' : aa)),
             RectF(acx - ad / 2.f, detail.Y + detail.Height / 2.f - ad / 2.f, ad, ad),
             font(12.f * s, true), Color(255, 255, 255, 255), StringAlignmentCenter,
             StringAlignmentCenter, false);
        text(g, utf8(info.nameZh) + L"  " + utf8(info.code3),
             RectF(x + 174.f * s, detail.Y + 6.f * s, 220.f * s, 20.f * s),
             font(11.2f * s, true), TXT, StringAlignmentNear, StringAlignmentCenter, true);
        bool isStart = (codon == "ATG");
        std::wstring prop = stop ? L"终止密码子" : utf8(info.propertyZh);
        if (isStart) prop = L"起始密码子 · " + prop;
        text(g, prop, RectF(x + 174.f * s, detail.Y + 26.f * s, 260.f * s, 18.f * s),
             font(8.6f * s), isStart ? GREEN : SUB, StringAlignmentNear,
             StringAlignmentCenter, true);
        int usage = a.hasSeq ? dna::codonUsageInOrf(a.tr, codon) : 0;
        std::wstring usageTxt = L"当前 ORF 使用 " + std::to_wstring(usage) + L" 次";
        if (usage == 0) usageTxt = a.hasSeq ? L"当前 ORF 未使用" : L"尚未载入序列";
        text(g, usageTxt, RectF(detail.X, detail.Y, detail.Width - 14.f * s, detail.Height),
             font(8.8f * s), MUTED, StringAlignmentFar, StringAlignmentCenter, false);
    } else {
        text(g, L"点击任意密码子查看详情 · 蓝色底色表示当前 ORF 使用过该密码子",
             RectF(detail.X + 14.f * s, detail.Y, detail.Width - 28.f * s, detail.Height),
             font(9.4f * s), MUTED, StringAlignmentNear, StringAlignmentCenter, true);
    }
}

// ================================================================== 入口
void drawViews(App& a, Graphics& g, const RECT& content) {
    (void)content;
    switch (a.tab) {
        case 0: drawReplication(a, g); break;
        case 1: drawTranscription(a, g); break;
        case 2: drawTranslation(a, g); break;
        case 3: drawCodonTable(a, g); break;
        default: break;
    }
}
