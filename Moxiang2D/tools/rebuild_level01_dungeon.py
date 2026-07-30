#!/usr/bin/env python3
"""Author the production-style level01 dungeon and write it to level01.mox.

This is an optional offline authoring utility. The game executable does not
run Python and does not require Python to load or play the generated level.
"""
from __future__ import annotations

import re
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEVEL = ROOT / "levels" / "level01.mox"
W = H = 60
TILE = 64
ORIGIN = -W * TILE / 2
PIECE_ROWS = 12

# RampDirection values from Game.h.
NONE, NORTH, EAST, SOUTH, WEST = range(5)
DIR_OFFSET = {
    NORTH: (0, -1),
    EAST: (1, 0),
    SOUTH: (0, 1),
    WEST: (-1, 0),
}
STAIR_TILE = {
    NORTH: 44,
    EAST: 45,
    SOUTH: 46,
    WEST: 47,
}

CHAMBERS = [
    (0, "Gate of Returning Ash", (70, 57, 48, 255), 0.28),
    (1, "Hall of the Broken Seal", (68, 65, 66, 255), 0.23),
    (2, "Sunken Dragon Court", (62, 67, 75, 255), 0.18),
    (3, "Flooded Reliquary", (35, 66, 76, 255), 0.31),
    (4, "Ember Archive", (82, 57, 39, 255), 0.27),
    (5, "Moonlit Prison Walk", (45, 55, 68, 255), 0.33),
    (6, "Azure Ritual Sanctum", (32, 62, 75, 255), 0.35),
    (7, "Throne of the Iron Warden", (67, 44, 40, 255), 0.38),
]

# Grid state.
enabled = [[0 for _ in range(W)] for _ in range(H)]
chamber = [[-1 for _ in range(W)] for _ in range(H)]
elevation = [[0 for _ in range(W)] for _ in range(H)]
ramp = [[NONE for _ in range(W)] for _ in range(H)]
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
            if dx <= rx and dy <= ry and dx + dy <= rx + ry - cut and 0 <= x < W and 0 <= y < H:
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


def place_ramp(cells: list[tuple[int, int]], direction: int) -> None:
    dx, dy = DIR_OFFSET[direction]
    for x, y in cells:
        tx, ty = x + dx, y + dy
        if not (0 <= x < W and 0 <= y < H and 0 <= tx < W and 0 <= ty < H):
            raise ValueError(f"ramp outside map at {(x, y)}")
        if not enabled[y][x] or not enabled[ty][tx]:
            raise ValueError(f"ramp touches disabled cell at {(x, y)} -> {(tx, ty)}")
        if elevation[ty][tx] != elevation[y][x] + 1:
            raise ValueError(
                f"invalid ramp height at {(x, y)} -> {(tx, ty)}: "
                f"{elevation[y][x]} to {elevation[ty][tx]}"
            )
        ramp[y][x] = direction
        tiles[y][x] = STAIR_TILE[direction]


# ---------------------------------------------------------------------------
# Chamber layout
# ---------------------------------------------------------------------------

# 1. Hall and main rooms are painted first. Later thresholds deliberately
# establish exact chamber ownership at every doorway.
paint_octagon(30, 41, 10, 6, 3, 1, 0)
paint_rect(19, 39, 22, 43, 1, 0)
paint_rect(38, 39, 41, 43, 1, 0)
paint_rect(26, 34, 34, 37, 1, 0)

paint_octagon(30, 25, 13, 8, 4, 2, 1)
paint_rect(26, 31, 34, 33, 2, 1)
paint_rect(17, 23, 20, 28, 2, 1)
paint_rect(40, 23, 43, 28, 2, 1)
paint_rect(27, 16, 33, 19, 2, 1)
paint_rect(18, 17, 22, 20, 2, 1)

paint_octagon(9, 25, 7, 8, 3, 3, 0)
paint_rect(14, 23, 16, 28, 3, 0)
paint_rect(4, 31, 13, 34, 3, 0)

paint_octagon(51, 26, 7, 8, 3, 4, 2)
paint_rect(44, 23, 47, 28, 4, 2)
paint_rect(48, 15, 53, 20, 4, 2)
paint_rect(51, 33, 57, 36, 4, 2)

