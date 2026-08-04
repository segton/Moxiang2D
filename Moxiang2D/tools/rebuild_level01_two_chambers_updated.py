#!/usr/bin/env python3
"""Build a fast two-chamber Moxiang2D level using the accepted decoration atlas.

The generated file is compatible with MOXIANG_LEVEL 14 and the current
Hybrid3D obstacle format, including billboardOrientation.

Run from the Moxiang2D project directory:
    python tools/rebuild_level01_two_chambers.py

Or run this downloaded copy directly:
    python rebuild_level01_two_chambers.py --root D:/Dev/Moxiang2D/Moxiang2D
"""

from __future__ import annotations

import argparse
import shutil
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

TILE = 64
PIECE_ROWS = 12
NONE, NORTH, EAST, SOUTH, WEST = range(5)
FACE_DIRECTIONS = ((0, -1), (1, 0), (0, 1), (-1, 0))

# HybridBillboardOrientation values from Game.h.
FACE_CAMERA = 0
UPRIGHT_WORLD = 1
WORLD_PLANE_X = 2
WORLD_PLANE_Z = 3

# ObstacleRenderLayer values.
BACKGROUND = 0
WORLD_LAYER = 1
FOREGROUND = 2

# ObstacleRenderMode values.
BILLBOARD = 0
GROUND_DECAL = 1


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
    focus: tuple[float, float]


