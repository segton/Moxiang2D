#!/usr/bin/env python3
"""Validate .mox level structure and referenced runtime assets.

This catches the most common desktop/web mismatch before compilation:
missing files, case-only path errors, invalid atlas grids, malformed v11-v13
obstacles, terrain-aware decal metadata and duplicate light IDs.
"""
from __future__ import annotations

import argparse
import shlex
import sys
from collections import deque
from pathlib import Path
from typing import Iterable

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - developer machine setup
    raise SystemExit("Pillow is required: python -m pip install Pillow") from exc

KNOWN_TAGS = {
    "TILES", "TERRAIN_ELEVATIONS", "TERRAIN_RAMPS", "WALL_TILES",
    "WALL_HEIGHTS", "BUILT_WALL_PIECES", "CLIFF_WALL_PIECES",
    "CHAMBERS", "CHAMBER_STYLES", "CELL_LAYOUT", "TERRAIN_ATLAS",
    "TILE_WALKABILITY", "TILE_BRUSHES", "OBSTACLES", "LIGHTS", "NPCS",
}


def is_section(line: str) -> bool:
    return bool(line) and line.split(maxsplit=1)[0] in KNOWN_TAGS


def section(lines: list[str], tag: str) -> tuple[list[str], list[str]]:
    for index, line in enumerate(lines):
        if line.startswith(tag + " "):
            header = shlex.split(line)
            end = index + 1
            while end < len(lines) and not is_section(lines[end]):
                end += 1
            return header, lines[index + 1:end]
    raise ValueError(f"missing section {tag}")


def exact_path(root: Path, portable: str) -> Path | None:
    current = root
    for part in Path(portable.replace("\\", "/")).parts:
        if part in (".", ""):
            continue
        if not current.is_dir():
            return None
        names = {child.name: child for child in current.iterdir()}
        if part not in names:
            return None
        current = names[part]
    return current


def check_count(tag: str, header: list[str], body: list[str], errors: list[str]) -> None:
    if len(header) < 2:
        errors.append(f"{tag}: missing count")
        return
    try:
        expected = int(header[1])
    except ValueError:
        errors.append(f"{tag}: invalid count {header[1]!r}")
        return
    if len(body) != expected:
        errors.append(f"{tag}: header says {expected}, found {len(body)} records")


def parse_grid(
    lines: list[str],
    tag: str,
    values_per_cell: int,
    errors: list[str],
) -> tuple[int, int, list[list[int]]]:
    header, body = section(lines, tag)
    if len(header) < 3:
        errors.append(f"{tag}: missing width/height")
        return 0, 0, []

    width = int(header[1])
    height = int(header[2])
    if len(body) != height:
        errors.append(f"{tag}: height says {height}, found {len(body)} rows")

    grid: list[list[int]] = []
    expected = width * values_per_cell
    for row_index, record in enumerate(body):
        values = [int(value) for value in shlex.split(record)]
        if len(values) != expected:
            errors.append(
                f"{tag} row {row_index + 1}: expected {expected} values, "
                f"found {len(values)}"
            )
        grid.append(values)

    return width, height, grid


