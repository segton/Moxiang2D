#!/usr/bin/env python3
"""Rebuild level01 as a deliberate multi-chamber wuxia dungeon.

The original level was a mechanics test. This script replaces its terrain,
chamber ownership, walls, elevations, ramps, atmosphere objects and lights with
a deterministic authored layout while preserving the existing hybrid camera.
"""
from __future__ import annotations

import re
import shlex
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEVEL = ROOT / "levels" / "level01.mox"
W = H = 60
TILE = 64
ORIGIN = -W * TILE / 2
ROWS = 12

CHAMBERS = [
    (0, "Gate of Returning Ash", (72, 60, 52, 255), 0.30),
    (1, "Hall of the Broken Seal", (70, 70, 76, 255), 0.22),
    (2, "Sunken Dragon Court", (68, 70, 78, 255), 0.18),
    (3, "Flooded Reliquary", (42, 70, 80, 255), 0.30),
    (4, "Ember Archive", (86, 62, 44, 255), 0.27),
    (5, "Moonlit Prison Walk", (50, 61, 72, 255), 0.32),
    (6, "Azure Ritual Sanctum", (40, 66, 78, 255), 0.34),
    (7, "Throne of the Iron Warden", (72, 52, 48, 255), 0.36),
]

# grid data
enabled = [[0 for _ in range(W)] for _ in range(H)]
chamber = [[-1 for _ in range(W)] for _ in range(H)]
elevation = [[0 for _ in range(W)] for _ in range(H)]
ramp = [[0 for _ in range(W)] for _ in range(H)]
tiles = [[0 for _ in range(W)] for _ in range(H)]


def paint_rect(x0: int, y0: int, x1: int, y1: int, cid: int, elev: int) -> None:
    for y in range(max(0, y0), min(H, y1 + 1)):
        for x in range(max(0, x0), min(W, x1 + 1)):
            enabled[y][x] = 1
            chamber[y][x] = cid
            elevation[y][x] = elev


def paint_octagon(cx: int, cy: int, rx: int, ry: int, cut: int, cid: int, elev: int) -> None:
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            dx = abs(x - cx)
            dy = abs(y - cy)
            if dx <= rx and dy <= ry and dx + dy <= rx + ry - cut:
                if 0 <= x < W and 0 <= y < H:
                    enabled[y][x] = 1
                    chamber[y][x] = cid
                    elevation[y][x] = elev


def paint_circle(cx: int, cy: int, rx: int, ry: int, cid: int, elev: int) -> None:
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            if ((x - cx) / max(1, rx)) ** 2 + ((y - cy) / max(1, ry)) ** 2 <= 1.0:
                if 0 <= x < W and 0 <= y < H:
                    enabled[y][x] = 1
                    chamber[y][x] = cid
                    elevation[y][x] = elev


def set_ramp(x: int, y: int, direction: int) -> None:
    ramp[y][x] = direction


# South entrance: narrow, compressed and heavily framed.
paint_rect(27, 50, 32, 58, 0, 0)
paint_rect(24, 47, 35, 52, 0, 0)
paint_rect(28, 44, 31, 47, 0, 0)

# Transitional hall before the main arena.
paint_octagon(30, 40, 8, 6, 3, 1, 1)
paint_rect(27, 43, 32, 46, 1, 0)
for x in range(27, 33):
    set_ramp(x, 43, 1)  # north, into elevation 1

# Main arena: broad combat space with clipped corners and side balconies.
paint_octagon(30, 30, 12, 9, 5, 2, 1)
paint_rect(19, 27, 41, 33, 2, 1)
paint_rect(26, 21, 34, 23, 2, 1)
paint_rect(27, 36, 33, 39, 2, 1)

# West flooded reliquary, lower than the arena.
paint_octagon(11, 31, 7, 8, 4, 3, 0)
paint_rect(16, 29, 20, 33, 3, 0)
paint_rect(19, 29, 20, 33, 2, 0)
for y in range(29, 34):
    set_ramp(20, y, 2)  # east, into arena elevation 1

