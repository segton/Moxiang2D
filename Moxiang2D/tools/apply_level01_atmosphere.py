#!/usr/bin/env python3
"""Apply the first authored atmosphere pass to level01.mox.

The script is intentionally deterministic so the sample level can be rebuilt
while the editor continues to evolve.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEVEL = ROOT / "levels" / "level01.mox"

CHAMBERS = [
    (0, "Sunken Antechamber", (72, 75, 88, 255), 0.22),
    (1, "Azure Reliquary", (52, 76, 88, 255), 0.28),
    (2, "Ember Archive", (82, 66, 52, 255), 0.24),
]

# v11 obstacle tuple:
# x y w h type height shadow block layer mode tintRGBA opacity animated
# columns rows frames fps phase path collision colliderAuto shape colliderW colliderH radius
OBSTACLES = [
    (-416, -480, 520, 520, 7, 0, 0, 0, 1, 1, 255, 255, 255, 255, 0.90, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/stone_medallion.png", 0, 0, 0, 1, 1, 1),
    (288, -1120, 420, 420, 7, 0, 0, 0, 1, 1, 214, 244, 255, 255, 0.88, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/ritual_square.png", 0, 0, 0, 1, 1, 1),
    (1120, -864, 360, 360, 7, 0, 0, 0, 1, 1, 255, 224, 190, 255, 0.86, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/stone_medallion.png", 0, 0, 0, 1, 1, 1),
    (-480, -992, 520, 240, 7, 0, 0, 0, 0, 0, 255, 240, 220, 255, 0.90, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/dragon_panel.png", 0, 0, 0, 1, 1, 1),
    (32, -1248, 160, 224, 7, 0, 0, 0, 0, 0, 205, 250, 255, 255, 0.82, 1, 4, 4, 16, 10.0, 0.0, "Assets/environment/azure_flame_4x4.png", 0, 0, 0, 1, 1, 1),
    (544, -1248, 160, 224, 7, 0, 0, 0, 0, 0, 205, 250, 255, 255, 0.82, 1, 4, 4, 16, 10.0, 0.73, "Assets/environment/azure_flame_4x4.png", 0, 0, 0, 1, 1, 1),
    (928, -1056, 320, 148, 7, 0, 0, 0, 0, 0, 255, 218, 174, 255, 0.82, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/dragon_panel.png", 0, 0, 0, 1, 1, 1),
    (1312, -544, 240, 360, 7, 0, 0, 0, 2, 0, 255, 238, 210, 255, 0.66, 0, 1, 1, 1, 8.0, 0.0, "Assets/environment/hanging_scroll.png", 0, 0, 0, 1, 1, 1),
]

# id, name, chamber, x, y, radius, intensity, color, enabled, follows,
# view offset, scale, rotation, flicker amount/speed/radius/phase
LIGHTS = [
    (10, "Antechamber Brazier North", 0, -480, -992, 300, 0.60, (255, 154, 74, 255), 1, 0, (0, -28), (1, 1), 0, 0.14, 6.8, 0.055, 0.2),
    (11, "Antechamber Hearth", 0, -416, -480, 460, 0.46, (255, 190, 112, 255), 1, 0, (0, -18), (1, 1), 0, 0.08, 5.2, 0.035, 1.7),
    (12, "Antechamber Lower Left", 0, -608, -96, 250, 0.48, (255, 139, 62, 255), 1, 0, (0, -22), (1, 1), 0, 0.16, 7.5, 0.065, 2.4),
    (13, "Antechamber Lower Right", 0, -288, -96, 250, 0.48, (255, 139, 62, 255), 1, 0, (0, -22), (1, 1), 0, 0.16, 7.2, 0.065, 4.1),
    (20, "Reliquary Core", 1, 288, -1120, 500, 0.54, (86, 210, 232, 255), 1, 0, (0, -16), (1, 1), 0, 0.05, 3.6, 0.025, 0.8),
    (21, "Reliquary Flame West", 1, 32, -1248, 210, 0.56, (84, 226, 244, 255), 1, 0, (0, -48), (1, 1), 0, 0.10, 5.9, 0.04, 1.1),
    (22, "Reliquary Flame East", 1, 544, -1248, 210, 0.56, (84, 226, 244, 255), 1, 0, (0, -48), (1, 1), 0, 0.10, 6.3, 0.04, 3.0),
    (30, "Archive Hearth", 2, 1120, -864, 430, 0.55, (255, 172, 82, 255), 1, 0, (0, -20), (1, 1), 0, 0.12, 6.6, 0.05, 0.5),
    (31, "Archive Sconce West", 2, 928, -1056, 230, 0.50, (255, 128, 52, 255), 1, 0, (0, -28), (1, 1), 0, 0.17, 7.8, 0.07, 2.0),
    (32, "Archive Sconce East", 2, 1248, -1056, 230, 0.50, (255, 128, 52, 255), 1, 0, (0, -28), (1, 1), 0, 0.17, 7.4, 0.07, 4.6),
]


def replace_section(lines: list[str], tag: str, replacement: list[str]) -> list[str]:
    start = next((i for i, line in enumerate(lines) if line.startswith(tag + " ")), None)
    if start is None:
        raise RuntimeError(f"Missing section: {tag}")
    end = start + 1
    while end < len(lines) and not re.match(r"^[A-Z][A-Z_]+(?:\s|$)", lines[end]):
        end += 1
    return lines[:start] + replacement + lines[end:]


def insert_before(lines: list[str], tag: str, replacement: list[str]) -> list[str]:
    start = next((i for i, line in enumerate(lines) if line.startswith(tag + " ")), None)
    if start is None:
        raise RuntimeError(f"Missing section: {tag}")
    return lines[:start] + replacement + lines[start:]


def obstacle_line(item: tuple[object, ...]) -> str:
    values = list(item)
    path = values[21]
    values[21] = f'"{path}"'
    return " ".join(str(value) for value in values)


def light_line(item: tuple[object, ...]) -> str:
    (ident, name, chamber, x, y, radius, intensity, color, enabled, follows,
     offset, scale, rotation, flicker, speed, radius_flicker, phase) = item
    return (
        f'{ident} "{name}" {chamber} {x} {y} {radius} {intensity} '
        f'{color[0]} {color[1]} {color[2]} {color[3]} {enabled} {follows} '
        f'{offset[0]} {offset[1]} {scale[0]} {scale[1]} {rotation} '
        f'{flicker} {speed} {radius_flicker} {phase}'
    )


def main() -> int:
    lines = LEVEL.read_text(encoding="utf-8").splitlines()
    lines[0] = "MOXIANG_LEVEL 11"

    chamber_lines = [f"CHAMBERS {len(CHAMBERS)}"] + [
        f'{ident} "{name}"' for ident, name, _, _ in CHAMBERS
    ]
    lines = replace_section(lines, "CHAMBERS", chamber_lines)

    # Remove a previous style block if this script is run repeatedly.
    if any(line.startswith("CHAMBER_STYLES ") for line in lines):
        lines = replace_section(lines, "CHAMBER_STYLES", [])

    chamber_end = next(i for i, line in enumerate(lines) if line.startswith("CELL_LAYOUT "))
    style_lines = [f"CHAMBER_STYLES {len(CHAMBERS)}"]
    for ident, _, color, vignette in CHAMBERS:
        style_lines.append(
            f"{ident} {color[0]} {color[1]} {color[2]} {color[3]} {vignette}"
        )
    lines = lines[:chamber_end] + style_lines + lines[chamber_end:]

    obstacle_lines = [f"OBSTACLES {len(OBSTACLES)}"] + [obstacle_line(item) for item in OBSTACLES]
    lines = replace_section(lines, "OBSTACLES", obstacle_lines)

    if any(line.startswith("LIGHTS ") for line in lines):
        lines = replace_section(lines, "LIGHTS", [])
    light_lines = [f"LIGHTS {len(LIGHTS)}"] + [light_line(item) for item in LIGHTS]
    lines = insert_before(lines, "NPCS", light_lines)

    LEVEL.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Updated {LEVEL} with {len(OBSTACLES)} environment objects and {len(LIGHTS)} lights")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