paint_rect(6, 8, 17, 19, 5, 2)
paint_rect(4, 11, 19, 16, 5, 2)
paint_rect(15, 17, 17, 20, 5, 2)

paint_circle(30, 8, 8, 6, 6, 2)
paint_rect(27, 13, 33, 15, 6, 2)

paint_octagon(50, 8, 8, 6, 3, 7, 3)
paint_rect(48, 13, 53, 14, 7, 3)

# South entrance is last so it owns the transition into chamber 1.
paint_rect(27, 52, 32, 58, 0, 0)
paint_rect(24, 48, 35, 53, 0, 0)
paint_rect(28, 46, 31, 48, 0, 0)

# Raised room features, each with its own explicit ramp strip.
paint_rect(28, 23, 32, 27, 2, 2)  # Dragon Court dais
paint_rect(49, 22, 53, 25, 4, 3)  # Archive reading dais
paint_rect(28, 6, 32, 10, 6, 3)   # Ritual core
paint_rect(48, 5, 52, 8, 7, 4)    # Boss throne dais

# Correct lower-cell -> adjacent level+1 ramp placement.
place_ramp([(x, 34) for x in range(28, 33)], NORTH)       # Hall -> court
place_ramp([(16, y) for y in range(24, 28)], EAST)        # Reliquary -> court
place_ramp([(43, y) for y in range(24, 28)], EAST)        # Court -> archive
place_ramp([(18, y) for y in range(17, 20)], WEST)        # Court -> prison
place_ramp([(x, 16) for x in range(28, 33)], NORTH)       # Court -> sanctum
place_ramp([(x, 15) for x in range(49, 53)], NORTH)       # Archive -> boss
place_ramp([(x, 28) for x in range(29, 32)], NORTH)       # Court dais
place_ramp([(x, 26) for x in range(50, 53)], NORTH)       # Archive dais
place_ramp([(x, 11) for x in range(29, 32)], NORTH)       # Ritual core
place_ramp([(x, 9) for x in range(49, 52)], NORTH)        # Boss dais

# ---------------------------------------------------------------------------
# Floor materials: 0..43 are floor variants; 44..47 are directional stairs;
# 48..63 are dedicated wall materials in dungeon_master_atlas.png.
# ---------------------------------------------------------------------------

PALETTES = {
    0: [0, 1, 4, 8, 16, 17, 20, 24, 32, 33, 36],
    1: [0, 1, 2, 4, 5, 8, 9, 16, 18, 20, 24, 25, 32, 34],
    2: [0, 1, 2, 4, 5, 8, 9, 10, 16, 18, 20, 24, 25, 26, 32, 34, 36],
    3: [2, 3, 5, 9, 10, 11, 18, 19, 21, 25, 26, 27, 34, 35, 37, 41],
    4: [0, 4, 8, 9, 11, 16, 20, 24, 27, 32, 36, 40, 43],
    5: [1, 2, 5, 8, 9, 10, 17, 18, 21, 24, 26, 33, 34, 37],
    6: [1, 2, 5, 9, 10, 17, 18, 21, 25, 26, 33, 34, 37, 41],
    7: [0, 2, 4, 8, 9, 11, 16, 18, 20, 24, 27, 32, 34, 40, 43],
}

for y in range(H):
    for x in range(W):
        if not enabled[y][x]:
            tiles[y][x] = 0
            continue
        if ramp[y][x] != NONE:
            tiles[y][x] = STAIR_TILE[ramp[y][x]]
            continue
        cid = chamber[y][x]
        palette = PALETTES[cid]
        edge = any(
            not (0 <= x + dx < W and 0 <= y + dy < H) or not enabled[y + dy][x + dx]
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
        )
        seed = x * 37 + y * 73 + cid * 101
        choice = palette[seed % len(palette)]
        # Edge cells favor worn/damp variants from the second and third banks.
        if edge and choice < 16:
            choice += 16 if seed % 3 else 32
        tiles[y][x] = min(choice, 43)

# ---------------------------------------------------------------------------
# Wall materials. Dedicated wall tiles eliminate the stretched floor-on-wall
# appearance from the previous version. South/front edges remain mostly open
# so the camera is not hidden behind a continuous near wall.
# ---------------------------------------------------------------------------