def validate(level: Path) -> list[str]:
    errors: list[str] = []
    project = level.resolve().parents[1]
    lines = level.read_text(encoding="utf-8").splitlines()

    if not lines:
        return ["level is empty"]

    first = shlex.split(lines[0])
    if len(first) != 2 or first[0] != "MOXIANG_LEVEL":
        return ["invalid MOXIANG_LEVEL header"]
    try:
        version = int(first[1])
    except ValueError:
        return ["invalid level version"]
    if version < 11:
        errors.append(f"level version {version}; atmosphere authoring requires version 11")
    if version > 13:
        errors.append(f"level version {version}; validator currently supports up to version 13")

    try:
        chamber_header, chamber_body = section(lines, "CHAMBERS")
        check_count("CHAMBERS", chamber_header, chamber_body, errors)
        chamber_ids: set[int] = set()
        for record in chamber_body:
            fields = shlex.split(record)
            if len(fields) != 2:
                errors.append(f"CHAMBERS malformed: {record}")
                continue
            chamber_id = int(fields[0])
            if chamber_id in chamber_ids:
                errors.append(f"duplicate chamber ID {chamber_id}")
            chamber_ids.add(chamber_id)

        style_header, style_body = section(lines, "CHAMBER_STYLES")
        check_count("CHAMBER_STYLES", style_header, style_body, errors)
        styled_ids: set[int] = set()
        for record in style_body:
            fields = shlex.split(record)
            expected_style_fields = 9 if version >= 12 else 6
            if len(fields) != expected_style_fields:
                errors.append(
                    f"CHAMBER_STYLES malformed: expected {expected_style_fields} "
                    f"fields, found {len(fields)}: {record}"
                )
                continue
            chamber_id = int(fields[0])
            styled_ids.add(chamber_id)
            rgba = [int(value) for value in fields[1:5]]
            vignette = float(fields[5])
            camera_zoom = float(fields[6]) if version >= 12 else 1.5
            focus_x = float(fields[7]) if version >= 12 else 0.0
            focus_y = float(fields[8]) if version >= 12 else 0.0
            if any(value < 0 or value > 255 for value in rgba):
                errors.append(f"chamber {chamber_id}: ambient RGBA outside 0..255")
            if not 0.0 <= vignette <= 0.85:
                errors.append(f"chamber {chamber_id}: vignette outside 0..0.85")
            if not 0.35 <= camera_zoom <= 3.5:
                errors.append(f"chamber {chamber_id}: camera zoom outside 0.35..3.5")
            if abs(focus_x) > 4096 or abs(focus_y) > 4096:
                errors.append(f"chamber {chamber_id}: camera focus offset is unreasonable")
        missing_styles = chamber_ids - styled_ids
        if missing_styles:
            errors.append(f"missing CHAMBER_STYLES for IDs {sorted(missing_styles)}")

        cell_header, cell_body = section(lines, "CELL_LAYOUT")
        if len(cell_header) != 3:
            errors.append("CELL_LAYOUT must contain width and height")
            map_width = 0
            map_height = 0
            cell_layout: list[list[tuple[int, int]]] = []
        else:
            map_width = int(cell_header[1])
            map_height = int(cell_header[2])
            cell_layout = []

            if len(cell_body) != map_height:
                errors.append(
                    f"CELL_LAYOUT height says {map_height}, found {len(cell_body)} rows"
                )

            for row_index, record in enumerate(cell_body):
                values = [int(value) for value in shlex.split(record)]
                expected_values = map_width * 2
                if len(values) != expected_values:
                    errors.append(
                        f"CELL_LAYOUT row {row_index + 1}: expected "
                        f"{expected_values} values, found {len(values)}"
                    )
                    continue

                cell_layout.append([
                    (values[column * 2], values[column * 2 + 1])
                    for column in range(map_width)
                ])

        tiles_width, tiles_height, tile_grid = parse_grid(
            lines, "TILES", 1, errors
        )
        elevation_width, elevation_height, elevation_grid = parse_grid(
            lines, "TERRAIN_ELEVATIONS", 1, errors
        )
        ramp_width, ramp_height, ramp_grid = parse_grid(
            lines, "TERRAIN_RAMPS", 1, errors
        )
        wall_width, wall_height, wall_grid = parse_grid(
            lines, "WALL_TILES", 4, errors
        )

        for tag_name, width, height in (
            ("TILES", tiles_width, tiles_height),
            ("TERRAIN_ELEVATIONS", elevation_width, elevation_height),
            ("TERRAIN_RAMPS", ramp_width, ramp_height),
            ("WALL_TILES", wall_width, wall_height),
        ):
            if width != map_width or height != map_height:
                errors.append(
                    f"{tag_name}: dimensions {width}x{height} do not match "
                    f"CELL_LAYOUT {map_width}x{map_height}"
                )

        def world_cell(x: float, y: float) -> tuple[int, int, int, int] | None:
            if (
                map_width <= 0 or
                map_height <= 0 or
                len(cell_layout) != map_height
            ):
                return None

            tile_size = 64.0
            origin_x = -map_width * tile_size * 0.5
            origin_y = -map_height * tile_size * 0.5
            cell_x = int((x - origin_x) // tile_size)
            cell_y = int((y - origin_y) // tile_size)

            if not (0 <= cell_x < map_width and 0 <= cell_y < map_height):
                return None

            enabled, chamber_id = cell_layout[cell_y][cell_x]
            return cell_x, cell_y, enabled, chamber_id

        atlas_header, _ = section(lines, "TERRAIN_ATLAS")
        atlas_tile_count = 0
        atlas_portable_path = ""
        if len(atlas_header) != 4:
            errors.append("TERRAIN_ATLAS must contain path, columns and rows")
        else:
            atlas_portable_path = atlas_header[1]
            atlas_path = exact_path(project, atlas_header[1])
            columns = int(atlas_header[2])
            rows = int(atlas_header[3])
            atlas_tile_count = max(0, columns * rows)
            if atlas_path is None or not atlas_path.is_file():
                errors.append(f"missing/case-mismatched atlas: {atlas_header[1]}")
            elif columns <= 0 or rows <= 0:
                errors.append("TERRAIN_ATLAS columns/rows must be positive")
            else:
                with Image.open(atlas_path) as image:
                    if image.width % columns or image.height % rows:
                        errors.append(
                            f"atlas {atlas_header[1]} size {image.width}x{image.height} "
                            f"is not divisible by {columns}x{rows}"
                        )

        if atlas_tile_count > 0:
            for row_index, values in enumerate(tile_grid):
                for column, tile_index in enumerate(values):
                    if tile_index < 0 or tile_index >= atlas_tile_count:
                        errors.append(
                            f"TILES ({column}, {row_index}): tile {tile_index} "
                            f"outside atlas range 0..{atlas_tile_count - 1}"
                        )

            for row_index, values in enumerate(wall_grid):
                for value_index, tile_index in enumerate(values):
                    if tile_index < -1 or tile_index >= atlas_tile_count:
                        cell_x = value_index // 4
                        face = value_index % 4
                        errors.append(
                            f"WALL_TILES ({cell_x}, {row_index}) face {face}: "
                            f"tile {tile_index} outside -1..{atlas_tile_count - 1}"
                        )

        # RampDirection is None/North/East/South/West = 0..4. A ramp is
        # stored on the lower cell and must point to an enabled level+1 cell.
        offsets = {1: (0, -1), 2: (1, 0), 3: (0, 1), 4: (-1, 0)}
        directional_stair_tiles = {1: 44, 2: 45, 3: 46, 4: 47}
        ramp_count = 0
        if (
            len(ramp_grid) == map_height and
            len(elevation_grid) == map_height and
            len(tile_grid) == map_height and
            len(cell_layout) == map_height
        ):
            for y in range(map_height):
                for x in range(map_width):
                    direction = ramp_grid[y][x]
                    if direction == 0:
                        continue
                    ramp_count += 1
                    if direction not in offsets:
                        errors.append(f"TERRAIN_RAMPS ({x}, {y}): invalid direction {direction}")
                        continue
                    dx, dy = offsets[direction]
                    target_x, target_y = x + dx, y + dy
                    if not (0 <= target_x < map_width and 0 <= target_y < map_height):
                        errors.append(f"TERRAIN_RAMPS ({x}, {y}): target outside map")
                        continue
                    source_enabled = cell_layout[y][x][0] != 0
                    target_enabled = cell_layout[target_y][target_x][0] != 0
                    if not source_enabled or not target_enabled:
                        errors.append(
                            f"TERRAIN_RAMPS ({x}, {y}): source/target cell is disabled"
                        )
                        continue
                    source_elevation = elevation_grid[y][x]
                    target_elevation = elevation_grid[target_y][target_x]
                    if target_elevation != source_elevation + 1:
                        errors.append(
                            f"TERRAIN_RAMPS ({x}, {y}): expected {source_elevation + 1} "
                            f"at target, found {target_elevation}"
                        )
                    if atlas_portable_path.endswith("dungeon_master_atlas.png"):
                        expected_tile = directional_stair_tiles[direction]
                        if tile_grid[y][x] != expected_tile:
                            errors.append(
                                f"TERRAIN_RAMPS ({x}, {y}): direction {direction} "
                                f"requires stair tile {expected_tile}, found {tile_grid[y][x]}"
                            )

            if ramp_count == 0:
                errors.append("TERRAIN_RAMPS: level contains no usable ramps")

            # Ensure every authored chamber can be reached through level edges
            # or correctly oriented ramps. Encounter gates are intentionally
            # ignored because they unlock after a chamber is cleared.
            start: tuple[int, int] | None = None
            for y in range(map_height - 1, -1, -1):
                for x in range(map_width):
                    if cell_layout[y][x][0] != 0:
                        start = (x, y)
                        break
                if start is not None:
                    break

            if start is not None:
                queue = deque([start])
                visited = {start}
                while queue:
                    x, y = queue.popleft()
                    for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < map_width and 0 <= ny < map_height):
                            continue
                        if cell_layout[ny][nx][0] == 0:
                            continue

                        traversable = elevation_grid[ny][nx] == elevation_grid[y][x]
                        if not traversable:
                            source_direction = ramp_grid[y][x]
                            if source_direction in offsets:
                                rdx, rdy = offsets[source_direction]
                                traversable = (
                                    (x + rdx, y + rdy) == (nx, ny) and
                                    elevation_grid[ny][nx] == elevation_grid[y][x] + 1
                                )
                            if not traversable:
                                target_direction = ramp_grid[ny][nx]
                                if target_direction in offsets:
                                    rdx, rdy = offsets[target_direction]
                                    traversable = (
                                        (nx + rdx, ny + rdy) == (x, y) and
                                        elevation_grid[y][x] == elevation_grid[ny][nx] + 1
                                    )

                        if traversable and (nx, ny) not in visited:
                            visited.add((nx, ny))
                            queue.append((nx, ny))

                reached_chambers = {
                    cell_layout[y][x][1]
                    for x, y in visited
                    if cell_layout[y][x][0] != 0
                }
                missing_chambers = chamber_ids - reached_chambers
                if missing_chambers:
                    errors.append(
                        f"unreachable chambers through terrain/ramp network: "
                        f"{sorted(missing_chambers)}"
                    )

        obstacle_header, obstacle_body = section(lines, "OBSTACLES")
        check_count("OBSTACLES", obstacle_header, obstacle_body, errors)
        expected_obstacle_fields = 43 if version >= 13 else (37 if version >= 12 else 28)
        blocking_obstacles: list[dict[str, float | int]] = []
        for row, record in enumerate(obstacle_body, start=1):
            fields = shlex.split(record)
            if len(fields) != expected_obstacle_fields:
                errors.append(
                    f"OBSTACLES row {row}: expected {expected_obstacle_fields} "
                    f"fields, found {len(fields)}"
                )
                continue

            layer = int(fields[8])
            mode = int(fields[9])
            opacity = float(fields[14])
            animated = int(fields[15]) != 0
            columns = int(fields[16])
            rows = int(fields[17])
            frame_count = int(fields[18])
            fps = float(fields[19])
            position_x = float(fields[0])
            position_y = float(fields[1])
            size_x = float(fields[2])
            size_y = float(fields[3])
            height_level = int(fields[5])

            if version >= 12:
                source_frame = int(fields[21])
                source_rect = tuple(float(value) for value in fields[22:26])
                rotation = float(fields[26])
                terrain_elevation = int(fields[27])
                clip_to_elevation = int(fields[28]) != 0
                allow_on_ramps = int(fields[29]) != 0
                asset = fields[30]
                collision_enabled = int(fields[31]) != 0
                collision_shape = int(fields[33])
                collider_x = float(fields[34])
                collider_y = float(fields[35])
                collider_radius = float(fields[36])
                if version >= 13:
                    visual_offset_x = float(fields[37])
                    visual_offset_y = float(fields[38])
                    collider_offset_x = float(fields[39])
                    collider_offset_y = float(fields[40])
                    visual_anchor_y = float(fields[41])
                    depth_bias = float(fields[42])
                else:
                    visual_offset_x = 0.0
                    visual_offset_y = 0.0
                    collider_offset_x = 0.0
                    collider_offset_y = 0.0
                    visual_anchor_y = 1.0
                    depth_bias = 0.0
            else:
                source_frame = 0
                source_rect = (0.0, 0.0, 0.0, 0.0)
                rotation = 0.0
                terrain_elevation = -1
                clip_to_elevation = False
                allow_on_ramps = False
                asset = fields[21]
                collision_enabled = int(fields[22]) != 0
                collision_shape = int(fields[24])
                collider_x = float(fields[25])
                collider_y = float(fields[26])
                collider_radius = float(fields[27])
                visual_offset_x = 0.0
                visual_offset_y = 0.0
                collider_offset_x = 0.0
                collider_offset_y = 0.0
                visual_anchor_y = 1.0
                depth_bias = 0.0

            if not 0.0 <= visual_anchor_y <= 1.0:
                errors.append(f"OBSTACLES row {row}: visual anchor Y outside 0..1")
            if abs(depth_bias) > 4096.0:
                errors.append(f"OBSTACLES row {row}: unreasonable visual height bias")

            if collision_enabled and mode == 0 and height_level <= 0:
                blocking_obstacles.append({
                    "x": position_x + collider_offset_x,
                    "y": position_y + collider_offset_y,
                    "size_x": size_x,
                    "size_y": size_y,
                    "shape": collision_shape,
                    "collider_x": collider_x,
                    "collider_y": collider_y,
                    "radius": collider_radius,
                })

            placed_cell = world_cell(position_x, position_y)
            if placed_cell is None:
                errors.append(
                    f"OBSTACLES row {row}: position is outside CELL_LAYOUT"
                )
            elif placed_cell[2] == 0:
                errors.append(
                    f"OBSTACLES row {row}: position is on disabled cell "
                    f"({placed_cell[0]}, {placed_cell[1]})"
                )

            if layer not in (0, 1, 2):
                errors.append(f"OBSTACLES row {row}: invalid render layer {layer}")
            if mode not in (0, 1):
                errors.append(f"OBSTACLES row {row}: invalid render mode {mode}")
            if not 0.0 <= opacity <= 1.0:
                errors.append(f"OBSTACLES row {row}: opacity outside 0..1")
            if columns <= 0 or rows <= 0:
                errors.append(f"OBSTACLES row {row}: animation grid must be positive")
            if frame_count <= 0 or frame_count > columns * rows:
                errors.append(f"OBSTACLES row {row}: frame count exceeds animation grid")
            if source_frame < 0 or source_frame >= max(1, columns * rows):
                errors.append(f"OBSTACLES row {row}: source frame outside animation grid")
            if animated and fps <= 0.0:
                errors.append(f"OBSTACLES row {row}: animated prop requires positive FPS")
            if abs(rotation) > 100000.0:
                errors.append(f"OBSTACLES row {row}: unreasonable decal rotation")

            source_x, source_y, source_w, source_h = source_rect
            custom_source = source_w > 0.0 and source_h > 0.0
            if (source_w > 0.0) != (source_h > 0.0):
                errors.append(f"OBSTACLES row {row}: custom source width/height must both be positive")
            if source_x < 0.0 or source_y < 0.0:
                errors.append(f"OBSTACLES row {row}: custom source origin cannot be negative")

            if mode == 1 and version >= 12 and placed_cell is not None:
                cell_x, cell_y, cell_enabled, _ = placed_cell
                if clip_to_elevation and cell_enabled and len(elevation_grid) == map_height:
                    actual_elevation = elevation_grid[cell_y][cell_x]
                    if terrain_elevation != actual_elevation:
                        errors.append(
                            f"OBSTACLES row {row}: decal anchor elevation "
                            f"{terrain_elevation} does not match terrain {actual_elevation}"
                        )
                if (
                    not allow_on_ramps and
                    len(ramp_grid) == map_height and
                    ramp_grid[cell_y][cell_x] != 0
                ):
                    errors.append(f"OBSTACLES row {row}: decal anchor is on a ramp")

            if asset:
                asset_path = exact_path(project, asset)
                if asset_path is None or not asset_path.is_file():
                    errors.append(f"OBSTACLES row {row}: missing/case-mismatched asset {asset}")
                else:
                    with Image.open(asset_path) as image:
                        if custom_source:
                            if source_x + source_w > image.width or source_y + source_h > image.height:
                                errors.append(
                                    f"OBSTACLES row {row}: custom source rectangle exceeds "
                                    f"{asset} size {image.width}x{image.height}"
                                )
                        elif image.width % columns or image.height % rows:
                            errors.append(
                                f"OBSTACLES row {row}: {asset} size {image.width}x{image.height} "
                                f"is not divisible by {columns}x{rows}"
                            )

        # Perform a second reachability pass with collision-enabled world
        # props. This catches decorative clusters that accidentally seal the
        # linear route even though the terrain and ramps themselves are valid.
        if (
            map_width > 0 and
            map_height > 0 and
            len(cell_layout) == map_height and
            len(elevation_grid) == map_height and
            len(ramp_grid) == map_height
        ):
            tile_size = 64.0
            origin_x = -map_width * tile_size * 0.5
            origin_y = -map_height * tile_size * 0.5
            player_radius = 20.0

            def cell_blocked_by_prop(cell_x: int, cell_y: int) -> bool:
                center_x = origin_x + (cell_x + 0.5) * tile_size
                center_y = origin_y + (cell_y + 0.5) * tile_size
                for obstacle in blocking_obstacles:
                    delta_x = center_x - float(obstacle["x"])
                    delta_y = center_y - float(obstacle["y"])
                    if int(obstacle["shape"]) == 0:
                        width = float(obstacle["collider_x"])
                        height = float(obstacle["collider_y"])
                        if width <= 0.0 or height <= 0.0:
                            width = float(obstacle["size_x"])
                            height = float(obstacle["size_y"])
                        if (
                            abs(delta_x) <= width * 0.5 + player_radius and
                            abs(delta_y) <= height * 0.5 + player_radius
                        ):
                            return True
                    else:
                        radius = float(obstacle["radius"]) + player_radius + 4.0
                        if delta_x * delta_x + delta_y * delta_y <= radius * radius:
                            return True
                return False

            obstacle_start: tuple[int, int] | None = None
            for y in range(map_height - 1, -1, -1):
                candidates = [
                    x for x in range(map_width)
                    if cell_layout[y][x][0] != 0 and not cell_blocked_by_prop(x, y)
                ]
                if candidates:
                    obstacle_start = (
                        min(candidates, key=lambda x: abs(x - map_width // 2)),
                        y,
                    )
                    break

            if obstacle_start is None:
                errors.append("all southern entrance cells are blocked by collision props")
            else:
                obstacle_queue = deque([obstacle_start])
                obstacle_visited = {obstacle_start}
                while obstacle_queue:
                    x, y = obstacle_queue.popleft()
                    for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < map_width and 0 <= ny < map_height):
                            continue
                        if (nx, ny) in obstacle_visited or cell_layout[ny][nx][0] == 0:
                            continue
                        if cell_blocked_by_prop(nx, ny):
                            continue

                        traversable = elevation_grid[ny][nx] == elevation_grid[y][x]
                        if not traversable:
                            source_direction = ramp_grid[y][x]
                            if source_direction in offsets:
                                rdx, rdy = offsets[source_direction]
                                traversable = (
                                    (x + rdx, y + rdy) == (nx, ny) and
                                    elevation_grid[ny][nx] == elevation_grid[y][x] + 1
                                )
                            if not traversable:
                                target_direction = ramp_grid[ny][nx]
                                if target_direction in offsets:
                                    rdx, rdy = offsets[target_direction]
                                    traversable = (
                                        (nx + rdx, ny + rdy) == (x, y) and
                                        elevation_grid[y][x] == elevation_grid[ny][nx] + 1
                                    )

                        if traversable:
                            obstacle_visited.add((nx, ny))
                            obstacle_queue.append((nx, ny))

                obstacle_reached_chambers = {
                    cell_layout[y][x][1]
                    for x, y in obstacle_visited
                    if cell_layout[y][x][0] != 0
                }
                obstacle_missing_chambers = chamber_ids - obstacle_reached_chambers
                if obstacle_missing_chambers:
                    errors.append(
                        "collision props block progression to chambers: "
                        f"{sorted(obstacle_missing_chambers)}"
                    )

                northern_y = min(
                    y for y in range(map_height)
                    if any(cell_layout[y][x][0] != 0 for x in range(map_width))
                )
                northern_targets = {
                    (x, northern_y)
                    for x in range(map_width)
                    if cell_layout[northern_y][x][0] != 0 and
                    not cell_blocked_by_prop(x, northern_y)
                }
                if northern_targets and not (northern_targets & obstacle_visited):
                    errors.append("collision props block the authored northern level exit")

        light_header, light_body = section(lines, "LIGHTS")
        check_count("LIGHTS", light_header, light_body, errors)
        light_ids: set[int] = set()
        for row, record in enumerate(light_body, start=1):
            fields = shlex.split(record)
            if len(fields) != 22:
                errors.append(f"LIGHTS row {row}: expected 22 fields, found {len(fields)}")
                continue
            light_id = int(fields[0])
            chamber_id = int(fields[2])
            radius = float(fields[5])
            intensity = float(fields[6])
            flicker = float(fields[18])
            speed = float(fields[19])
            radius_flicker = float(fields[20])
            position_x = float(fields[3])
            position_y = float(fields[4])
            placed_cell = world_cell(position_x, position_y)
            if placed_cell is None:
                errors.append(
                    f"LIGHTS row {row}: position is outside CELL_LAYOUT"
                )
            elif placed_cell[2] == 0:
                errors.append(
                    f"LIGHTS row {row}: position is on disabled cell "
                    f"({placed_cell[0]}, {placed_cell[1]})"
                )
            elif chamber_id >= 0 and placed_cell[3] != chamber_id:
                errors.append(
                    f"LIGHTS row {row}: chamber {chamber_id} does not match "
                    f"cell chamber {placed_cell[3]} at "
                    f"({placed_cell[0]}, {placed_cell[1]})"
                )
            if light_id in light_ids:
                errors.append(f"duplicate light ID {light_id}")
            light_ids.add(light_id)
            if chamber_id >= 0 and chamber_id not in chamber_ids:
                errors.append(f"LIGHTS row {row}: unknown chamber ID {chamber_id}")
            if radius <= 0.0:
                errors.append(f"LIGHTS row {row}: radius must be positive")
            if intensity < 0.0:
                errors.append(f"LIGHTS row {row}: intensity cannot be negative")
            if not 0.0 <= flicker <= 1.0 or not 0.0 <= radius_flicker <= 1.0:
                errors.append(f"LIGHTS row {row}: flicker amounts must be in 0..1")
            if speed < 0.0:
                errors.append(f"LIGHTS row {row}: flicker speed cannot be negative")

    except (ValueError, OSError) as exc:
        errors.append(str(exc))

    return errors


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("level", nargs="?", default="levels/level01.mox")
    args = parser.parse_args(argv)
    level = Path(args.level)
    if not level.is_absolute():
        level = (Path.cwd() / level).resolve()
    if not level.is_file():
        print(f"ERROR: level not found: {level}")
        return 1

    errors = validate(level)
    if errors:
        print(f"Validation failed for {level}:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"Level validation passed: {level}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
