#!/usr/bin/env python3
"""Build a fast two-chamber Moxiang2D level.

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

    def place_environment(self) -> None:
        generated = "Assets/environment/generated"
        authored = "Assets/environment/authored"

        # Room-wide decals establish distinct visual identities.
        self.add_decal(f"{authored}/decal_dragon_grand.png", 28, 57, 640, 650, opacity=0.58)
        self.add_decal(
            f"{authored}/decal_azure_seal_round.png",
            28,
            20,
            720,
            740,
            tint=(155, 224, 245, 255),
            opacity=0.68,
        )
        self.add_decal(f"{authored}/threshold_emblem_gold.png", 28, 44, 360, 86, opacity=0.76)
        self.add_decal(f"{authored}/threshold_blue_runes.png", 28, 35, 380, 90, opacity=0.78)
        self.add_decal(f"{authored}/threshold_purple_seal.png", 28, 4, 380, 92, opacity=0.72)

        # Gates use explicit world-plane projection, unlike isometric props.
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

        def prop(name: str, x: float, y: float, w: float, h: float, **kwargs: object) -> None:
            self.add_object(f"{generated}/{name}.png", x, y, w, h, **kwargs)

        # Chamber 0: compact clutter ring with a large open centre.
        for args in [
            ("structural_short_square_pillar", 18, 64, 250, 300, True, 27),
            ("structural_damaged_square_pillar", 38, 64, 260, 320, True, 27),
            ("structural_short_round_column", 16, 55, 245, 300, True, 26),
            ("structural_broken_round_column", 40, 55, 250, 300, True, 26),
            ("structural_low_rect_pedestal", 19, 48, 300, 220, True, 28),
            ("structural_low_square_pedestal", 37, 48, 250, 230, True, 26),
            ("ritual_amber_brazier", 21, 62, 250, 300, False, 20),
            ("ritual_cyan_brazier", 35, 62, 250, 300, False, 20),
            ("ritual_incense_burner", 16, 61, 220, 260, False, 18),
            ("ritual_offering_table", 40, 61, 310, 250, False, 24),
            ("storage_chest_bronze_closed", 19, 53, 250, 200, False, 22),
            ("storage_crate", 37, 53, 220, 210, False, 22),
            ("debris_rubble_pile", 16, 49, 250, 190, False, 20),
            ("debris_broken_floor_slab", 40, 49, 280, 190, False, 20),
            ("debris_ceramic_fragments", 22, 47, 220, 150, False, 18),
            ("debris_loose_stones", 34, 47, 210, 145, False, 18),
        ]:
            name, x, y, w, h, collision, radius = args
            prop(name, x, y, w, h, collision=collision, radius=radius)

        # Chamber 1: larger landmark silhouettes around a broad boss arena.
        for args in [
            ("ornament_guardian_statue", 14, 29, 350, 430, True, 34),
            ("ornament_guardian_statue", 42, 29, 350, 430, True, 34),
            ("ornament_rune_obelisk", 13, 18, 310, 430, True, 30),
            ("ornament_damaged_rune_obelisk", 43, 18, 310, 430, True, 30),
            ("ornament_sword_shrine", 18, 10, 360, 400, True, 32),
            ("ornament_dragon_relief_monument", 38, 10, 390, 420, True, 34),
            ("ornament_small_sarcophagus", 16, 24, 340, 250, True, 30),
            ("ornament_small_sarcophagus", 40, 24, 340, 250, True, 30),
            ("ornament_circular_ritual_altar", 19, 32, 390, 290, False, 28),
            ("ornament_rectangular_ceremonial_altar", 37, 32, 390, 280, False, 28),
            ("ritual_seal_pedestal", 17, 15, 280, 290, False, 23),
            ("ritual_rune_pedestal", 39, 15, 280, 290, False, 23),
            ("ritual_spirit_lantern", 15, 34, 230, 320, False, 20),
            ("ritual_spirit_lantern", 41, 34, 230, 320, False, 20),
            ("debris_pillar_fragments", 12, 12, 300, 190, False, 22),
            ("debris_wall_bricks", 44, 12, 300, 190, False, 22),
            ("storage_chest_ceremonial", 20, 27, 260, 210, False, 22),
            ("storage_chest_broken", 36, 27, 250, 200, False, 22),
        ]:
            name, x, y, w, h, collision, radius = args
            prop(name, x, y, w, h, collision=collision, radius=radius)

        # Authored throne landmark on the raised stage.
        self.add_object(
            f"{authored}/warden_throne.png",
            28,
            8,
            500,
            570,
            collision=True,
            radius=38,
            depth_bias=2.0,
        )

        # Animated flames and fire stands are separate for better readability.
        for index, (x, y, tint, phase) in enumerate([
            (21, 62, (255, 150, 74, 255), 0.3),
            (35, 62, (255, 150, 74, 255), 1.5),
            (15, 34, (89, 216, 241, 255), 0.9),
            (41, 34, (89, 216, 241, 255), 2.2),
        ]):
            self.add_object(
                "Assets/environment/azure_flame_4x4.png",
                x,
                y,
                96,
                142,
                layer=BACKGROUND,
                tint=tint,
                opacity=0.92,
                casts_shadow=False,
                animated=True,
                columns=4,
                rows=4,
                frames=16,
                fps=12.0,
                phase=phase,
                visual_offset=(0.0, -18.0),
            )

        # Sparse foreground framing creates depth without covering combat.
        for path, x, y, w, h, opacity in [
            (f"{authored}/foreground_pillar_left.png", 17, 68, 370, 600, 0.58),
            (f"{authored}/foreground_pillar_right.png", 39, 68, 370, 600, 0.58),
            (f"{authored}/foreground_corner_drape.png", 13, 35, 420, 500, 0.40),
            (f"{authored}/foreground_hanging_beam.png", 43, 35, 420, 500, 0.40),
        ]:
            self.add_object(
                path,
                x,
                y,
                w,
                h,
                layer=FOREGROUND,
                opacity=opacity,
                casts_shadow=False,
                collision=False,
            )

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