wall_tiles = [[[-1, -1, -1, -1] for _ in range(W)] for _ in range(H)]
wall_heights = [[[0, 0, 0, 0] for _ in range(W)] for _ in range(H)]
built_pieces = [[[[ -1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(W)] for _ in range(H)]
cliff_pieces = [[[[ -1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(W)] for _ in range(H)]

WALL_PALETTE = {
    0: (49, 55, 61),
    1: (55, 61, 63),
    2: (55, 62, 61),
    3: (62, 63, 60),
    4: (49, 57, 61),
    5: (61, 62, 63),
    6: (54, 58, 62),
    7: (57, 58, 63),
}

for y in range(H):
    for x in range(W):
        if not enabled[y][x]:
            continue
        cid = chamber[y][x]
        primary, accent, base = WALL_PALETTE[cid]
        for face, (dx, dy) in enumerate(((0, -1), (1, 0), (0, 1), (-1, 0))):
            seed = x * 19 + y * 31 + face * 7 + cid * 43
            wall_tiles[y][x][face] = accent if seed % 9 == 0 else primary
            # Cliff rows use rugged dedicated wall materials.
            for row in range(PIECE_ROWS):
                cliff_pieces[y][x][face][row] = 60 + ((seed + row) % 4)

            nx, ny = x + dx, y + dy
            exposed = not (0 <= nx < W and 0 <= ny < H) or not enabled[ny][nx]
            if not exposed:
                continue

            if face == 0:  # back wall
                height = 2
            elif face in (1, 3):  # side walls
                height = 1 + (1 if seed % 13 == 0 else 0)
            else:  # near/front edge: isolated low segments only
                height = 1 if seed % 11 == 0 else 0

            wall_heights[y][x][face] = height
            if height > 0:
                built_pieces[y][x][face][0] = base
            if height > 1:
                built_pieces[y][x][face][1] = wall_tiles[y][x][face]


def world(cell_x: float, cell_y: float) -> tuple[float, float]:
    return ORIGIN + (cell_x + 0.5) * TILE, ORIGIN + (cell_y + 0.5) * TILE


def obstacle(
    x: float,
    y: float,
    w: int,
    h: int,
    path: str,
    *,
    layer: int = 1,
    mode: int = 0,
    tint: tuple[int, int, int, int] = (255, 255, 255, 255),
    opacity: float = 1.0,
    height_level: int = 0,
    casts_shadow: int = 1,
    blocks_light: int = 0,
    animated: int = 0,
    columns: int = 1,
    rows: int = 1,
    frames: int = 1,
    fps: float = 8.0,
    phase: float = 0.0,
    collision: int = 0,
    auto: int = 0,
    shape: int = 0,
    collider_w: float = 0.0,
    collider_h: float = 0.0,
    radius: float = 0.0,
) -> tuple[object, ...]:
    wx, wy = world(x, y)
    return (
        wx, wy, w, h, 7, height_level, casts_shadow, blocks_light,
        layer, mode, *tint, opacity, animated, columns, rows, frames, fps,
        phase, path, collision, auto, shape, collider_w, collider_h, radius,
    )


OBSTACLES: list[tuple[object, ...]] = []

# Large floor motifs anchor the major arenas without consuming collision space.
for data in [
    (30, 25, 650, 650, "Assets/environment/stone_medallion.png", (220, 211, 190, 255), 0.64),
    (30, 8, 500, 500, "Assets/environment/ritual_square.png", (145, 221, 238, 255), 0.72),
    (50, 8, 560, 560, "Assets/environment/stone_medallion.png", (240, 174, 142, 255), 0.54),
]:
    x, y, w, h, path, tint, opacity = data
    OBSTACLES.append(obstacle(x, y, w, h, path, mode=1, casts_shadow=0, tint=tint, opacity=opacity))

# Puddles and floor damage add non-repeating visual texture.
for x, y, sx, sy in [
    (7, 23, 390, 260), (11, 28, 460, 310), (7, 32, 350, 240),
]:
    OBSTACLES.append(obstacle(x, y, sx, sy, "Assets/environment/water_puddle.png", mode=1, casts_shadow=0, tint=(145, 230, 238, 255), opacity=0.66))
for x, y, size, rot_tint in [
    (25, 42, 330, (210, 205, 192, 255)), (36, 40, 280, (210, 205, 192, 255)),
    (22, 27, 300, (190, 205, 211, 255)), (39, 21, 260, (190, 205, 211, 255)),
    (11, 13, 260, (170, 194, 202, 255)), (54, 30, 250, (225, 189, 158, 255)),
]:
    OBSTACLES.append(obstacle(x, y, size, size, "Assets/environment/floor_cracks.png", mode=1, casts_shadow=0, tint=rot_tint, opacity=0.55))

# Architectural pillars frame entrances and room boundaries.
for x, y in [(24, 48), (35, 48), (21, 38), (39, 38), (18, 22), (42, 22), (23, 17), (37, 17), (45, 22), (57, 22), (6, 10), (17, 10), (43, 4), (57, 4)]:
    OBSTACLES.append(obstacle(x, y, 128, 242, "Assets/environment/stone_pillar.png", blocks_light=1, collision=1, shape=1, radius=30))
for x, y in [(22, 32), (39, 31), (5, 30), (15, 19), (56, 34)]:
    OBSTACLES.append(obstacle(x, y, 120, 205, "Assets/environment/broken_pillar.png", collision=1, shape=1, radius=28))

# Treasure, urns and rubble live mostly near walls, preserving combat centers.
for x, y in [(20, 41), (40, 41), (5, 33), (55, 35), (16, 11), (54, 12)]:
    OBSTACLES.append(obstacle(x, y, 112, 96, "Assets/environment/treasure_chest.png", tint=(235, 210, 170, 255), collision=1, collider_w=74, collider_h=44))
for x, y in [(25, 49), (34, 49), (22, 40), (38, 40), (5, 14), (18, 14), (46, 25), (56, 25), (24, 9), (36, 9)]:
    OBSTACLES.append(obstacle(x, y, 72, 108, "Assets/environment/stone_urn.png", collision=1, shape=1, radius=20))
for x, y, collision in [(23, 45, 0), (37, 45, 0), (20, 29, 1), (40, 29, 1), (6, 19, 0), (14, 31, 0), (47, 32, 0), (56, 19, 0), (45, 11, 0)]:
    OBSTACLES.append(obstacle(x, y, 118, 78, "Assets/environment/rubble_pile.png", collision=collision, shape=1, radius=28 if collision else 0))

# Animated flames correspond to authored lights below.
FLAMES = [
    (25, 49, (255, 195, 126, 255), 0.0), (34, 49, (255, 195, 126, 255), 0.7),
    (22, 37, (255, 158, 84, 255), 1.1), (38, 37, (255, 158, 84, 255), 2.0),
    (15, 22, (106, 221, 238, 255), 0.5), (5, 22, (106, 221, 238, 255), 1.6),
    (45, 22, (255, 132, 72, 255), 2.4), (57, 22, (255, 132, 72, 255), 3.1),
    (27, 13, (104, 219, 241, 255), 1.9), (33, 13, (104, 219, 241, 255), 2.8),
    (44, 12, (255, 103, 68, 255), 0.9), (56, 12, (255, 103, 68, 255), 2.2),
]
for x, y, tint, phase in FLAMES:
    OBSTACLES.append(obstacle(x, y, 104, 150, "Assets/environment/azure_flame_4x4.png", layer=0, tint=tint, opacity=0.9, casts_shadow=0, animated=1, columns=4, rows=4, frames=16, fps=10.0, phase=phase))

# Foreground framing is sparse and chamber-specific rather than a full wall.
OBSTACLES.append(obstacle(30, 56, 620, 300, "Assets/environment/foreground_gate.png", layer=2, tint=(210, 198, 178, 255), opacity=0.88, casts_shadow=0))
OBSTACLES.append(obstacle(50, 14, 600, 300, "Assets/environment/foreground_gate.png", layer=2, tint=(180, 160, 150, 255), opacity=0.48, casts_shadow=0))


def light(ident: int, name: str, cid: int, x: float, y: float, radius: int,
          intensity: float, color: tuple[int, int, int, int], phase: float,
          flicker: float = 0.10) -> tuple[object, ...]:
    wx, wy = world(x, y)
    return (ident, name, cid, wx, wy, radius, intensity, color, 1, 0,
            (0, -22), (1, 1), 0, flicker, 6.0, 0.04, phase)


LIGHTS = [
    light(100, "Gate Brazier West", 0, 25, 49, 310, .62, (255, 157, 78, 255), .1, .15),
    light(101, "Gate Brazier East", 0, 34, 49, 310, .62, (255, 157, 78, 255), 1.0, .15),
    light(110, "Broken Seal Hall", 1, 30, 41, 560, .42, (238, 189, 126, 255), 2.1, .05),
    light(111, "Hall West Brazier", 1, 22, 37, 300, .52, (255, 143, 72, 255), 1.4, .14),
    light(112, "Hall East Brazier", 1, 38, 37, 300, .52, (255, 143, 72, 255), 2.5, .14),
    light(120, "Dragon Court Core", 2, 30, 25, 760, .32, (202, 202, 190, 255), .8, .02),
    light(121, "Dragon Court Dais", 2, 30, 25, 350, .38, (125, 194, 218, 255), 1.6, .05),
    light(130, "Reliquary Pool", 3, 9, 25, 650, .60, (68, 194, 220, 255), .5, .05),
    light(131, "Reliquary West", 3, 5, 22, 270, .48, (80, 211, 229, 255), 1.7, .10),
    light(132, "Reliquary East", 3, 15, 22, 270, .48, (80, 211, 229, 255), 2.8, .10),
    light(140, "Ember Archive", 4, 51, 26, 610, .56, (255, 133, 64, 255), 2.0, .11),
    light(141, "Archive West", 4, 45, 22, 290, .54, (255, 123, 59, 255), 2.7, .15),
    light(142, "Archive East", 4, 57, 22, 290, .54, (255, 123, 59, 255), 3.4, .15),
    light(150, "Prison Moonlight", 5, 11, 13, 610, .50, (90, 172, 207, 255), 1.4, .04),
    light(160, "Ritual Sanctum Core", 6, 30, 8, 650, .66, (65, 207, 235, 255), .3, .06),
    light(161, "Sanctum West", 6, 27, 13, 280, .48, (95, 218, 240, 255), 1.8, .10),
    light(162, "Sanctum East", 6, 33, 13, 280, .48, (95, 218, 240, 255), 2.9, .10),
    light(170, "Iron Warden Throne", 7, 50, 8, 720, .52, (255, 92, 62, 255), 1.0, .07),
    light(171, "Warden West", 7, 44, 12, 290, .56, (255, 111, 57, 255), 2.2, .16),
    light(172, "Warden East", 7, 56, 12, 290, .56, (255, 111, 57, 255), 3.6, .16),
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
    return [f"{tag} {W} {H}{extra}"] + [" ".join(map(str, row)) for row in rows]


def wall_section(tag: str, data: list[list[list[int]]]) -> list[str]:
    output = [f"{tag} {W} {H}"]
    for row in data:
        values: list[str] = []
        for cell in row:
            values.extend(map(str, cell))
        output.append(" ".join(values))
    return output


def piece_section(tag: str, data: list[list[list[list[int]]]]) -> list[str]:
    output = [f"{tag} {W} {H} {PIECE_ROWS}"]
    for row in data:
        values: list[str] = []
        for cell in row:
            for face in cell:
                values.extend(map(str, face))
        output.append(" ".join(values))
    return output


def obstacle_line(values: tuple[object, ...]) -> str:
    items = list(values)
    items[21] = f'"{items[21]}"'
    return " ".join(str(value) for value in items)


def light_line(values: tuple[object, ...]) -> str:
    ident, name, cid, x, y, radius, intensity, color, enabled_l, follows, offset, scale, rotation, flicker, speed, radius_flicker, phase = values
    return (f'{ident} "{name}" {cid} {x} {y} {radius} {intensity} '
            f'{color[0]} {color[1]} {color[2]} {color[3]} {enabled_l} {follows} '
            f'{offset[0]} {offset[1]} {scale[0]} {scale[1]} {rotation} '
            f'{flicker} {speed} {radius_flicker} {phase}')


def validate_authored_layout() -> None:
    # Every stored ramp must be valid and use the dedicated directional tile.
    for y in range(H):
        for x in range(W):
            direction = ramp[y][x]
            if direction == NONE:
                continue
            dx, dy = DIR_OFFSET[direction]
            tx, ty = x + dx, y + dy
            if elevation[ty][tx] != elevation[y][x] + 1:
                raise ValueError(f"invalid ramp at {(x, y)}")
            if tiles[y][x] != STAIR_TILE[direction]:
                raise ValueError(f"wrong stair tile at {(x, y)}")

    # Reachability mirrors the game's terrain edge rule, ignoring encounter
    # gates. This catches accidental disconnected rooms before shipping.
    start = (29, 56)
    queue = deque([start])
    visited = {start}
    while queue:
        x, y = queue.popleft()
        for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < W and 0 <= ny < H and enabled[ny][nx]):
                continue
            traversable = elevation[ny][nx] == elevation[y][x]
            if not traversable:
                if ramp[y][x] in DIR_OFFSET and (x + DIR_OFFSET[ramp[y][x]][0], y + DIR_OFFSET[ramp[y][x]][1]) == (nx, ny):
                    traversable = elevation[ny][nx] == elevation[y][x] + 1
                if ramp[ny][nx] in DIR_OFFSET and (nx + DIR_OFFSET[ramp[ny][nx]][0], ny + DIR_OFFSET[ramp[ny][nx]][1]) == (x, y):
                    traversable = elevation[y][x] == elevation[ny][nx] + 1
            if traversable and (nx, ny) not in visited:
                visited.add((nx, ny))
                queue.append((nx, ny))

    missing_chambers = {
        cid for cid, *_ in CHAMBERS
        if not any((x, y) in visited and chamber[y][x] == cid for y in range(H) for x in range(W))
    }
    if missing_chambers:
        raise ValueError(f"unreachable chambers: {sorted(missing_chambers)}")


def main() -> int:
    validate_authored_layout()
    lines = LEVEL.read_text(encoding="utf-8").splitlines()
    lines[0] = "MOXIANG_LEVEL 11"
    lines = replace(lines, "TILES", grid_section("TILES", tiles))
    lines = replace(lines, "TERRAIN_ELEVATIONS", grid_section("TERRAIN_ELEVATIONS", elevation, " 64"))
    lines = replace(lines, "TERRAIN_RAMPS", grid_section("TERRAIN_RAMPS", ramp))
    lines = replace(lines, "WALL_TILES", wall_section("WALL_TILES", wall_tiles))
    lines = replace(lines, "WALL_HEIGHTS", wall_section("WALL_HEIGHTS", wall_heights))
    lines = replace(lines, "BUILT_WALL_PIECES", piece_section("BUILT_WALL_PIECES", built_pieces))
    lines = replace(lines, "CLIFF_WALL_PIECES", piece_section("CLIFF_WALL_PIECES", cliff_pieces))
    lines = replace(lines, "CHAMBERS", [f"CHAMBERS {len(CHAMBERS)}"] + [f'{cid} "{name}"' for cid, name, _, _ in CHAMBERS])
    lines = replace(lines, "CHAMBER_STYLES", [f"CHAMBER_STYLES {len(CHAMBERS)}"] + [f"{cid} {color[0]} {color[1]} {color[2]} {color[3]} {vignette}" for cid, _, color, vignette in CHAMBERS])
    cell_rows: list[list[int]] = []
    for y in range(H):
        values: list[int] = []
        for x in range(W):
            values.extend((enabled[y][x], chamber[y][x]))
        cell_rows.append(values)
    lines = replace(lines, "CELL_LAYOUT", grid_section("CELL_LAYOUT", cell_rows))
    lines = replace(lines, "TERRAIN_ATLAS", ['TERRAIN_ATLAS "Assets/tiles/dungeon_master_atlas.png" 8 8'])
    lines = replace(lines, "TILE_WALKABILITY", ["TILE_WALKABILITY 64"] + [f"{index} 1" for index in range(64)])
    lines = replace(lines, "TILE_BRUSHES", ["TILE_BRUSHES 64"] + [f'{index} "" 1 90 90 94 255' for index in range(64)])
    lines = replace(lines, "OBSTACLES", [f"OBSTACLES {len(OBSTACLES)}"] + [obstacle_line(item) for item in OBSTACLES])
    lines = replace(lines, "LIGHTS", [f"LIGHTS {len(LIGHTS)}"] + [light_line(item) for item in LIGHTS])
    LEVEL.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(
        f"Rebuilt {LEVEL} with {len(CHAMBERS)} chambers, "
        f"{sum(1 for row in ramp for direction in row if direction != NONE)} ramps, "
        f"{len(OBSTACLES)} environment objects and {len(LIGHTS)} lights"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
