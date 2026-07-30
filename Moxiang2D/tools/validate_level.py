#!/usr/bin/env python3
"""Validate .mox level structure and referenced runtime assets.

This catches the most common desktop/web mismatch before compilation:
missing files, case-only path errors, invalid atlas grids, malformed v11
obstacles, broken animation metadata and duplicate light IDs.
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
            if len(fields) != 6:
                errors.append(f"CHAMBER_STYLES malformed: {record}")
                continue
            chamber_id = int(fields[0])
            styled_ids.add(chamber_id)
            rgba = [int(value) for value in fields[1:5]]
            vignette = float(fields[5])
            if any(value < 0 or value > 255 for value in rgba):
                errors.append(f"chamber {chamber_id}: ambient RGBA outside 0..255")
            if not 0.0 <= vignette <= 0.85:
                errors.append(f"chamber {chamber_id}: vignette outside 0..0.85")
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
        for row, record in enumerate(obstacle_body, start=1):
            fields = shlex.split(record)
            if len(fields) != 28:
                errors.append(f"OBSTACLES row {row}: expected 28 fields, found {len(fields)}")
                continue
            layer = int(fields[8])
            mode = int(fields[9])
            opacity = float(fields[14])
            animated = int(fields[15]) != 0
            columns = int(fields[16])
            rows = int(fields[17])
            frame_count = int(fields[18])
            fps = float(fields[19])
            asset = fields[21]
            position_x = float(fields[0])
            position_y = float(fields[1])
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
            if animated and fps <= 0.0:
                errors.append(f"OBSTACLES row {row}: animated prop requires positive FPS")
            if asset:
                asset_path = exact_path(project, asset)
                if asset_path is None or not asset_path.is_file():
                    errors.append(f"OBSTACLES row {row}: missing/case-mismatched asset {asset}")
                else:
                    with Image.open(asset_path) as image:
                        if image.width % columns or image.height % rows:
                            errors.append(
                                f"OBSTACLES row {row}: {asset} size {image.width}x{image.height} "
                                f"is not divisible by {columns}x{rows}"
                            )

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
