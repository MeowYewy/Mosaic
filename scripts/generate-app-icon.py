"""Generate app-icon.png and app-icon.ico from resources/logo.svg geometry.

Renders a large master bitmap, then downscales with Lanczos for smooth edges.
Sizes <= 32 use a simplified layout so 1px bars do not look harsh on taskbars.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "resources" / "app-icon.png"
OUT_ICO = ROOT / "resources" / "app-icon.ico"

MASTER_SIZE = 512
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)

# logo.svg palette
C_BG = (15, 118, 110, 255)
C_PAPER = (255, 255, 255, 242)
C_BAR1 = (19, 78, 74, 255)
C_BAR2 = (94, 234, 212, 255)
C_BAR3 = (19, 78, 74, 89)


def _scale(value: float, factor: float) -> int:
    return max(1, round(value * factor))


def _clamp(v: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, v))


def render_logo(size: int, *, compact: bool = False) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    s = size / 128.0

    outer_r = _scale(28, s)
    draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=outer_r, fill=C_BG)

    if compact and size <= 32:
        # Small sizes: thicker bars, tighter layout — avoids 1px harsh lines.
        pad = max(2, round(size * 0.12))
        inner_r = max(2, round(size * 0.12))
        x0, y0 = pad, pad + max(1, round(size * 0.04))
        x1, y1 = size - pad - 1, size - pad - 1
        draw.rounded_rectangle((x0, y0, x1, y1), radius=inner_r, fill=C_PAPER)

        bar_x = x0 + max(2, round(size * 0.14))
        bar_w = x1 - bar_x - max(2, round(size * 0.14))
        bar_h1 = max(2, round(size * 0.14))
        bar_h2 = max(2, round(size * 0.11))
        gap = max(2, round(size * 0.08))
        y = y0 + max(3, round(size * 0.18))
        draw.rounded_rectangle(
            (bar_x, y, bar_x + bar_w, y + bar_h1), radius=max(1, bar_h1 // 3), fill=C_BAR1
        )
        y += bar_h1 + gap
        draw.rounded_rectangle(
            (bar_x, y, bar_x + _clamp(round(bar_w * 0.72), 2, bar_w), y + bar_h2),
            radius=max(1, bar_h2 // 3),
            fill=C_BAR2,
        )
        y += bar_h2 + gap
        draw.rounded_rectangle(
            (bar_x, y, bar_x + _clamp(round(bar_w * 0.85), 2, bar_w), y + bar_h2),
            radius=max(1, bar_h2 // 3),
            fill=C_BAR3,
        )
        return img

    draw.rounded_rectangle(
        (_scale(24, s), _scale(28, s), _scale(104, s), _scale(100, s)),
        radius=_scale(10, s),
        fill=C_PAPER,
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(44, s), _scale(92, s), _scale(56, s)),
        radius=max(1, _scale(4, s)),
        fill=C_BAR1,
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(64, s), _scale(76, s), _scale(74, s)),
        radius=max(1, _scale(4, s)),
        fill=C_BAR2,
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(82, s), _scale(84, s), _scale(92, s)),
        radius=max(1, _scale(4, s)),
        fill=C_BAR3,
    )
    return img


def icon_for_size(master: Image.Image, size: int) -> Image.Image:
    if size <= 32:
        icon = render_logo(size, compact=True)
    elif size >= MASTER_SIZE:
        icon = master.copy()
    else:
        icon = master.resize((size, size), Image.Resampling.LANCZOS)
    # Very light soften on tiny taskbar sizes only (reduces "over-sharp" halos).
    if size <= 20:
        icon = icon.filter(ImageFilter.GaussianBlur(radius=0.4))
    return icon


def main() -> None:
    master = render_logo(MASTER_SIZE)
    png_out = master.resize((256, 256), Image.Resampling.LANCZOS)
    png_out.save(OUT_PNG, format="PNG", optimize=True)

    icons = [icon_for_size(master, size) for size in ICO_SIZES]
    icons[-1].save(
        OUT_ICO,
        format="ICO",
        sizes=[(size, size) for size in ICO_SIZES],
        append_images=icons[:-1],
    )
    print(f"Wrote {OUT_PNG} (256px, from {MASTER_SIZE}px master)")
    print(f"Wrote {OUT_ICO} (sizes: {', '.join(map(str, ICO_SIZES))})")


if __name__ == "__main__":
    main()
