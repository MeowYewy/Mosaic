"""Generate app-icon.png and app-icon.ico from resources/logo.svg geometry."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT_PNG = ROOT / "resources" / "app-icon.png"
OUT_ICO = ROOT / "resources" / "app-icon.ico"

ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)


def _scale(value: float, factor: float) -> int:
    return max(1, round(value * factor))


def render_logo(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    s = size / 128.0

    draw.rounded_rectangle(
        (0, 0, size - 1, size - 1),
        radius=_scale(28, s),
        fill=(15, 118, 110, 255),
    )
    draw.rounded_rectangle(
        (_scale(24, s), _scale(28, s), _scale(104, s), _scale(100, s)),
        radius=_scale(10, s),
        fill=(255, 255, 255, 242),
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(44, s), _scale(92, s), _scale(56, s)),
        radius=_scale(4, s),
        fill=(19, 78, 74, 255),
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(64, s), _scale(76, s), _scale(74, s)),
        radius=_scale(4, s),
        fill=(94, 234, 212, 255),
    )
    draw.rounded_rectangle(
        (_scale(36, s), _scale(82, s), _scale(84, s), _scale(92, s)),
        radius=_scale(4, s),
        fill=(19, 78, 74, 89),
    )
    return img


def main() -> None:
    master = render_logo(256)
    master.save(OUT_PNG, format="PNG")

    icons = [render_logo(size) for size in ICO_SIZES]
    icons[-1].save(
        OUT_ICO,
        format="ICO",
        sizes=[(size, size) for size in ICO_SIZES],
        append_images=icons[:-1],
    )
    print(f"Wrote {OUT_PNG}")
    print(f"Wrote {OUT_ICO}")


if __name__ == "__main__":
    main()