class LevelBuilder:
    def __init__(self, root: Path, output: Path) -> None:
        self.root = root
        self.output = output
        self.asset_root = root / "Assets"

        self.w = 56
        self.h = 76
        self.origin_x = -self.w * TILE / 2.0
        self.origin_y = -self.h * TILE / 2.0

        self.rooms = [
            Room(
                0,
                "Hall of Returning Embers",
                28,
                57,
                13,
                12,
                4,
                0,
                (100, 88, 82, 255),
                0.12,
                1.10,
                (0.0, -30.0),
            ),
            Room(
                1,
                "Azure Warden Arena",
                28,
                20,
                17,
                15,
                5,
                0,
                (78, 88, 108, 255),
                0.16,
                0.86,
                (0.0, -54.0),
            ),
        ]

        self.enabled = [[0 for _ in range(self.w)] for _ in range(self.h)]
        self.chamber = [[-1 for _ in range(self.w)] for _ in range(self.h)]
        self.elevation = [[0 for _ in range(self.w)] for _ in range(self.h)]
        self.ramp = [[NONE for _ in range(self.w)] for _ in range(self.h)]
        self.tiles = [[0 for _ in range(self.w)] for _ in range(self.h)]

        self.obstacles: list[dict[str, object]] = []
        self.lights: list[dict[str, object]] = []
        self.missing_assets: set[str] = set()

    def inside(self, x: int, y: int) -> bool:
        return 0 <= x < self.w and 0 <= y < self.h

    @staticmethod
    def octagon_contains(room: Room, x: int, y: int, inset: int = 0) -> bool:
        rx = room.rx - inset
        ry = room.ry - inset
        cut = max(0, room.cut - inset)
        if rx < 1 or ry < 1:
            return False
        dx = abs(x - room.cx)
        dy = abs(y - room.cy)
        return dx <= rx and dy <= ry and dx + dy <= rx + ry - cut

    def paint_cell(self, x: int, y: int, cid: int, elev: int) -> None:
        if not self.inside(x, y):
            raise ValueError(f"cell outside map: {(x, y)}")
        self.enabled[y][x] = 1
        self.chamber[y][x] = cid
        self.elevation[y][x] = elev

    def paint_rect(self, x0: int, y0: int, x1: int, y1: int, cid: int, elev: int) -> None:
        for y in range(max(0, y0), min(self.h, y1 + 1)):
            for x in range(max(0, x0), min(self.w, x1 + 1)):
                self.paint_cell(x, y, cid, elev)

    def paint_room(self, room: Room, wall_thickness: int = 2) -> None:
        # Raised terrain creates a real wall shell around a flat combat floor.
        for y in range(room.cy - room.ry, room.cy + room.ry + 1):
            for x in range(room.cx - room.rx, room.cx + room.rx + 1):
                if self.octagon_contains(room, x, y, 0):
                    self.paint_cell(x, y, room.ident, room.base + 2)
                if self.octagon_contains(room, x, y, wall_thickness):
                    self.paint_cell(x, y, room.ident, room.base)

        # Five-cell north/south openings preserve fast enemy and player flow.
        for x in range(room.cx - 2, room.cx + 3):
            for y in range(room.cy - room.ry, room.cy - room.ry + wall_thickness + 1):
                self.paint_cell(x, y, room.ident, room.base)
            for y in range(room.cy + room.ry - wall_thickness, room.cy + room.ry + 1):
                self.paint_cell(x, y, room.ident, room.base)

    def build_layout(self) -> None:
        for room in self.rooms:
            self.paint_room(room)

        south = self.rooms[0]
        north = self.rooms[1]

        # Short starting runway.
        self.paint_rect(26, south.cy + south.ry - 1, 30, self.h - 1, 0, south.base)

        # Ten-cell connector, deliberately short for an ad-game pace.
        connector_south = south.cy - south.ry
        connector_north = north.cy + north.ry
        transition = (connector_south + connector_north) // 2
        for y in range(connector_north, connector_south + 1):
            cid = 1 if y <= transition else 0
            self.paint_rect(26, y, 30, y, cid, 0)

        # Brief northern victory threshold behind the boss arena.
        self.paint_rect(26, 0, 30, north.cy - north.ry + 1, 1, north.base)

        # Small raised side stages add silhouette without obstructing combat.
        for x0, y0, x1, y1, cid in [
            (16, 53, 19, 60, 0),
            (37, 53, 40, 60, 0),
            (13, 15, 17, 23, 1),
            (39, 15, 43, 23, 1),
        ]:
            self.paint_rect(x0, y0, x1, y1, cid, self.rooms[cid].base + 1)

        # Raised boss throne stage at the north end of chamber 1.
        self.paint_rect(24, 7, 32, 10, 1, 1)
        self.paint_rect(26, 11, 30, 11, 1, 0)
        self.paint_rect(26, 6, 30, 6, 1, 0)
        for x in range(26, 31):
            self.ramp[11][x] = NORTH
            self.ramp[6][x] = SOUTH

    def assign_floor_tiles(self) -> None:
        palettes = {
            0: [0, 1, 4, 8, 16, 17, 20, 24, 32, 33, 36, 40],
            1: [0, 2, 5, 9, 10, 18, 21, 25, 26, 34, 37, 41, 42, 43],
        }
        stair_tile = {NORTH: 44, EAST: 45, SOUTH: 46, WEST: 47}

        for y in range(self.h):
            for x in range(self.w):
                if not self.enabled[y][x]:
                    self.tiles[y][x] = 0
                    continue
                if self.ramp[y][x] != NONE:
                    self.tiles[y][x] = stair_tile[self.ramp[y][x]]
                    continue
                cid = self.chamber[y][x]
                palette = palettes.get(cid, palettes[0])
                seed = x * 37 + y * 73 + cid * 101 + self.elevation[y][x] * 17
                choice = palette[seed % len(palette)]
                edge = any(
                    not self.inside(x + dx, y + dy)
                    or not self.enabled[y + dy][x + dx]
                    or self.elevation[y + dy][x + dx] != self.elevation[y][x]
                    for dx, dy in FACE_DIRECTIONS
                )
                if edge and choice < 16:
                    choice += 16 if seed % 3 else 32
                self.tiles[y][x] = min(choice, 43)

    def build_walls(self) -> tuple[list, list, list, list]:
        wall_tiles = [[[-1, -1, -1, -1] for _ in range(self.w)] for _ in range(self.h)]
        wall_heights = [[[0, 0, 0, 0] for _ in range(self.w)] for _ in range(self.h)]
        built = [
            [[[-1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(self.w)]
            for _ in range(self.h)
        ]
        cliff = [
            [[[-1 for _ in range(PIECE_ROWS)] for _ in range(4)] for _ in range(self.w)]
            for _ in range(self.h)
        ]

        palettes = {
            0: (49, 55, 61, 63),
            1: (54, 58, 62, 60),
        }

        for y in range(self.h):
            for x in range(self.w):
                if not self.enabled[y][x]:
                    continue
                cid = max(0, self.chamber[y][x])
                primary, accent, base_material, damaged = palettes.get(cid, palettes[0])
                for face, (dx, dy) in enumerate(FACE_DIRECTIONS):
                    seed = x * 19 + y * 31 + face * 7 + cid * 43
                    material = accent if seed % 8 == 0 else primary
                    if seed % 17 == 0:
                        material = damaged
                    wall_tiles[y][x][face] = material
                    for row in range(PIECE_ROWS):
                        cliff[y][x][face][row] = 60 + ((seed + row) % 4)

                    nx, ny = x + dx, y + dy
                    if self.inside(nx, ny) and self.enabled[ny][nx]:
                        continue

                    # Sparse exterior wall caps; raised terrain supplies mass.
                    height = 2 if face == 0 else (1 if face in (1, 3) else 0)
                    wall_heights[y][x][face] = height
                    if height > 0:
                        built[y][x][face][0] = base_material
                    if height > 1:
                        built[y][x][face][1] = material

        return wall_tiles, wall_heights, built, cliff

    def world(self, cell_x: float, cell_y: float) -> tuple[float, float]:
        return (
            self.origin_x + (cell_x + 0.5) * TILE,
            self.origin_y + (cell_y + 0.5) * TILE,
        )

    def terrain_at(self, cell_x: float, cell_y: float) -> int:
        x = max(0, min(self.w - 1, int(cell_x)))
        y = max(0, min(self.h - 1, int(cell_y)))
        return self.elevation[y][x]

    def asset_exists(self, path: str) -> bool:
        exists = (self.root / path).is_file()
        if not exists:
            self.missing_assets.add(path)
        return exists

    def add_object(
        self,
        path: str,
        x: float,
        y: float,
        w: float,
        h: float,
        *,
        layer: int = WORLD_LAYER,
        mode: int = BILLBOARD,
        orientation: int = FACE_CAMERA,
        tint: tuple[int, int, int, int] = (255, 255, 255, 255),
        opacity: float = 1.0,
        rotation: float = 0.0,
        collision: bool = False,
        shape: int = 1,
        radius: float = 26.0,
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
        source_frame: int = 0,
        source_rect: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0),
        visual_offset: tuple[float, float] = (0.0, 0.0),
        collider_offset: tuple[float, float] = (0.0, 0.0),
        anchor_y: float = 1.0,
        depth_bias: float = 0.0,
    ) -> None:
        # Missing optional props are skipped instead of breaking the map build.
        if not self.asset_exists(path):
            return

        wx, wy = self.world(x, y)
        self.obstacles.append({
            "position": (wx, wy),
            "size": (float(w), float(h)),
            "type": 7,
            "height_level": 0,
            "casts_shadow": int(casts_shadow and mode == BILLBOARD),
            "blocks_light": int(blocks_light and mode == BILLBOARD),
            "layer": layer,
            "mode": mode,
            "orientation": orientation,
            "tint": tint,
            "opacity": opacity,
            "animated": int(animated),
            "columns": columns,
            "rows": rows,
            "frames": frames,
            "fps": fps,
            "phase": phase,
            "source_frame": source_frame,
            "source_rect": source_rect,
            "rotation": rotation,
            "terrain_elevation": self.terrain_at(x, y) if mode == GROUND_DECAL else -1,
            "clip": int(clip),
            "allow_ramps": int(allow_ramps),
            "path": path,
            "collision": int(collision and mode == BILLBOARD),
            "auto": 0,
            "shape": shape,
            "collider": collider,
            "radius": radius,
            "visual_offset": visual_offset,
            "collider_offset": collider_offset,
            "anchor_y": max(0.0, min(1.0, anchor_y)),
            "depth_bias": depth_bias,
        })

    def add_decal(self, path: str, x: float, y: float, w: float, h: float, **kwargs: object) -> None:
        self.add_object(
            path,
            x,
            y,
            w,
            h,
            mode=GROUND_DECAL,
            layer=BACKGROUND,
            orientation=FACE_CAMERA,
            casts_shadow=False,
            collision=False,
            **kwargs,
        )

    def add_dungeon_decoration(
        self,
        frame: int,
        x: float,
        y: float,
        *,
        scale: float = 1.0,
        collision: bool = False,
        depth_bias: float = 0.0,
        opacity: float = 1.0,
        tint: tuple[int, int, int, int] = (255, 255, 255, 255),
    ) -> None:
        """Place one correctly grounded cell from dungeon_decorations.png.

        The atlas is 2048 x 2048 with a regular 4 x 4 grid. Every source
        cell is square, so each destination is also square to avoid stretching.
        Per-frame anchor values compensate for the transparent bottom padding,
        placing the visible base of every prop directly on the terrain.
        """
        atlas = "Assets/tiles/dungeon_decorations.png"

        # Full-cell world size. Keeping width == height preserves the source
        # pixel aspect ratio and prevents tall props from becoming too wide.
        base_size = (
            210.0,  # 0  brazier
            250.0,  # 1  lantern tower
            280.0,  # 2  stone sarcophagus
            300.0,  # 3  broken sarcophagus
            260.0,  # 4  guardian lion
            300.0,  # 5  fallen column
            300.0,  # 6  rubble mound
            310.0,  # 7  collapsed wall
            290.0,  # 8  treasure cache
            265.0,  # 9  carved marker / obelisk
            270.0,  # 10 ritual pedestal
            300.0,  # 11 tall square pillar
            280.0,  # 12 grave rubble
            250.0,  # 13 treasure chest
            280.0,  # 14 low offering table
            300.0,  # 15 ritual altar
        )

        # Alpha-bounds bottom edge divided by 512. This is the visible ground
        # contact point inside each otherwise square atlas cell.
        anchor_y = (
            463.0 / 512.0,
            464.0 / 512.0,
            467.0 / 512.0,
            454.0 / 512.0,
            475.0 / 512.0,
            438.0 / 512.0,
            426.0 / 512.0,
            443.0 / 512.0,
            457.0 / 512.0,
            497.0 / 512.0,
            489.0 / 512.0,
            492.0 / 512.0,
            425.0 / 512.0,
            484.0 / 512.0,
            448.0 / 512.0,
            457.0 / 512.0,
        )

        # Gameplay footprints are deliberately much smaller than the artwork.
        # They represent only the object's contact area on the floor.
        collider_specs: tuple[tuple[int, tuple[float, float], float], ...] = (
            (1, (0.0, 0.0), 28.0),    # brazier, circle
            (1, (0.0, 0.0), 22.0),    # lantern, circle
            (0, (150.0, 82.0), 0.0),  # sarcophagus, box
            (0, (172.0, 92.0), 0.0),  # broken sarcophagus, box
            (1, (0.0, 0.0), 35.0),    # lion, circle
            (0, (184.0, 70.0), 0.0),  # fallen column, box
            (0, (168.0, 76.0), 0.0),  # rubble, box
            (0, (192.0, 78.0), 0.0),  # wall, box
            (0, (148.0, 76.0), 0.0),  # treasure cache, box
            (1, (0.0, 0.0), 30.0),    # obelisk, circle
            (1, (0.0, 0.0), 38.0),    # pedestal, circle
            (1, (0.0, 0.0), 29.0),    # pillar, circle
            (0, (142.0, 68.0), 0.0),  # grave rubble, box
            (0, (108.0, 64.0), 0.0),  # chest, box
            (0, (142.0, 76.0), 0.0),  # table, box
            (0, (152.0, 86.0), 0.0),  # altar, box
        )

        if frame < 0 or frame >= len(base_size):
            raise ValueError(f"invalid dungeon decoration frame: {frame}")

        size = base_size[frame] * max(0.10, scale)
        shape, collider, radius = collider_specs[frame]
        collider = (collider[0] * scale, collider[1] * scale)
        radius *= scale

        self.add_object(
            atlas,
            x,
            y,
            size,
            size,
            layer=WORLD_LAYER,
            mode=BILLBOARD,
            orientation=FACE_CAMERA,
            tint=tint,
            opacity=opacity,
            collision=collision,
            shape=shape,
            radius=radius,
            collider=collider,
            casts_shadow=frame not in (6, 8, 12),
            animated=False,
            columns=4,
            rows=4,
            frames=1,
            source_frame=frame,
            anchor_y=anchor_y[frame],
            depth_bias=depth_bias,
        )

    def place_environment(self) -> None:
        authored = "Assets/environment/authored"

        # Floor decals are not upright decoration sprites, so they remain as
        # dedicated ground assets. The actual props below come almost entirely
        # from dungeon_decorations.png.
        self.add_decal(f"{authored}/decal_dragon_grand.png", 28, 57, 640, 650, opacity=0.52)
        self.add_decal(
            f"{authored}/decal_azure_seal_round.png",
            28,
            20,
            720,
            740,
            tint=(155, 224, 245, 255),
            opacity=0.62,
        )
        self.add_decal(f"{authored}/threshold_emblem_gold.png", 28, 44, 360, 86, opacity=0.72)
        self.add_decal(f"{authored}/threshold_blue_runes.png", 28, 35, 380, 90, opacity=0.74)
        self.add_decal(f"{authored}/threshold_purple_seal.png", 28, 4, 380, 92, opacity=0.68)

        # Gates remain standalone because they need fixed world-plane
        # projection instead of the camera-facing isometric prop artwork.
        self.add_object(
            f"{authored}/gate_open_frame.png",
            28,
            70,
            390,
            470,
            layer=BACKGROUND,
            orientation=WORLD_PLANE_X,
            casts_shadow=False,
        )
        self.add_object(
            f"{authored}/gate_magic_seal.png",
            28,
            40,
            420,
            500,
            layer=BACKGROUND,
            orientation=WORLD_PLANE_X,
            casts_shadow=False,
        )
        self.add_object(
            f"{authored}/gate_boss_chained.png",
            28,
            4,
            470,
            560,
            layer=BACKGROUND,
            orientation=WORLD_PLANE_X,
            casts_shadow=False,
        )

        decor = self.add_dungeon_decoration

        # --------------------------------------------------------------
        # Chamber 0: dense edge dressing with a broad central combat lane.
        # --------------------------------------------------------------
        for frame, x, y, scale, solid in [
            (11, 17, 64, 1.00, True),
            (11, 39, 64, 1.00, True),
            (11, 15, 56, 0.92, True),
            (11, 41, 56, 0.92, True),
            (0, 20, 63, 0.88, False),
            (0, 36, 63, 0.88, False),
            (0, 18, 49, 0.82, False),
            (0, 38, 49, 0.82, False),
            (1, 16, 61, 0.90, False),
            (1, 40, 61, 0.90, False),
            (2, 17, 52, 1.00, True),
            (2, 39, 52, 1.00, True),
            (3, 15, 48, 0.94, False),
            (3, 41, 48, 0.94, False),
            (13, 20, 54, 0.88, False),
            (13, 36, 54, 0.88, False),
            (8, 18, 58, 0.84, False),
            (8, 38, 58, 0.84, False),
            (6, 14, 52, 0.88, False),
            (6, 42, 52, 0.88, False),
            (5, 20, 47, 0.90, True),
            (5, 36, 47, 0.90, True),
            (14, 18, 66, 0.86, False),
            (14, 38, 66, 0.86, False),
            (10, 14, 64, 0.82, False),
            (10, 42, 64, 0.82, False),
        ]:
            decor(frame, x, y, scale=scale, collision=solid)

        # --------------------------------------------------------------
        # Chamber 1: monumental perimeter around an intentionally empty
        # centre. The Boss can always dash to cell (28, 20) unobstructed.
        # --------------------------------------------------------------
        for frame, x, y, scale, solid in [
            (4, 15, 31, 1.05, True),
            (4, 41, 31, 1.05, True),
            (11, 13, 22, 1.05, True),
            (11, 43, 22, 1.05, True),
            (11, 15, 8, 1.00, True),
            (11, 41, 8, 1.00, True),
            (9, 14, 16, 1.00, True),
            (9, 42, 16, 1.00, True),
            (2, 18, 10, 1.02, True),
            (2, 38, 10, 1.02, True),
            (3, 14, 27, 1.00, True),
            (3, 42, 27, 1.00, True),
            (7, 12, 13, 1.00, True),
            (7, 44, 13, 1.00, True),
            (7, 12, 28, 0.92, True),
            (7, 44, 28, 0.92, True),
            (6, 16, 14, 0.90, False),
            (6, 40, 14, 0.90, False),
            (6, 16, 25, 0.90, False),
            (6, 40, 25, 0.90, False),
            (10, 19, 32, 0.94, False),
            (10, 37, 32, 0.94, False),
            (0, 22, 32, 0.86, False),
            (0, 34, 32, 0.86, False),
            (1, 14, 34, 0.92, False),
            (1, 42, 34, 0.92, False),
            (5, 18, 6, 0.96, True),
            (5, 38, 6, 0.96, True),
            (13, 20, 29, 0.88, False),
            (13, 36, 29, 0.88, False),
            (12, 13, 8, 0.92, False),
            (12, 43, 8, 0.92, False),
            (8, 24, 8, 0.82, False),
            (8, 32, 8, 0.82, False),
        ]:
            decor(frame, x, y, scale=scale, collision=solid)

        # Main altar on the raised northern stage. It uses the same accepted
        # decoration atlas and no longer relies on the mismatched throne art.
        decor(15, 28, 8, scale=1.20, collision=True, depth_bias=2.0)

    def add_light(
        self,
        ident: int,
        name: str,
        chamber: int,
        x: float,
        y: float,
        radius: float,
        intensity: float,
        color: tuple[int, int, int, int],
        phase: float,
        flicker: float = 0.08,
        speed: float = 7.0,
    ) -> None:
        wx, wy = self.world(x, y)
        self.lights.append({
            "id": ident,
            "name": name,
            "chamber": chamber,
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
            "radius_flicker": 0.035,
            "phase": phase,
        })

    def place_lights(self) -> None:
        for args in [
            (100, "Ember Hall Ambient", 0, 28, 57, 860, 0.43, (235, 190, 145, 255), 0.2, 0.03),
            (101, "Ember West Fire", 0, 21, 62, 360, 0.66, (255, 143, 72, 255), 1.0, 0.12),
            (102, "Ember East Fire", 0, 35, 62, 360, 0.66, (255, 143, 72, 255), 2.0, 0.12),
            (103, "Connector Seal", 0, 28, 40, 430, 0.54, (152, 214, 235, 255), 2.8, 0.05),
            (110, "Warden Arena Ambient", 1, 28, 20, 1180, 0.45, (155, 172, 205, 255), 0.5, 0.02),
            (111, "Warden West Azure", 1, 15, 34, 390, 0.66, (85, 211, 240, 255), 1.5, 0.10),
            (112, "Warden East Azure", 1, 41, 34, 390, 0.66, (85, 211, 240, 255), 2.5, 0.10),
            (113, "Warden Throne", 1, 28, 8, 620, 0.58, (235, 125, 92, 255), 3.2, 0.08),
        ]:
            self.add_light(*args)

    def grid_section(self, tag: str, rows: list[list[int]], extra: str = "") -> list[str]:
        return [f"{tag} {self.w} {self.h}{extra}"] + [" ".join(map(str, row)) for row in rows]

    def wall_section(self, tag: str, data: list[list[list[int]]]) -> list[str]:
        output = [f"{tag} {self.w} {self.h}"]
        for row in data:
            values: list[str] = []
            for cell in row:
                values.extend(map(str, cell))
            output.append(" ".join(values))
        return output

    def piece_section(self, tag: str, data: list[list[list[list[int]]]]) -> list[str]:
        output = [f"{tag} {self.w} {self.h} {PIECE_ROWS}"]
        for row in data:
            values: list[str] = []
            for cell in row:
                for face in cell:
                    values.extend(map(str, face))
            output.append(" ".join(values))
        return output

    @staticmethod
    def obstacle_line(item: dict[str, object]) -> str:
        x, y = item["position"]
        w, h = item["size"]
        tint = item["tint"]
        sx, sy, sw, sh = item["source_rect"]
        cw, ch = item["collider"]
        path = str(item["path"]).replace('"', "")
        values = [
            x,
            y,
            w,
            h,
            item["type"],
            item["height_level"],
            item["casts_shadow"],
            item["blocks_light"],
            item["layer"],
            item["mode"],
            item["orientation"],
            *tint,
            item["opacity"],
            item["animated"],
            item["columns"],
            item["rows"],
            item["frames"],
            item["fps"],
            item["phase"],
            item["source_frame"],
            sx,
            sy,
            sw,
            sh,
            item["rotation"],
            item["terrain_elevation"],
            item["clip"],
            item["allow_ramps"],
            f'"{path}"',
            item["collision"],
            item["auto"],
            item["shape"],
            cw,
            ch,
            item["radius"],
            *item["visual_offset"],
            *item["collider_offset"],
            item["anchor_y"],
            item["depth_bias"],
        ]
        return " ".join(str(value) for value in values)

    @staticmethod
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

    def is_traversable(self, ax: int, ay: int, bx: int, by: int) -> bool:
        if not self.inside(bx, by) or not self.enabled[by][bx]:
            return False
        if self.elevation[ay][ax] == self.elevation[by][bx]:
            return True

        direction = self.ramp[ay][ax]
        if direction == NORTH and (ax, ay - 1) == (bx, by):
            return self.elevation[by][bx] == self.elevation[ay][ax] + 1
        if direction == EAST and (ax + 1, ay) == (bx, by):
            return self.elevation[by][bx] == self.elevation[ay][ax] + 1
        if direction == SOUTH and (ax, ay + 1) == (bx, by):
            return self.elevation[by][bx] == self.elevation[ay][ax] + 1
        if direction == WEST and (ax - 1, ay) == (bx, by):
            return self.elevation[by][bx] == self.elevation[ay][ax] + 1

        direction = self.ramp[by][bx]
        if direction == NORTH and (bx, by - 1) == (ax, ay):
            return self.elevation[ay][ax] == self.elevation[by][bx] + 1
        if direction == EAST and (bx + 1, by) == (ax, ay):
            return self.elevation[ay][ax] == self.elevation[by][bx] + 1
        if direction == SOUTH and (bx, by + 1) == (ax, ay):
            return self.elevation[ay][ax] == self.elevation[by][bx] + 1
        if direction == WEST and (bx - 1, by) == (ax, ay):
            return self.elevation[ay][ax] == self.elevation[by][bx] + 1
        return False

    def validate(self) -> None:
        # Verify authored ramps.
        for y in range(self.h):
            for x in range(self.w):
                direction = self.ramp[y][x]
                if direction == NONE:
                    continue
                dx, dy = FACE_DIRECTIONS[direction - 1]
                tx, ty = x + dx, y + dy
                if not self.inside(tx, ty):
                    raise ValueError(f"ramp outside map: {(x, y)}")
                if self.elevation[ty][tx] != self.elevation[y][x] + 1:
                    raise ValueError(f"invalid ramp height: {(x, y)} -> {(tx, ty)}")

        # Reachability from the entrance must include both chambers and exit.
        start = (28, self.h - 1)
        queue = deque([start])
        visited = {start}
        while queue:
            x, y = queue.popleft()
            for dx, dy in FACE_DIRECTIONS:
                nx, ny = x + dx, y + dy
                if (nx, ny) in visited:
                    continue
                if self.is_traversable(x, y, nx, ny):
                    visited.add((nx, ny))
                    queue.append((nx, ny))

        missing = {
            room.ident
            for room in self.rooms
            if not any(
                self.chamber[y][x] == room.ident and (x, y) in visited
                for y in range(self.h)
                for x in range(self.w)
            )
        }
        if missing:
            raise ValueError(f"unreachable chambers: {sorted(missing)}")
        if (28, 0) not in visited:
            raise ValueError("north victory threshold is unreachable")

    def build_lines(self) -> list[str]:
        wall_tiles, wall_heights, built, cliff = self.build_walls()
        lines = ["MOXIANG_LEVEL 14"]
        lines += self.grid_section("TILES", self.tiles)
        lines += self.grid_section("TERRAIN_ELEVATIONS", self.elevation, " 64")
        lines += self.grid_section("TERRAIN_RAMPS", self.ramp)
        lines += self.wall_section("WALL_TILES", wall_tiles)
        lines += self.wall_section("WALL_HEIGHTS", wall_heights)
        lines += self.piece_section("BUILT_WALL_PIECES", built)
        lines += self.piece_section("CLIFF_WALL_PIECES", cliff)
        lines += [f"CHAMBERS {len(self.rooms)}"]
        lines += [f'{room.ident} "{room.name}"' for room in self.rooms]
        lines += [f"CHAMBER_STYLES {len(self.rooms)}"]
        lines += [
            f"{room.ident} {room.ambient[0]} {room.ambient[1]} {room.ambient[2]} {room.ambient[3]} "
            f"{room.vignette} {room.camera_zoom} {room.focus[0]} {room.focus[1]}"
            for room in self.rooms
        ]

        cell_rows: list[list[int]] = []
        for y in range(self.h):
            values: list[int] = []
            for x in range(self.w):
                values.extend((self.enabled[y][x], self.chamber[y][x]))
            cell_rows.append(values)
        lines += self.grid_section("CELL_LAYOUT", cell_rows)
        lines += ['TERRAIN_ATLAS "Assets/tiles/dungeon_master_atlas.png" 8 8']
        lines += ["TILE_WALKABILITY 64"] + [f"{index} 1" for index in range(64)]
        lines += ["TILE_BRUSHES 64"] + [f'{index} "" 1 90 90 94 255' for index in range(64)]
        lines += [f"OBSTACLES {len(self.obstacles)}"] + [self.obstacle_line(item) for item in self.obstacles]
        lines += [f"LIGHTS {len(self.lights)}"] + [self.light_line(item) for item in self.lights]
        lines += ["NPCS 0"]
        return lines

    def write(self) -> None:
        self.output.parent.mkdir(parents=True, exist_ok=True)
        if self.output.exists():
            backup = self.output.with_suffix(self.output.suffix + ".before_two_chambers.bak")
            shutil.copy2(self.output, backup)
            print(f"Backup: {backup}")

        self.output.write_text("\n".join(self.build_lines()) + "\n", encoding="utf-8")
        print(
            f"Built {self.output} | map={self.w}x{self.h} | chambers={len(self.rooms)} | "
            f"obstacles={len(self.obstacles)} | lights={len(self.lights)}"
        )
        if self.missing_assets:
            print("Skipped missing optional assets:")
            for path in sorted(self.missing_assets):
                print(f"  - {path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Path to the inner Moxiang2D folder containing Game.cpp, Assets and levels.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Optional output .mox path. Defaults to <root>/levels/level01.mox.",
    )
    return parser.parse_args()


def detect_root(script: Path, explicit: Path | None) -> Path:
    if explicit is not None:
        return explicit.resolve()

    candidates = [
        Path.cwd(),
        Path.cwd() / "Moxiang2D",
        script.resolve().parent,
        script.resolve().parent.parent,
    ]
    for candidate in candidates:
        if (candidate / "Game.cpp").is_file() and (candidate / "Assets").is_dir():
            return candidate.resolve()

    raise SystemExit(
        "Could not detect the inner Moxiang2D project folder. "
        "Pass --root D:/Dev/Moxiang2D/Moxiang2D"
    )


def main() -> int:
    args = parse_args()
    root = detect_root(Path(__file__), args.root)
    output = args.output.resolve() if args.output else root / "levels" / "level01.mox"

    builder = LevelBuilder(root, output)
    builder.build_layout()
    builder.assign_floor_tiles()
    builder.place_environment()
    builder.place_lights()
    builder.validate()
    builder.write()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