# East archive raised above the central court.
paint_octagon(49, 32, 7, 8, 4, 4, 2)
paint_rect(40, 29, 44, 34, 4, 1)
for y in range(29, 35):
    set_ramp(43, y, 2)  # east, into elevation 2

# Northwest prison walk: irregular room and narrow bridge.
paint_rect(7, 13, 17, 23, 5, 2)
paint_rect(5, 16, 19, 20, 5, 2)
paint_rect(17, 18, 24, 21, 5, 1)
for y in range(18, 22):
    set_ramp(23, y, 4)  # west, into elevation 2

# North ritual sanctum: circular ritual chamber at elevation 3.
paint_circle(30, 13, 9, 7, 6, 3)
paint_rect(27, 18, 33, 23, 6, 2)
for x in range(27, 34):
    set_ramp(x, 22, 1)  # north, into elevation 3

# Northeast boss chamber: highest and visually dominant.
paint_octagon(49, 14, 8, 8, 4, 7, 4)
paint_rect(38, 13, 42, 17, 7, 3)
for y in range(13, 18):
    set_ramp(41, y, 2)  # east, into elevation 4

# Small architectural alcoves / silhouette breaks.
paint_rect(22, 26, 24, 28, 2, 1)
paint_rect(36, 25, 38, 27, 2, 1)
paint_rect(8, 37, 12, 39, 3, 0)
paint_rect(47, 40, 52, 42, 4, 2)

# Deterministic floor variation: central paths calmer, room edges rougher.
for y in range(H):
    for x in range(W):
        if not enabled[y][x]:
            tiles[y][x] = 0
            continue
        cid = chamber[y][x]
        edge = any(
            not (0 <= x + dx < W and 0 <= y + dy < H) or not enabled[y + dy][x + dx]
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
        )
        noise = (x * 17 + y * 31 + cid * 13) % 11
        if edge:
            tiles[y][x] = 8 + (noise % 4)
        elif cid in (3, 6):
            tiles[y][x] = 4 + (noise % 4)
        elif cid in (4, 7):
            tiles[y][x] = 12 + (noise % 4)
        else:
            tiles[y][x] = noise % 4

# Wall data. Exposed room edges receive real wall faces; no billboard murals.
wall_tiles = [[[ -1, -1, -1, -1 ] for _ in range(W)] for _ in range(H)]
wall_heights = [[[ 0, 0, 0, 0 ] for _ in range(W)] for _ in range(H)]
for y in range(H):
    for x in range(W):
        if not enabled[y][x]:
            continue
        for face, (dx, dy) in enumerate(((0, -1), (1, 0), (0, 1), (-1, 0))):
            nx, ny = x + dx, y + dy
            exposed = not (0 <= nx < W and 0 <= ny < H) or not enabled[ny][nx]
            if exposed:
                wall_tiles[y][x][face] = 9 + ((x + y + face) % 3)
                # Taller back/side walls, slightly lower near-camera walls.
                wall_heights[y][x][face] = 3 if face in (0, 1, 3) else 2


def world(x: float, y: float) -> tuple[float, float]:
    return ORIGIN + x * TILE, ORIGIN + y * TILE


def obstacle(x: float, y: float, w: int, h: int, layer: int, mode: int,
             tint: tuple[int, int, int, int], opacity: float, animated: int,
             cols: int, rows: int, frames: int, fps: float, phase: float,
             path: str) -> tuple[object, ...]:
    wx, wy = world(x, y)
    return (wx, wy, w, h, 7, 0, 0, 0, layer, mode,
            *tint, opacity, animated, cols, rows, frames, fps, phase, path,
            0, 0, 0, 1, 1, 1)

