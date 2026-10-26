#!/usr/bin/env python3
"""Erzeugt Add-on-Icon, Fanart und README-Banner.

- assets/icon.svg      Finsternis aus Pixelstaub (Vektor, fester Zufallssamen)
- assets/banner.svg    Titel links, Ausschnitt aus assets/eclipse-on-pi.png rechts
- PNG/JPG daraus:      visualization.spektra/resources/icon.png (512×512),
                       visualization.spektra/resources/fanart.jpg (1920×1080),
                       assets/banner.png (1280×360)

Braucht rsvg-convert (librsvg) und magick (ImageMagick).
"""

import base64
import math
import random
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
RESOURCES = ROOT / "visualization.spektra" / "resources"
SCREENSHOT = ASSETS / "eclipse-on-pi.png"

# Flammenfarben wie in der Szene: weiß -> silber -> gold -> glut -> rubin
STOPS = [(250, 250, 255), (222, 230, 242), (226, 182, 110), (214, 96, 46), (190, 22, 48)]


def fire(h):
    h = min(0.999, max(0.0, h)) * (len(STOPS) - 1)
    i = int(h)
    k = h - i
    a, b = STOPS[i], STOPS[i + 1]
    r, g, b2 = (round(a[c] + (b[c] - a[c]) * k) for c in range(3))
    return f"#{r:02x}{g:02x}{b2:02x}"


def icon_svg():
    rng = random.Random(7)
    size, cx, cy, radius = 512, 256, 256, 112
    pixels = []
    # Fünf Flammenzungen und ein dünner Saum rundum, wie die Szene bei einem Puls
    tongues = [rng.uniform(0, math.tau) for _ in range(5)]
    for _ in range(1500):
        if rng.random() < 0.7:
            a = rng.choice(tongues) + rng.gauss(0, 0.22)
            reach = rng.expovariate(1 / 0.38)
        else:
            a = rng.uniform(0, math.tau)
            reach = rng.expovariate(1 / 0.14)
        r = radius * (1.04 + reach)
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r
        h = min(1.0, (r / radius - 1.0) / 0.9)
        opacity = max(0.08, 0.95 - h * 0.8)
        px = rng.choice((5, 6, 6, 8, 10))
        if 14 < x < size - 14 - px and 14 < y < size - 14 - px:
            pixels.append(f'<rect x="{x:.0f}" y="{y:.0f}" width="{px}" height="{px}" '
                          f'fill="{fire(h)}" opacity="{opacity:.2f}"/>')
    ring = f'<circle cx="{cx}" cy="{cy}" r="{radius * 1.7:.0f}" fill="none" stroke="#E2B66E" ' \
        'stroke-opacity="0.22" stroke-width="2"/>'
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}"
     viewBox="0 0 {size} {size}">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#14101C"/><stop offset="1" stop-color="#050407"/>
    </linearGradient>
    <radialGradient id="glow" cx="0.5" cy="0.5" r="0.5">
      <stop offset="0.42" stop-color="#FFC454" stop-opacity="0.55"/>
      <stop offset="0.6" stop-color="#E2B66E" stop-opacity="0.18"/>
      <stop offset="1" stop-color="#E2B66E" stop-opacity="0"/>
    </radialGradient>
    <clipPath id="tile"><rect width="{size}" height="{size}" rx="108"/></clipPath>
  </defs>
  <rect width="{size}" height="{size}" rx="108" fill="url(#bg)"/>
  <g clip-path="url(#tile)">
    <circle cx="{cx}" cy="{cy}" r="250" fill="url(#glow)"/>
    {"".join(pixels)}
    {ring}
  </g>
  <circle cx="{cx}" cy="{cy}" r="{radius}" fill="#000"/>
  <circle cx="{cx}" cy="{cy}" r="{radius}" fill="none" stroke="#FAFAFF" stroke-width="7"/>
</svg>
"""


def banner_svg(crop_jpg):
    data = base64.b64encode(crop_jpg.read_bytes()).decode("ascii")
    return f"""<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"
     width="1280" height="360" viewBox="0 0 1280 360">
  <defs>
    <linearGradient id="fade" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#050407" stop-opacity="1"/>
      <stop offset="0.42" stop-color="#050407" stop-opacity="1"/>
      <stop offset="0.62" stop-color="#050407" stop-opacity="0"/>
    </linearGradient>
  </defs>
  <rect width="1280" height="360" fill="#050407"/>
  <image x="480" y="0" width="800" height="360" xlink:href="data:image/jpeg;base64,{data}"/>
  <rect width="1280" height="360" fill="url(#fade)"/>
  <text x="72" y="178" font-family="Noto Sans, DejaVu Sans, Helvetica, Arial, sans-serif" font-size="96"
        font-weight="700" letter-spacing="6" fill="#FAFAFF">SPEKTRA</text>
  <text x="76" y="232" font-family="Noto Sans, DejaVu Sans, Helvetica, Arial, sans-serif" font-size="30"
        fill="#E2B66E">Audio-reactive scenes for Kodi</text>
  <text x="76" y="276" font-family="Noto Sans, DejaVu Sans, Helvetica, Arial, sans-serif" font-size="22"
        fill="#9A93A8">Bass · Lows · Mids · Highs → Eclipse</text>
</svg>
"""


def run(*args):
    subprocess.run(args, check=True)


def main():
    (ASSETS / "icon.svg").write_text(icon_svg(), encoding="utf-8")
    run("rsvg-convert", "-w", "512", "-h", "512", str(ASSETS / "icon.svg"),
        "-o", str(RESOURCES / "icon.png"))
    # Fanart: der echte Screenshot vom Pi
    run("magick", str(SCREENSHOT), "-quality", "88", str(RESOURCES / "fanart.jpg"))
    # Banner: mittiger Ausschnitt des Screenshots im Seitenverhältnis 800:360
    crop = ASSETS / ".banner-crop.jpg"
    run("magick", str(SCREENSHOT), "-gravity", "center", "-crop", "1500x675+0+0", "+repage",
        "-resize", "800x360", "-quality", "88", str(crop))
    (ASSETS / "banner.svg").write_text(banner_svg(crop), encoding="utf-8")
    crop.unlink()
    run("rsvg-convert", "-w", "1280", "-h", "360", str(ASSETS / "banner.svg"),
        "-o", str(ASSETS / "banner.png"))
    print("geschrieben: resources/icon.png, resources/fanart.jpg, assets/icon.svg, assets/banner.svg, "
          "assets/banner.png")


if __name__ == "__main__":
    main()
