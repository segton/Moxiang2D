#!/usr/bin/env python3
"""Rebuild level01 as a linear south-to-north authored dungeon.

The script is an optional editor-side authoring utility. The C++ game does
not execute Python at runtime. It writes a deterministic MOXIANG_LEVEL 13 map
that uses raised terrain as the chamber walls, directional ramps for every
height transition, terrain-aware floor decals, authored props and per-room
camera framing.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
LEVEL = ROOT / "levels" / "level01.mox"
ASSET_ROOT = ROOT / "Assets"

W = 64
H = 144
TILE = 64
ORIGIN_X = -W * TILE / 2.0
ORIGIN_Y = -H * TILE / 2.0
PIECE_ROWS = 12

NONE, NORTH, EAST, SOUTH, WEST = range(5)
DIR_OFFSET = {
    NORTH: (0, -1),
    EAST: (1, 0),
    SOUTH: (0, 1),
    WEST: (-1, 0),
}
STAIR_TILE = {NORTH: 44, EAST: 45, SOUTH: 46, WEST: 47}


@dataclass(frozen=True)
class Room:
    ident: int
    name: str
    cx: int
    cy: int
    rx: int
    ry: int
    cut: int
    base: int
    ambient: tuple[int, int, int, int]
    vignette: float
    camera_zoom: float
    focus: tuple[float, float] = (0.0, 0.0)


# Room IDs follow the player's progression. South is the level entrance and
# every chamber's north opening leads directly toward the next room.
ROOMS = [
    Room(0, "Gate of Returning Ash", 32, 135, 7, 6, 2, 0, (74, 59, 49, 255), 0.30, 1.62, (0, -20)),
    Room(1, "Ember Oath Hall", 32, 116, 10, 8, 3, 0, (79, 57, 43, 255), 0.25, 1.38, (0, -24)),
    Room(2, "Sunken Dragon Court", 32, 91, 14, 10, 4, 1, (58, 62, 70, 255), 0.20, 1.02, (0, -34)),
    Room(3, "Flooded Reliquary", 32, 66, 11, 9, 3, 1, (34, 65, 76, 255), 0.32, 1.22, (0, -28)),
    Room(4, "Azure Ritual Sanctum", 32, 42, 13, 9, 4, 2, (35, 59, 74, 255), 0.35, 1.08, (0, -34)),
    Room(5, "Throne of the Iron Warden", 32, 15, 18, 11, 5, 3, (64, 43, 39, 255), 0.40, 0.74, (0, -76)),
]

ROOM_BY_ID = {room.ident: room for room in ROOMS}

# Grid state.
enabled = [[0 for _ in range(W)] for _ in range(H)]
chamber = [[-1 for _ in range(W)] for _ in range(H)]
elevation = [[0 for _ in range(W)] for _ in range(H)]
ramp = [[NONE for _ in range(W)] for _ in range(H)]
tiles = [[0 for _ in range(W)] for _ in range(H)]


def inside(x: int, y: int) -> bool:
    return 0 <= x < W and 0 <= y < H


def octagon_contains(room: Room, x: int, y: int, inset: int = 0) -> bool:
    rx = room.rx - inset
    ry = room.ry - inset
    cut = max(0, room.cut - inset)
    if rx < 1 or ry < 1:
        return False
    dx = abs(x - room.cx)
    dy = abs(y - room.cy)
    return dx <= rx and dy <= ry and dx + dy <= rx + ry - cut


def paint_cell(x: int, y: int, cid: int, elev: int) -> None:
    if not inside(x, y):
        raise ValueError(f"cell outside map: {(x, y)}")
    enabled[y][x] = 1
    chamber[y][x] = cid
    elevation[y][x] = elev


def paint_rect(x0: int, y0: int, x1: int, y1: int, cid: int, elev: int) -> None:
    for y in range(max(0, y0), min(H, y1 + 1)):
        for x in range(max(0, x0), min(W, x1 + 1)):
            paint_cell(x, y, cid, elev)


def paint_room(room: Room, wall_thickness: int = 2) -> None:
    # The outer octagonal shell is raised terrain. Its inner cliff faces are
    # the room's walls, matching the user's existing terrain-wall workflow.
    for y in range(room.cy - room.ry, room.cy + room.ry + 1):
        for x in range(room.cx - room.rx, room.cx + room.rx + 1):
            if octagon_contains(room, x, y, 0):
                paint_cell(x, y, room.ident, room.base + 2)
            if octagon_contains(room, x, y, wall_thickness):
                paint_cell(x, y, room.ident, room.base)

    # Cut a five-cell south and north doorway through the raised shell.
    for x in range(room.cx - 2, room.cx + 3):
        for y in range(room.cy - room.ry, room.cy - room.ry + wall_thickness + 1):
            paint_cell(x, y, room.ident, room.base)
        for y in range(room.cy + room.ry - wall_thickness, room.cy + room.ry + 1):
            paint_cell(x, y, room.ident, room.base)


def place_ramp(cells: Iterable[tuple[int, int]], direction: int) -> None:
    dx, dy = DIR_OFFSET[direction]
    for x, y in cells:
        tx, ty = x + dx, y + dy
        if not inside(x, y) or not inside(tx, ty):
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


def connect_rooms(south_room: Room, north_room: Room) -> None:
    south_y = south_room.cy - south_room.ry
    north_y = north_room.cy + north_room.ry
    if north_y >= south_y:
        raise ValueError(f"rooms overlap in progression: {south_room.name} -> {north_room.name}")

    x0, x1 = south_room.cx - 2, south_room.cx + 2
    transition_y = (south_y + north_y) // 2

    if north_room.base < south_room.base or north_room.base > south_room.base + 1:
        raise ValueError("linear connector supports a flat link or one-level ascent")

    if north_room.base == south_room.base:
        for y in range(north_y, south_y + 1):
            cid = north_room.ident if y <= transition_y else south_room.ident
            paint_rect(x0, y, x1, y, cid, south_room.base)
        return

    # Ascending north: the stored ramp lives on the lower cell and points to
    # the immediately adjacent level+1 cell.
    ramp_y = transition_y + 1
    for y in range(ramp_y, south_y + 1):
        paint_rect(x0, y, x1, y, south_room.ident, south_room.base)
    for y in range(north_y, ramp_y - 1 + 1):
        paint_rect(x0, y, x1, y, north_room.ident, north_room.base)

    # Ensure the destination row is the higher level and the ramp row remains
    # owned by the room the player is leaving.
    paint_rect(x0, ramp_y - 1, x1, ramp_y - 1, north_room.ident, north_room.base)
    paint_rect(x0, ramp_y, x1, ramp_y, south_room.ident, south_room.base)
    place_ramp(((x, ramp_y) for x in range(x0, x1 + 1)), NORTH)


# ---------------------------------------------------------------------------
# Linear layout
# ---------------------------------------------------------------------------

for authored_room in ROOMS:
    paint_room(authored_room)

# South entrance path reaches the map boundary.
paint_rect(30, 141, 34, H - 1, 0, ROOMS[0].base)

for south, north in zip(ROOMS, ROOMS[1:]):
    connect_rooms(south, north)

# A final north threshold behind the boss room suggests the next floor.
paint_rect(30, 0, 34, ROOMS[-1].cy - ROOMS[-1].ry, ROOMS[-1].ident, ROOMS[-1].base)

# Side plinths add vertical depth but stay outside large central decals.
for x0, y0, x1, y1, cid in [
    (20, 88, 23, 94, 2),
    (41, 88, 44, 94, 2),
    (23, 63, 25, 68, 3),
    (39, 63, 41, 68, 3),
    (21, 39, 24, 45, 4),
    (40, 39, 43, 45, 4),
]:
    base = ROOM_BY_ID[cid].base
    paint_rect(x0, y0, x1, y1, cid, base + 1)

# Boss throne platform: explicit north-facing ascent from the flat arena.
paint_rect(28, 7, 36, 10, 5, ROOMS[5].base + 1)
paint_rect(30, 11, 34, 11, 5, ROOMS[5].base)
place_ramp(((x, 11) for x in range(30, 35)), NORTH)

# ---------------------------------------------------------------------------
# Floor materials
# ---------------------------------------------------------------------------

PALETTES = {
    0: [0, 1, 4, 8, 16, 17, 20, 24, 32, 33, 36],
    1: [0, 1, 4, 5, 8, 9, 16, 18, 20, 24, 25, 32, 36, 40],
    2: [0, 1, 2, 4, 5, 8, 9, 10, 16, 18, 20, 24, 25, 26, 32, 34, 36, 40],
    3: [2, 3, 5, 9, 10, 11, 18, 19, 21, 25, 26, 27, 34, 35, 37, 41],
    4: [1, 2, 5, 9, 10, 17, 18, 21, 25, 26, 33, 34, 37, 41, 42],
    5: [0, 2, 4, 8, 9, 11, 16, 18, 20, 24, 27, 32, 34, 40, 43],
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
        palette = PALETTES.get(cid, PALETTES[0])
        seed = x * 37 + y * 73 + cid * 101 + elevation[y][x] * 17
        choice = palette[seed % len(palette)]

        # Near cliffs and void edges favor the worn banks of the atlas.
        edge = any(
            not inside(x + dx, y + dy)
            or not enabled[y + dy][x + dx]
            or elevation[y + dy][x + dx] != elevation[y][x]
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
        )
        if edge and choice < 16:
            choice += 16 if seed % 3 else 32
        tiles[y][x] = min(choice, 43)

# ---------------------------------------------------------------------------
# Raised-terrain wall faces and exterior caps
# ---------------------------------------------------------------------------

wall_tiles = [[[-1, -1, -1, -1] for _ in range(W)] for _ in range(H)]
wall_heights = [[[0, 0, 0, 0] for _ in range(W)] for _ in range(H)]
built_pieces = [[[[ -1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(W)] for _ in range(H)]
cliff_pieces = [[[[ -1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(W)] for _ in range(H)]

WALL_PALETTE = {
    0: (49, 55, 61, 63),
    1: (49, 57, 61, 55),
    2: (55, 62, 61, 60),
    3: (54, 58, 62, 63),
    4: (54, 58, 62, 60),
    5: (57, 58, 63, 61),
}
FACE_DIRECTIONS = ((0, -1), (1, 0), (0, 1), (-1, 0))

for y in range(H):
    for x in range(W):
        if not enabled[y][x]:
            continue
        cid = chamber[y][x]
        primary, accent, base_material, damaged = WALL_PALETTE.get(cid, WALL_PALETTE[0])

        for face, (dx, dy) in enumerate(FACE_DIRECTIONS):
            seed = x * 19 + y * 31 + face * 7 + cid * 43
            material = accent if seed % 8 == 0 else primary
            if seed % 17 == 0:
                material = damaged
            wall_tiles[y][x][face] = material

            # Each elevation row gets a dedicated vertical wall texture. The
            # renderer only consumes the rows actually exposed by a cliff.
            for row in range(PIECE_ROWS):
                cliff_pieces[y][x][face][row] = 60 + ((seed + row) % 4)

            nx, ny = x + dx, y + dy
            exposed_to_void = not inside(nx, ny) or not enabled[ny][nx]
            if not exposed_to_void:
                continue

            # Raised terrain already provides the wall mass. These authored
            # exterior caps add detail without building a second solid box in
            # front of the player.
            if face == 0:
                height = 2
            elif face in (1, 3):
                height = 1 + (1 if seed % 11 == 0 else 0)
            else:
                height = 1 if seed % 19 == 0 else 0

            wall_heights[y][x][face] = height
            if height > 0:
                built_pieces[y][x][face][0] = base_material
            if height > 1:
                built_pieces[y][x][face][1] = material


# ---------------------------------------------------------------------------
# Environment objects
# ---------------------------------------------------------------------------

def world(cell_x: float, cell_y: float) -> tuple[float, float]:
    return (
        ORIGIN_X + (cell_x + 0.5) * TILE,
        ORIGIN_Y + (cell_y + 0.5) * TILE,
    )


def terrain_at(cell_x: float, cell_y: float) -> int:
    x = max(0, min(W - 1, int(cell_x)))
    y = max(0, min(H - 1, int(cell_y)))
    return elevation[y][x]


def asset(name: str) -> str:
    return f"Assets/environment/authored/{name}.png"


def generated_asset(name: str) -> str:
    return f"Assets/environment/generated/{name}.png"


OBSTACLES: list[dict[str, object]] = []


def add_object(
    name: str,
    x: float,
    y: float,
    w: float | None = None,
    h: float | None = None,
    *,
    path: str | None = None,
    layer: int = 1,
    mode: int = 0,
    tint: tuple[int, int, int, int] = (255, 255, 255, 255),
    opacity: float = 1.0,
    rotation: float = 0.0,
    collision: bool = False,
    shape: int = 1,
    radius: float = 28.0,
    collider: tuple[float, float] = (0.0, 0.0),
    blocks_light: bool = False,
    casts_shadow: bool = True,
    clip: bool = True,
    allow_ramps: bool = False,
    animated: bool = False,
    columns: int = 1,
    rows: int = 1,
    frames: int = 1,
    fps: float = 8.0,
    phase: float = 0.0,
    visual_offset: tuple[float, float] = (0.0, 0.0),
    collider_offset: tuple[float, float] = (0.0, 0.0),
    anchor_y: float = 1.0,
    depth_bias: float = 0.0,
) -> None:
    image_path = path if path is not None else asset(name)
    disk_path = ROOT / image_path
    if not disk_path.exists():
        raise FileNotFoundError(f"missing environment asset: {image_path}")

    # Preserve source aspect ratio when only one or neither dimension is set.
    if w is None or h is None:
        from PIL import Image
        with Image.open(disk_path) as image:
            source_w, source_h = image.size
        if w is None and h is None:
            w, h = float(source_w), float(source_h)
        elif w is None:
            w = float(h) * source_w / source_h
        else:
            h = float(w) * source_h / source_w

    wx, wy = world(x, y)
    OBSTACLES.append({
        "position": (wx, wy),
        "size": (float(w), float(h)),
        "type": 7,
        "height_level": 0,
        "casts_shadow": int(casts_shadow and mode == 0),
        "blocks_light": int(blocks_light and mode == 0),
        "layer": layer,
        "mode": mode,
        "tint": tint,
        "opacity": opacity,
        "animated": int(animated),
        "columns": columns,
        "rows": rows,
        "frames": frames,
        "fps": fps,
        "phase": phase,
        "source_frame": 0,
        "source_rect": (0.0, 0.0, 0.0, 0.0),
        "rotation": rotation,
        "terrain_elevation": terrain_at(x, y) if mode == 1 else -1,
        "clip": int(clip),
        "allow_ramps": int(allow_ramps),
        "path": image_path,
        "collision": int(collision and mode == 0),
        "auto": 0,
        "shape": shape,
        "collider": collider,
        "radius": radius,
        "visual_offset": visual_offset,
        "collider_offset": collider_offset,
        "anchor_y": max(0.0, min(1.0, anchor_y)),
        "depth_bias": depth_bias,
    })


def add_decal(name: str, x: float, y: float, w: float, h: float, **kwargs: object) -> None:
    add_object(name, x, y, w, h, mode=1, layer=0, casts_shadow=False, collision=False, **kwargs)


def add_prop(name: str, x: float, y: float, scale: float = 1.0, **kwargs: object) -> None:
    disk_path = ROOT / asset(name)
    from PIL import Image
    with Image.open(disk_path) as image:
        w = image.width * scale
        h = image.height * scale
    add_object(name, x, y, w, h, **kwargs)


def add_generated_prop(
    name: str,
    x: float,
    y: float,
    scale: float = 0.62,
    **kwargs: object,
) -> None:
    image_path = generated_asset(name)
    disk_path = ROOT / image_path
    from PIL import Image
    with Image.open(disk_path) as image:
        w = image.width * scale
        h = image.height * scale
    add_object(name, x, y, w, h, path=image_path, **kwargs)


# --- Ground decals and trims. Every large motif is fully contained on a flat
#     chamber floor. Terrain-aware clipping remains enabled as a safety net.
add_decal("decal_gold_seal_round", 32, 135, 260, 300, opacity=0.62)
add_decal("decal_dragon_half", 32, 116, 320, 410, opacity=0.56)
add_decal("decal_dragon_grand", 32, 91, 650, 680, opacity=0.68)
add_decal("decal_cyan_ring_quadrants", 32, 66, 400, 560, tint=(165, 235, 245, 255), opacity=0.65)
add_decal("decal_azure_seal_round", 32, 42, 480, 520, tint=(165, 231, 245, 255), opacity=0.72)
add_decal("decal_labyrinth_round", 32, 17, 650, 700, tint=(225, 199, 177, 255), opacity=0.56)

# Secondary motifs add irregularity without spanning terrain transitions.
add_decal("decal_broken_dragon", 24, 115, 250, 260, rotation=-8, opacity=0.42)
add_decal("decal_dragon_quadrants", 41, 92, 250, 250, rotation=12, opacity=0.42)
add_decal("decal_azure_seal_round", 25, 67, 210, 230, tint=(115, 219, 237, 255), opacity=0.46)
add_decal("decal_small_symbols", 32, 48, 760, 90, tint=(130, 215, 235, 255), opacity=0.52)
add_decal("decal_ring_row", 32, 84, 760, 100, opacity=0.36)
add_decal("decal_calligraphy_row", 32, 22, 780, 110, tint=(225, 182, 150, 255), opacity=0.43)

# Door thresholds and long platform trims. These are separate decals so Build
# Mode can move, resize, rotate or replace them independently.
thresholds = [
    ("threshold_steps_plain", 32, 142, 340, 80),
    ("threshold_emblem_gold", 32, 129, 350, 82),
    ("threshold_cracked_plain", 32, 124, 340, 76),
    ("threshold_cloud_carving", 32, 108, 350, 76),
    ("threshold_flame_carving", 32, 101, 360, 76),
    ("threshold_cracked_carving", 32, 81, 350, 76),
    ("threshold_dragon_gold", 32, 76, 390, 82),
    ("threshold_blue_seal", 32, 57, 350, 90),
    ("threshold_blue_runes", 32, 52, 350, 90),
    ("threshold_green_runes", 32, 33, 390, 86),
    ("threshold_cyan_runes", 32, 27, 350, 88),
    ("threshold_purple_seal", 32, 4, 350, 92),
]
for trim_name, x, y, w, h in thresholds:
    add_decal(trim_name, x, y, w, h, opacity=0.78)
add_decal("threshold_purple_runes", 24, 43, 350, 86, rotation=90, opacity=0.48)
add_decal("threshold_cracked_carving", 40, 43, 350, 76, rotation=90, opacity=0.48)
add_decal("trim_long_cloud_dark", 32, 119, 720, 78, opacity=0.38)
add_decal("trim_long_cloud_gold", 32, 95, 820, 80, opacity=0.44)
add_decal("trim_long_emblem_gold", 32, 12, 920, 86, opacity=0.42)

# --- Chamber gates. Flat front-facing art is used as wall-aligned billboards.
gate_positions = [
    ("gate_open_frame", 32, 142, 0.88),
    ("gate_empty_frame", 32, 129, 0.90),
    ("gate_quarter_open", 32, 108, 0.90),
    ("gate_half_open", 32, 81, 0.92),
    ("gate_three_quarter_open", 32, 57, 0.92),
    ("gate_magic_seal", 32, 33, 0.96),
    ("gate_boss_closed", 32, 27, 1.05),
    ("gate_boss_chained", 32, 4, 1.05),
]
for gate_name, x, y, scale in gate_positions:
    add_prop(gate_name, x, y, scale, layer=0, casts_shadow=False, blocks_light=False)
# A sealed side alcove uses the remaining closed gate without obstructing the route.
add_prop("gate_closed", 23, 116, 0.72, layer=0, casts_shadow=False)

# --- New perspective-correct generated prop sheets.
# These replace the earlier high-angle miniature-looking obstacle set. Every
# generated asset is used once, while the central south-to-north route remains
# clear for combat and chamber progression.

GENERATED_PROP_PLACEMENTS: list[tuple[str, float, float, float, bool, int, tuple[float, float], float]] = [
    # Structural obstacles.
    ("structural_short_square_pillar", 27, 138, .66, True, 1, (0, 0), 24),
    ("structural_damaged_square_pillar", 37, 138, .66, True, 1, (0, 0), 24),
    ("structural_short_round_column", 24, 119, .68, True, 1, (0, 2), 23),
    ("structural_broken_round_column", 40, 119, .68, True, 1, (0, 2), 23),
    ("structural_low_rect_pedestal", 22, 96, .70, True, 0, (62, 34), 0),
    ("structural_low_square_pedestal", 42, 96, .70, True, 0, (48, 38), 0),
    ("structural_dragon_support_block", 22, 86, .70, True, 0, (52, 42), 0),
    ("structural_boundary_post", 42, 86, .68, True, 1, (0, 4), 20),
    ("structural_collapsed_pillar_base", 20, 91, .72, False, 1, (0, 0), 20),
    ("structural_half_cracked_column", 44, 91, .70, True, 1, (0, 3), 24),
    ("structural_carved_cover_block", 18, 18, .74, True, 0, (72, 40), 0),
    ("structural_broken_support", 46, 18, .72, True, 1, (0, 3), 26),
    ("structural_low_wall_fragment", 19, 10, .72, True, 0, (78, 30), 0),
    ("structural_damaged_wall_fragment", 45, 10, .72, True, 0, (78, 30), 0),
    ("structural_guardian_lion_pedestal", 23, 7, .70, True, 1, (0, 5), 30),
    ("structural_brazier_pedestal", 41, 7, .70, True, 1, (0, 4), 27),

    # Ritual props.
    ("ritual_amber_brazier", 25, 121, .66, True, 1, (0, 3), 24),
    ("ritual_cyan_brazier", 39, 121, .66, True, 1, (0, 3), 24),
    ("ritual_extinguished_brazier", 24, 113, .64, True, 1, (0, 3), 23),
    ("ritual_incense_altar", 40, 113, .66, True, 0, (66, 32), 0),
    ("ritual_offering_table", 22, 116, .66, True, 0, (68, 34), 0),
    ("ritual_seal_pedestal", 42, 116, .66, True, 0, (52, 38), 0),
    ("ritual_rune_pedestal", 25, 70, .66, True, 0, (50, 38), 0),
    ("ritual_gong_stand", 39, 70, .68, True, 0, (68, 28), 0),
    ("ritual_chained_post", 23, 63, .66, True, 1, (0, 4), 22),
    ("ritual_incense_burner", 41, 63, .68, False, 1, (0, 2), 18),
    ("ritual_stone_basin", 24, 46, .68, True, 1, (0, 3), 26),
    ("ritual_cracked_platform", 40, 46, .70, True, 0, (74, 46), 0),
    ("ritual_candle_cluster", 22, 39, .68, False, 1, (0, 0), 14),
    ("ritual_sealed_relic", 42, 39, .68, True, 0, (50, 36), 0),
    ("ritual_damaged_shrine_base", 25, 35, .68, False, 0, (58, 36), 0),
    ("ritual_spirit_lantern", 39, 35, .66, True, 1, (0, 4), 22),

    # Storage props.
    ("storage_chest_plain_closed", 25, 134, .62, True, 0, (54, 28), 0),
    ("storage_chest_plain_open", 39, 134, .62, True, 0, (54, 30), 0),
    ("storage_chest_bronze_closed", 24, 120, .62, True, 0, (58, 30), 0),
    ("storage_chest_bronze_open", 40, 120, .62, True, 0, (58, 32), 0),
    ("storage_chest_ceremonial", 21, 99, .62, True, 0, (58, 30), 0),
    ("storage_chest_broken", 43, 99, .62, False, 0, (58, 30), 0),
    ("storage_large_urn", 24, 88, .62, True, 1, (0, 5), 22),
    ("storage_small_jar", 40, 88, .62, False, 1, (0, 4), 17),
    ("storage_cracked_urn", 22, 68, .62, True, 1, (0, 4), 21),
    ("storage_shattered_urn", 42, 68, .66, False, 1, (0, 0), 18),
    ("storage_crate", 22, 44, .62, True, 0, (52, 36), 0),
    ("storage_reinforced_crate", 42, 44, .62, True, 0, (54, 38), 0),
    ("storage_damaged_crate", 17, 22, .62, True, 0, (52, 36), 0),
    ("storage_collapsed_crate", 47, 22, .64, False, 0, (52, 34), 0),
    ("storage_scroll_bundle", 20, 14, .64, False, 1, (0, 0), 16),
    ("storage_supply_basket", 44, 14, .64, False, 1, (0, 0), 18),

    # Chamber ornaments.
    ("ornament_dragon_relief_monument", 24, 94, .68, True, 0, (70, 38), 0),
    ("ornament_circular_ritual_altar", 40, 94, .70, True, 0, (66, 48), 0),
    ("ornament_rectangular_ceremonial_altar", 25, 84, .68, True, 0, (72, 34), 0),
    ("ornament_guardian_statue", 39, 84, .68, True, 1, (0, 6), 27),
    ("ornament_sword_shrine", 26, 73, .66, True, 0, (64, 34), 0),
    ("ornament_ancient_bell", 38, 73, .68, True, 0, (66, 30), 0),
    ("ornament_rune_obelisk", 26, 60, .66, True, 1, (0, 5), 23),
    ("ornament_damaged_rune_obelisk", 38, 60, .66, True, 1, (0, 4), 22),
    ("ornament_chained_relic", 27, 49, .66, True, 0, (56, 38), 0),
    ("ornament_spiritual_fountain", 37, 49, .70, False, 1, (0, 0), 25),
    ("ornament_small_sarcophagus", 16, 16, .70, True, 0, (76, 36), 0),
    ("ornament_broken_monument", 48, 16, .70, True, 0, (72, 38), 0),
    ("ornament_incense_shrine", 22, 25, .66, True, 0, (62, 34), 0),
    ("ornament_prison_memorial", 42, 25, .68, True, 0, (64, 36), 0),
    ("ornament_boss_emblem_pedestal", 26, 12, .70, True, 0, (66, 42), 0),
    ("ornament_damaged_dragon_altar", 38, 12, .70, True, 0, (68, 40), 0),

    # Low debris and clutter.
    ("debris_loose_stones", 28, 132, .72, False, 1, (0, 0), 14),
    ("debris_rubble_pile", 36, 132, .72, False, 1, (0, 0), 18),
    ("debris_broken_floor_slab", 27, 118, .72, False, 0, (50, 28), 0),
    ("debris_wall_bricks", 37, 118, .72, False, 1, (0, 0), 18),
    ("debris_pillar_fragments", 20, 89, .72, False, 1, (0, 0), 20),
    ("debris_ceramic_fragments", 44, 89, .72, False, 1, (0, 0), 16),
    ("debris_wooden_boards", 25, 101, .72, False, 0, (48, 24), 0),
    ("debris_broken_crate", 39, 101, .72, False, 0, (48, 26), 0),
    ("debris_fallen_plaque", 27, 64, .72, False, 0, (48, 24), 0),
    ("debris_tablet_fragment", 37, 64, .72, False, 0, (48, 24), 0),
    ("debris_chain_coil", 27, 40, .72, False, 1, (0, 0), 16),
    ("debris_bones_rubble", 37, 40, .72, False, 1, (0, 0), 18),
    ("debris_torn_cloth_stones", 20, 20, .72, False, 1, (0, 0), 18),
    ("debris_weapon_fragments", 44, 20, .72, False, 1, (0, 0), 17),
    ("debris_ash_pile", 28, 24, .72, False, 1, (0, 0), 16),
    ("debris_moss_stones", 36, 24, .72, False, 1, (0, 0), 18),
]

for prop_name, x, y, scale, collide, shape, collider_size, radius in GENERATED_PROP_PLACEMENTS:
    add_generated_prop(
        prop_name,
        x,
        y,
        scale,
        collision=collide,
        shape=shape,
        collider=collider_size,
        radius=radius,
        blocks_light=prop_name.startswith("structural_") and "pillar" in prop_name,
        visual_offset=(0.0, 6.0 if y < 30 else 3.0),
        collider_offset=(0.0, 4.0 if collide else 0.0),
        anchor_y=1.0,
        depth_bias=1.0,
    )

# Keep shallow wall-aligned details from the original flat asset set. They are
# background decorations rather than freestanding obstacles.
for name, x, y, scale in [
    ("low_relief_wall", 27, 108, 0.82),
    ("wall_relief_short", 37, 108, 0.82),
    ("wall_block_short", 23, 81, 0.80),
    ("broken_wall_block", 41, 81, 0.80),
    ("war_banner", 25, 57, 0.78),
    ("war_banner", 39, 57, 0.78),
]:
    add_prop(name, x, y, scale, layer=0, collision=False, casts_shadow=False)

# Animated flames are kept separate from their stands so lights can flicker
# without requiring a different prop atlas.
FLAMES = [
    (28, 132, (255, 181, 104, 255), 0.0), (36, 132, (255, 181, 104, 255), 0.7),
    (24, 114, (255, 139, 72, 255), 1.1), (40, 114, (255, 139, 72, 255), 2.0),
    (22, 87, (255, 153, 81, 255), 0.5), (42, 87, (255, 153, 81, 255), 1.6),
    (25, 69, (89, 213, 236, 255), 2.4), (39, 69, (89, 213, 236, 255), 3.1),
    (25, 39, (89, 216, 241, 255), 1.9), (39, 39, (89, 216, 241, 255), 2.8),
    (20, 12, (255, 105, 66, 255), 0.9), (44, 12, (255, 105, 66, 255), 2.2),
]
for x, y, tint, phase in FLAMES:
    add_object(
        "", x, y, 96, 142,
        path="Assets/environment/azure_flame_4x4.png",
        layer=0,
        tint=tint,
        opacity=0.90,
        casts_shadow=False,
        animated=True,
        columns=4,
        rows=4,
        frames=16,
        fps=10.0,
        phase=phase,
    )

# Foreground framing creates Hades-like depth while remaining sparse enough
# not to obscure combat. These are explicitly foreground-layer billboards.
for name, x, y, scale, opacity in [
    ("foreground_pillar_left", 25, 124, 0.78, 0.86),
    ("foreground_pillar_right", 39, 124, 0.78, 0.86),
    ("foreground_arch_segment", 22, 101, 0.80, 0.62),
    ("foreground_roof_right", 42, 101, 0.82, 0.62),
    ("foreground_railing", 24, 75, 0.80, 0.58),
    ("foreground_chain_banner", 40, 75, 0.72, 0.58),
    ("foreground_corner_drape", 23, 51, 0.82, 0.54),
    ("foreground_hanging_beam", 41, 51, 0.74, 0.54),
    ("foreground_pillar_left", 19, 26, 0.92, 0.72),
    ("foreground_pillar_right", 45, 26, 0.92, 0.72),
    ("foreground_arch_center", 32, 27, 1.02, 0.66),
    ("foreground_broken_roof", 20, 4, 0.84, 0.48),
    ("foreground_draped_gate", 44, 4, 0.78, 0.48),
]:
    add_prop(name, x, y, scale, layer=2, opacity=opacity, casts_shadow=False, collision=False)


# ---------------------------------------------------------------------------
# Authored lights
# ---------------------------------------------------------------------------

LIGHTS: list[dict[str, object]] = []


def add_light(
    ident: int,
    name: str,
    cid: int,
    x: float,
    y: float,
    radius: float,
    intensity: float,
    color: tuple[int, int, int, int],
    phase: float,
    flicker: float = 0.10,
    speed: float = 6.0,
) -> None:
    wx, wy = world(x, y)
    LIGHTS.append({
        "id": ident,
        "name": name,
        "chamber": cid,
        "position": (wx, wy),
        "radius": radius,
        "intensity": intensity,
        "color": color,
        "enabled": 1,
        "follows": 0,
        "offset": (0.0, -22.0),
        "scale": (1.0, 1.0),
        "rotation": 0.0,
        "flicker": flicker,
        "speed": speed,
        "radius_flicker": 0.04,
        "phase": phase,
    })


for args in [
    (100, "Entrance West Lantern", 0, 28, 132, 330, .62, (255, 158, 82, 255), .1, .14),
    (101, "Entrance East Lantern", 0, 36, 132, 330, .62, (255, 158, 82, 255), 1.0, .14),
    (110, "Ember Hall Ambient", 1, 32, 116, 620, .39, (237, 182, 122, 255), 2.1, .04),
    (111, "Ember Hall West", 1, 24, 114, 310, .56, (255, 136, 68, 255), 1.4, .15),
    (112, "Ember Hall East", 1, 40, 114, 310, .56, (255, 136, 68, 255), 2.5, .15),
    (120, "Dragon Court Moonlight", 2, 32, 91, 900, .31, (188, 197, 207, 255), .8, .02),
    (121, "Dragon Court West", 2, 22, 87, 340, .50, (255, 143, 73, 255), 1.6, .12),
    (122, "Dragon Court East", 2, 42, 87, 340, .50, (255, 143, 73, 255), 2.4, .12),
    (130, "Reliquary Core", 3, 32, 66, 700, .60, (61, 191, 221, 255), .5, .05),
    (131, "Reliquary West", 3, 25, 69, 300, .48, (79, 214, 235, 255), 1.7, .10),
    (132, "Reliquary East", 3, 39, 69, 300, .48, (79, 214, 235, 255), 2.8, .10),
    (140, "Sanctum Core", 4, 32, 42, 760, .62, (67, 202, 233, 255), .3, .06),
    (141, "Sanctum West", 4, 25, 39, 310, .48, (91, 220, 242, 255), 1.8, .10),
    (142, "Sanctum East", 4, 39, 39, 310, .48, (91, 220, 242, 255), 2.9, .10),
    (150, "Warden Arena Ambient", 5, 32, 17, 1050, .39, (190, 121, 94, 255), 1.0, .03),
    (151, "Warden West Pyre", 5, 20, 12, 350, .58, (255, 94, 57, 255), 2.2, .16),
    (152, "Warden East Pyre", 5, 44, 12, 350, .58, (255, 94, 57, 255), 3.6, .16),
    (153, "Warden Throne", 5, 32, 8, 520, .48, (255, 145, 91, 255), .7, .08),
]:
    add_light(*args)


# ---------------------------------------------------------------------------
# Serialization
# ---------------------------------------------------------------------------

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


def obstacle_line(item: dict[str, object]) -> str:
    x, y = item["position"]
    w, h = item["size"]
    tint = item["tint"]
    sx, sy, sw, sh = item["source_rect"]
    cw, ch = item["collider"]
    path = str(item["path"]).replace('"', '')
    values = [
        x, y, w, h, item["type"], item["height_level"],
        item["casts_shadow"], item["blocks_light"], item["layer"], item["mode"],
        *tint, item["opacity"], item["animated"], item["columns"], item["rows"],
        item["frames"], item["fps"], item["phase"], item["source_frame"],
        sx, sy, sw, sh, item["rotation"], item["terrain_elevation"],
        item["clip"], item["allow_ramps"], f'"{path}"', item["collision"],
        item["auto"], item["shape"], cw, ch, item["radius"],
        *item["visual_offset"], *item["collider_offset"],
        item["anchor_y"], item["depth_bias"],
    ]
    return " ".join(str(value) for value in values)


def light_line(item: dict[str, object]) -> str:
    x, y = item["position"]
    r, g, b, a = item["color"]
    ox, oy = item["offset"]
    sx, sy = item["scale"]
    return (
        f'{item["id"]} "{item["name"]}" {item["chamber"]} '
        f'{x} {y} {item["radius"]} {item["intensity"]} '
        f'{r} {g} {b} {a} {item["enabled"]} {item["follows"]} '
        f'{ox} {oy} {sx} {sy} {item["rotation"]} {item["flicker"]} '
        f'{item["speed"]} {item["radius_flicker"]} {item["phase"]}'
    )


def is_traversable(ax: int, ay: int, bx: int, by: int) -> bool:
    if not inside(bx, by) or not enabled[by][bx]:
        return False
    if elevation[ay][ax] == elevation[by][bx]:
        return True
    direction = ramp[ay][ax]
    if direction in DIR_OFFSET:
        dx, dy = DIR_OFFSET[direction]
        if (ax + dx, ay + dy) == (bx, by) and elevation[by][bx] == elevation[ay][ax] + 1:
            return True
    direction = ramp[by][bx]
    if direction in DIR_OFFSET:
        dx, dy = DIR_OFFSET[direction]
        if (bx + dx, by + dy) == (ax, ay) and elevation[ay][ax] == elevation[by][bx] + 1:
            return True
    return False


def blocked_by_authored_obstacle(cell_x: int, cell_y: int) -> bool:
    """Approximate Game::IsCellBlocked for deterministic map validation."""
    center_x, center_y = world(cell_x, cell_y)
    player_radius = 20.0

    for item in OBSTACLES:
        if not item["collision"] or item["mode"] != 0 or item["height_level"] > 0:
            continue

        object_x, object_y = item["position"]
        if item["shape"] == 0:
            collider_w, collider_h = item["collider"]
            if collider_w <= 0.0 or collider_h <= 0.0:
                collider_w, collider_h = item["size"]
            if (
                abs(center_x - object_x) <= collider_w * 0.5 + player_radius and
                abs(center_y - object_y) <= collider_h * 0.5 + player_radius
            ):
                return True
        else:
            radius = float(item["radius"]) + player_radius + 4.0
            if (center_x - object_x) ** 2 + (center_y - object_y) ** 2 <= radius ** 2:
                return True

    return False


def validate_authored_layout() -> None:
    for y in range(H):
        for x in range(W):
            direction = ramp[y][x]
            if direction == NONE:
                continue
            dx, dy = DIR_OFFSET[direction]
            tx, ty = x + dx, y + dy
            if not inside(tx, ty) or elevation[ty][tx] != elevation[y][x] + 1:
                raise ValueError(f"invalid ramp at {(x, y)}")
            if tiles[y][x] != STAIR_TILE[direction]:
                raise ValueError(f"wrong stair tile at {(x, y)}")

    # Reachability starts at the authored southern entrance and must visit all
    # chamber floors, including the boss arena.
    start = (32, H - 1)
    queue = deque([start])
    visited = {start}
    while queue:
        x, y = queue.popleft()
        for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
            nx, ny = x + dx, y + dy
            if (nx, ny) not in visited and is_traversable(x, y, nx, ny):
                visited.add((nx, ny))
                queue.append((nx, ny))

    missing = {
        room.ident
        for room in ROOMS
        if not any(chamber[y][x] == room.ident and (x, y) in visited for y in range(H) for x in range(W))
    }
    if missing:
        raise ValueError(f"unreachable chambers: {sorted(missing)}")

    # Repeat the traversal with authored collision obstacles enabled. This
    # prevents a decorative prop cluster from accidentally sealing a gate,
    # a narrow connector or the only route through a combat chamber.
    if blocked_by_authored_obstacle(*start):
        raise ValueError("southern entrance is blocked by an authored obstacle")

    obstacle_queue = deque([start])
    obstacle_visited = {start}
    while obstacle_queue:
        x, y = obstacle_queue.popleft()
        for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
            nx, ny = x + dx, y + dy
            if (nx, ny) in obstacle_visited or not is_traversable(x, y, nx, ny):
                continue
            if blocked_by_authored_obstacle(nx, ny):
                continue
            obstacle_visited.add((nx, ny))
            obstacle_queue.append((nx, ny))

    obstacle_missing = {
        room.ident
        for room in ROOMS
        if not any(
            chamber[y][x] == room.ident and (x, y) in obstacle_visited
            for y in range(H)
            for x in range(W)
        )
    }
    if obstacle_missing:
        raise ValueError(
            "collision props block progression to chambers: "
            f"{sorted(obstacle_missing)}"
        )

    # The final northern threshold must also be reachable, not merely one
    # arbitrary cell somewhere inside the boss chamber.
    final_targets = {
        (x, 0)
        for x in range(30, 35)
        if enabled[0][x] and not blocked_by_authored_obstacle(x, 0)
    }
    if final_targets and not (final_targets & obstacle_visited):
        raise ValueError("boss chamber north exit is unreachable after prop placement")

    # Every clipped decal anchor must be on valid, non-ramp terrain.
    for item in OBSTACLES:
        if item["mode"] != 1:
            continue
        wx, wy = item["position"]
        cx = int((wx - ORIGIN_X) // TILE)
        cy = int((wy - ORIGIN_Y) // TILE)
        if not inside(cx, cy) or not enabled[cy][cx]:
            raise ValueError(f"decal anchor outside terrain: {item['path']}")
        if ramp[cy][cx] != NONE and not item["allow_ramps"]:
            raise ValueError(f"decal anchor is a ramp: {item['path']}")


def build_lines() -> list[str]:
    lines = ["MOXIANG_LEVEL 13"]
    lines += grid_section("TILES", tiles)
    lines += grid_section("TERRAIN_ELEVATIONS", elevation, " 64")
    lines += grid_section("TERRAIN_RAMPS", ramp)
    lines += wall_section("WALL_TILES", wall_tiles)
    lines += wall_section("WALL_HEIGHTS", wall_heights)
    lines += piece_section("BUILT_WALL_PIECES", built_pieces)
    lines += piece_section("CLIFF_WALL_PIECES", cliff_pieces)
    lines += [f"CHAMBERS {len(ROOMS)}"]
    lines += [f'{room.ident} "{room.name}"' for room in ROOMS]
    lines += [f"CHAMBER_STYLES {len(ROOMS)}"]
    lines += [
        f"{room.ident} {room.ambient[0]} {room.ambient[1]} {room.ambient[2]} {room.ambient[3]} "
        f"{room.vignette} {room.camera_zoom} {room.focus[0]} {room.focus[1]}"
        for room in ROOMS
    ]

    cell_rows: list[list[int]] = []
    for y in range(H):
        values: list[int] = []
        for x in range(W):
            values.extend((enabled[y][x], chamber[y][x]))
        cell_rows.append(values)
    lines += grid_section("CELL_LAYOUT", cell_rows)
    lines += ['TERRAIN_ATLAS "Assets/tiles/dungeon_master_atlas.png" 8 8']
    lines += ["TILE_WALKABILITY 64"] + [f"{index} 1" for index in range(64)]
    lines += ["TILE_BRUSHES 64"] + [f'{index} "" 1 90 90 94 255' for index in range(64)]
    lines += [f"OBSTACLES {len(OBSTACLES)}"] + [obstacle_line(item) for item in OBSTACLES]
    lines += [f"LIGHTS {len(LIGHTS)}"] + [light_line(item) for item in LIGHTS]
    lines += ["NPCS 0"]
    return lines


def main() -> int:
    validate_authored_layout()
    LEVEL.write_text("\n".join(build_lines()) + "\n", encoding="utf-8")
    ramp_count = sum(1 for row in ramp for direction in row if direction != NONE)
    decal_count = sum(1 for item in OBSTACLES if item["mode"] == 1)
    print(
        f"Rebuilt {LEVEL} as {len(ROOMS)} linear chambers: "
        f"{ramp_count} ramps, {len(OBSTACLES)} objects ({decal_count} decals), "
        f"{len(LIGHTS)} lights"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