# Only floor-integrated motifs and animated flames. The former dragon-panel and
# hanging-scroll billboards are intentionally removed because they did not
# conform to wall perspective.
OBSTACLES = [
    obstacle(30, 30, 690, 690, 1, 1, (238, 224, 196, 255), 0.82, 0, 1, 1, 1, 8, 0, "Assets/environment/stone_medallion.png"),
    obstacle(30, 13, 510, 510, 1, 1, (176, 231, 245, 255), 0.84, 0, 1, 1, 1, 8, 0, "Assets/environment/ritual_square.png"),
    obstacle(49, 14, 590, 590, 1, 1, (255, 202, 174, 255), 0.72, 0, 1, 1, 1, 8, 0, "Assets/environment/stone_medallion.png"),
    obstacle(11, 31, 430, 430, 1, 1, (157, 224, 236, 255), 0.62, 0, 1, 1, 1, 8, 0, "Assets/environment/ritual_square.png"),
]

# Animated flame pairs at entrances and room anchors.
for i, (x, y, tint, phase) in enumerate([
    (25, 49, (255, 204, 144, 255), 0.0), (34, 49, (255, 204, 144, 255), 0.7),
    (22, 36, (255, 174, 96, 255), 1.1), (38, 36, (255, 174, 96, 255), 2.0),
    (27, 19, (166, 238, 255, 255), 0.5), (33, 19, (166, 238, 255, 255), 1.6),
    (43, 20, (255, 142, 86, 255), 2.4), (55, 20, (255, 142, 86, 255), 3.1),
    (7, 16, (126, 220, 238, 255), 1.9), (17, 16, (126, 220, 238, 255), 2.8),
]):
    OBSTACLES.append(obstacle(x, y, 128, 184, 0, 0, tint, 0.88, 1, 4, 4, 16, 10, phase, "Assets/environment/azure_flame_4x4.png"))


def light(ident: int, name: str, cid: int, x: float, y: float, radius: int,
          intensity: float, color: tuple[int, int, int, int], phase: float,
          flicker: float = 0.12) -> tuple[object, ...]:
    wx, wy = world(x, y)
    return (ident, name, cid, wx, wy, radius, intensity, color, 1, 0,
            (0, -24), (1, 1), 0, flicker, 6.2, 0.045, phase)

LIGHTS = [
    light(100, "Gate Brazier West", 0, 25, 49, 290, .58, (255, 157, 76, 255), .1, .16),
    light(101, "Gate Brazier East", 0, 34, 49, 290, .58, (255, 157, 76, 255), 1.0, .16),
    light(110, "Broken Seal Hall", 1, 30, 40, 470, .42, (255, 190, 116, 255), 2.1, .07),
    light(120, "Dragon Court Core", 2, 30, 30, 680, .30, (212, 198, 172, 255), .8, .03),
    light(121, "Dragon Court West", 2, 22, 36, 310, .52, (255, 142, 68, 255), 1.6, .15),
    light(122, "Dragon Court East", 2, 38, 36, 310, .52, (255, 142, 68, 255), 2.7, .15),
    light(130, "Flooded Reliquary", 3, 11, 31, 560, .56, (79, 202, 224, 255), .5, .05),
    light(140, "Ember Archive", 4, 49, 32, 520, .58, (255, 139, 65, 255), 2.0, .13),
    light(150, "Prison Walk West", 5, 7, 16, 290, .48, (98, 191, 215, 255), 1.4, .10),
    light(151, "Prison Walk East", 5, 17, 16, 290, .48, (98, 191, 215, 255), 2.4, .10),
    light(160, "Ritual Sanctum Core", 6, 30, 13, 620, .62, (75, 213, 238, 255), .3, .06),
    light(161, "Sanctum Threshold West", 6, 27, 19, 260, .46, (107, 220, 241, 255), 1.8, .10),
    light(162, "Sanctum Threshold East", 6, 33, 19, 260, .46, (107, 220, 241, 255), 2.9, .10),
    light(170, "Iron Warden Throne", 7, 49, 14, 700, .48, (255, 102, 70, 255), 1.0, .08),
    light(171, "Warden Gate West", 7, 43, 20, 280, .54, (255, 127, 63, 255), 2.2, .16),
    light(172, "Warden Gate East", 7, 55, 20, 280, .54, (255, 127, 63, 255), 3.6, .16),
]


def is_tag(line: str) -> bool:
    return bool(re.match(r"^[A-Z][A-Z_]+(?:\s|$)", line))


