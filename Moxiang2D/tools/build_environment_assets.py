#!/usr/bin/env python3
"""Build reusable dungeon environment sprites from the authored decal sheet.

This keeps source art in one place while producing tightly cropped runtime files
and a small pulsing rune sheet for the environment animation system.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
from PIL import Image, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Assets" / "tiles" / "dungeon_decals.png"
OUTPUT = ROOT / "Assets" / "environment"

CROPS = {
    "stone_medallion.png": (18, 16, 325, 319),
    "ritual_square.png": (334, 42, 574, 283),
    "dragon_panel.png": (14, 493, 362, 652),
    "hanging_scroll.png": (825, 26, 1018, 315),
    "azure_flame_source.png": (690, 800, 830, 996),
}


def trim(image: Image.Image, padding: int = 6) -> Image.Image:
    alpha = image.getchannel("A")
    box = alpha.getbbox()
    if box is None:
        return image
    left, top, right, bottom = box
    return image.crop((
        max(0, left - padding),
        max(0, top - padding),
        min(image.width, right + padding),
        min(image.height, bottom + padding),
    ))


def contain(image: Image.Image, size: tuple[int, int], scale: float = 1.0) -> Image.Image:
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    target_w = max(1, int(size[0] * scale))
    target_h = max(1, int(size[1] * scale))
    copy = image.copy()
    copy.thumbnail((target_w, target_h), Image.Resampling.LANCZOS)
    x = (size[0] - copy.width) // 2
    y = (size[1] - copy.height) // 2
    canvas.alpha_composite(copy, (x, y))
    return canvas


def make_pulse_sheet(source: Image.Image) -> Image.Image:
    frame_size = (160, 224)
    columns = 4
    rows = 4
    sheet = Image.new("RGBA", (frame_size[0] * columns, frame_size[1] * rows), (0, 0, 0, 0))

    for frame in range(columns * rows):
        phase = frame / float(columns * rows)
        # Smooth loop: 0 -> 1 -> 0.
        pulse = 0.5 - 0.5 * math.cos(phase * math.tau)
        scale = 0.90 + pulse * 0.08
        brightness = 0.88 + pulse * 0.35
        alpha_scale = 0.76 + pulse * 0.24

        base = contain(source, frame_size, scale)
        base = ImageEnhance.Brightness(base).enhance(brightness)

        alpha = base.getchannel("A").point(lambda value: int(value * alpha_scale))
        base.putalpha(alpha)

        # A restrained cyan halo. It remains transparent and does not flood
        # the whole frame, which keeps it useful as an in-world prop.
        glow_alpha = alpha.filter(ImageFilter.GaussianBlur(radius=7 + int(pulse * 5)))
        glow = Image.new("RGBA", frame_size, (74, 214, 230, 0))
        glow.putalpha(glow_alpha.point(lambda value: int(value * (0.20 + pulse * 0.12))))
        frame_image = Image.alpha_composite(glow, base)

        x = (frame % columns) * frame_size[0]
        y = (frame // columns) * frame_size[1]
        sheet.alpha_composite(frame_image, (x, y))

    return sheet


def main() -> int:
    if not SOURCE.exists():
        raise SystemExit(f"Missing source decal sheet: {SOURCE}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = Image.open(SOURCE).convert("RGBA")
    built: dict[str, dict[str, object]] = {}

    for filename, crop_box in CROPS.items():
        image = trim(source.crop(crop_box))
        target = OUTPUT / filename
        image.save(target)
        built[filename] = {
            "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
            "crop": list(crop_box),
            "size": [image.width, image.height],
        }

    flame_source = Image.open(OUTPUT / "azure_flame_source.png").convert("RGBA")
    pulse = make_pulse_sheet(flame_source)
    pulse_path = OUTPUT / "azure_flame_4x4.png"
    pulse.save(pulse_path)
    built[pulse_path.name] = {
        "source": "Assets/environment/azure_flame_source.png",
        "columns": 4,
        "rows": 4,
        "frames": 16,
        "fps": 10.0,
        "size": [pulse.width, pulse.height],
    }

    (OUTPUT / "environment_manifest.json").write_text(
        json.dumps({"version": 1, "assets": built}, indent=2) + "\n",
        encoding="utf-8",
    )

    print(f"Built {len(built)} environment assets in {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
