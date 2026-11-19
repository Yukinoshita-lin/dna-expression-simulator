# -*- coding: utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Yukinoshita-lin
"""生成应用图标：深色圆角底 + 青蓝 DNA 双螺旋（无文字）。"""
import math
from PIL import Image, ImageDraw, ImageFilter

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

# 圆角底板 + 垂直渐变
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle((48, 48, S - 48, S - 48), radius=220, fill=255)
grad = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gd = ImageDraw.Draw(grad)
top = (10, 24, 48)
bottom = (21, 58, 105)
for y in range(S):
    t = y / (S - 1)
    c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
    gd.line([(0, y), (S, y)], fill=c, width=1)
img.paste(grad, (0, 0), mask)

d = ImageDraw.Draw(img)
# 内描边
d.rounded_rectangle((48, 48, S - 48, S - 48), radius=220, outline=(96, 170, 255, 60), width=3)

# 柔光层
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gdr = ImageDraw.Draw(glow)
cx, cy = S // 2, S // 2
amp = 150
y0, y1 = 205, 819
phase = 0.0
steps = 220


def helix_points(ph):
    pts = []
    for i in range(steps + 1):
        t = i / steps
        y = y0 + (y1 - y0) * t
        x = cx + math.sin(t * math.pi * 3.0 + ph) * amp
        pts.append((x, y))
    return pts


def color_at(t):
    # 青色 -> 天蓝 -> 蓝紫
    a = (94, 234, 212)
    b = (56, 182, 255)
    c = (99, 102, 241)
    if t < 0.5:
        k = t / 0.5
        rgb = tuple(int(a[i] + (b[i] - a[i]) * k) for i in range(3))
    else:
        k = (t - 0.5) / 0.5
        rgb = tuple(int(b[i] + (c[i] - b[i]) * k) for i in range(3))
    return rgb + (255,)


for ph in (0.0, math.pi):
    pts = helix_points(ph)
    for i in range(len(pts) - 1):
        t = i / (len(pts) - 1)
        col = color_at(t)
        gdr.line([pts[i], pts[i + 1]], fill=col, width=54)
        gdr.ellipse((pts[i][0] - 27, pts[i][1] - 27, pts[i][0] + 27, pts[i][1] + 27), fill=col)

# 横向碱基对（连接两条链）
for k in range(9):
    t = 0.06 + 0.88 * k / 8
    y = y0 + (y1 - y0) * t
    x1 = cx + math.sin(t * math.pi * 3.0) * amp
    x2 = cx - math.sin(t * math.pi * 3.0) * amp
    a = int(80 + 120 * (math.sin(t * math.pi * 3.0) ** 2))
    gdr.line([(x1, y), (x2, y)], fill=(170, 235, 255, a), width=15)

glow = glow.filter(ImageFilter.GaussianBlur(26))
img.alpha_composite(glow)

# 主体线条（清晰）
d = ImageDraw.Draw(img)
for ph in (0.0, math.pi):
    pts = helix_points(ph)
    for i in range(len(pts) - 1):
        t = i / (len(pts) - 1)
        d.line([pts[i], pts[i + 1]], fill=color_at(t), width=42)
for k in range(9):
    t = 0.06 + 0.88 * k / 8
    y = y0 + (y1 - y0) * t
    x1 = cx + math.sin(t * math.pi * 3.0) * amp
    x2 = cx - math.sin(t * math.pi * 3.0) * amp
    d.line([(x1, y), (x2, y)], fill=(214, 247, 255, 235), width=11)

# 链端小圆
for ph in (0.0, math.pi):
    pts = helix_points(ph)
    for p, col in ((pts[0], color_at(0)), (pts[-1], color_at(1))):
        d.ellipse((p[0] - 32, p[1] - 32, p[0] + 32, p[1] + 32), fill=col)

img.save("assets/app_icon_1024.png")

sizes = [16, 24, 32, 48, 64, 128, 256]
icos = [img.resize((n, n), Image.LANCZOS) for n in sizes]
icos[-1].save("src/app.ico", format="ICO", sizes=[(n, n) for n in sizes])
print("icons written")