def replace(lines: list[str], tag: str, replacement: list[str]) -> list[str]:
    start = next(i for i, line in enumerate(lines) if line.startswith(tag + " "))
    end = start + 1
    while end < len(lines) and not is_tag(lines[end]):
        end += 1
    return lines[:start] + replacement + lines[end:]


def grid_section(tag: str, rows: list[list[int]], extra: str = "") -> list[str]:
    header = f"{tag} {W} {H}{extra}"
    return [header] + [" ".join(map(str, row)) for row in rows]


def wall_section(tag: str, data: list[list[list[int]]]) -> list[str]:
    out = [f"{tag} {W} {H}"]
    for row in data:
        values: list[str] = []
        for cell in row:
            values.extend(map(str, cell))
        out.append(" ".join(values))
    return out


def piece_section(tag: str) -> list[str]:
    # 4 faces x 12 individually paintable rows per cell.
    record = " ".join(["-1"] * (4 * ROWS))
    return [f"{tag} {W} {H} {ROWS}"] + [" ".join([record] * W) for _ in range(H)]


def obstacle_line(values: tuple[object, ...]) -> str:
    values = list(values)
    values[21] = f'"{values[21]}"'
    return " ".join(str(v) for v in values)


def light_line(values: tuple[object, ...]) -> str:
    ident, name, cid, x, y, radius, intensity, color, enabled_l, follows, offset, scale, rotation, flicker, speed, radius_flicker, phase = values
    return (f'{ident} "{name}" {cid} {x} {y} {radius} {intensity} '
            f'{color[0]} {color[1]} {color[2]} {color[3]} {enabled_l} {follows} '
            f'{offset[0]} {offset[1]} {scale[0]} {scale[1]} {rotation} '
            f'{flicker} {speed} {radius_flicker} {phase}')


def main() -> int:
    lines = LEVEL.read_text(encoding="utf-8").splitlines()
    lines[0] = "MOXIANG_LEVEL 11"
    lines = replace(lines, "TILES", grid_section("TILES", tiles))
    lines = replace(lines, "TERRAIN_ELEVATIONS", grid_section("TERRAIN_ELEVATIONS", elevation, " 64"))
    lines = replace(lines, "TERRAIN_RAMPS", grid_section("TERRAIN_RAMPS", ramp))
    lines = replace(lines, "WALL_TILES", wall_section("WALL_TILES", wall_tiles))
    lines = replace(lines, "WALL_HEIGHTS", wall_section("WALL_HEIGHTS", wall_heights))
    lines = replace(lines, "BUILT_WALL_PIECES", piece_section("BUILT_WALL_PIECES"))
    lines = replace(lines, "CLIFF_WALL_PIECES", piece_section("CLIFF_WALL_PIECES"))
    lines = replace(lines, "CHAMBERS", [f"CHAMBERS {len(CHAMBERS)}"] + [f'{cid} "{name}"' for cid, name, _, _ in CHAMBERS])
    lines = replace(lines, "CHAMBER_STYLES", [f"CHAMBER_STYLES {len(CHAMBERS)}"] + [f"{cid} {c[0]} {c[1]} {c[2]} {c[3]} {v}" for cid, _, c, v in CHAMBERS])
    cell_rows = []
    for y in range(H):
        values: list[int] = []
        for x in range(W):
            values.extend((enabled[y][x], chamber[y][x]))
        cell_rows.append(values)
    lines = replace(lines, "CELL_LAYOUT", grid_section("CELL_LAYOUT", cell_rows))
    lines = replace(lines, "OBSTACLES", [f"OBSTACLES {len(OBSTACLES)}"] + [obstacle_line(o) for o in OBSTACLES])
    lines = replace(lines, "LIGHTS", [f"LIGHTS {len(LIGHTS)}"] + [light_line(l) for l in LIGHTS])
    LEVEL.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Rebuilt {LEVEL} with {len(CHAMBERS)} chambers, {len(OBSTACLES)} environment objects and {len(LIGHTS)} lights")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
