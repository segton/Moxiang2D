#!/usr/bin/env python3
"""Render a top-down authoring plan from a .mox level.

This is a debugging view, not the game's camera or final art. It makes chamber
boundaries, elevations, environment props and light coverage easy to inspect
without launching the editor.
"""
from __future__ import annotations

import argparse
import math
import shlex
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw

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


def chamber_color(chamber_id: int) -> tuple[int, int, int]:
    palette = [
        (68, 74, 88),
        (43, 97, 112),
        (111, 76, 44),
        (78, 63, 101),
        (55, 94, 72),
        (104, 58, 68),
    ]
    return palette[chamber_id % len(palette)] if chamber_id >= 0 else (24, 27, 34)


def render(level: Path, output: Path, cell_pixels: int = 18) -> None:
    lines = level.read_text(encoding="utf-8").splitlines()

    cell_header, cell_body = section(lines, "CELL_LAYOUT")
    width = int(cell_header[1])
    height = int(cell_header[2])

    cells: list[list[tuple[int, int]]] = []
    for record in cell_body:
        values = [int(value) for value in shlex.split(record)]
        cells.append([
            (values[column * 2], values[column * 2 + 1])
            for column in range(width)
        ])

    _, elevation_body = section(lines, "TERRAIN_ELEVATIONS")
    elevations = [
        [int(value) for value in shlex.split(record)]
        for record in elevation_body
    ]

    _, ramp_body = section(lines, "TERRAIN_RAMPS")
    ramps = [
        [int(value) for value in shlex.split(record)]
        for record in ramp_body
    ]

    _, chamber_body = section(lines, "CHAMBERS")
    chamber_names = {
        int(fields[0]): fields[1]
        for record in chamber_body
        if len(fields := shlex.split(record)) == 2
    }

    _, style_body = section(lines, "CHAMBER_STYLES")
    chamber_zoom = {}
    for record in style_body:
        fields = shlex.split(record)
        if len(fields) >= 7:
            chamber_zoom[int(fields[0])] = float(fields[6])

    margin = 48
    canvas = Image.new(
        "RGB",
        (width * cell_pixels + margin * 2, height * cell_pixels + margin * 2),
        (8, 10, 14),
    )
    draw = ImageDraw.Draw(canvas)

    for y, row in enumerate(cells):
        for x, (enabled, chamber_id) in enumerate(row):
            if not enabled:
                continue

            elevation = elevations[y][x]
            base = chamber_color(chamber_id)
            fill = tuple(min(255, value + elevation * 11) for value in base)
            left = margin + x * cell_pixels
            top = margin + y * cell_pixels
            right = left + cell_pixels - 1
            bottom = top + cell_pixels - 1
            draw.rectangle((left, top, right, bottom), fill=fill)

            neighbours = (
                (-1, 0, (left, top, left, bottom)),
                (1, 0, (right, top, right, bottom)),
                (0, -1, (left, top, right, top)),
                (0, 1, (left, bottom, right, bottom)),
            )
            for delta_x, delta_y, edge in neighbours:
                neighbour_x = x + delta_x
                neighbour_y = y + delta_y
                outside = not (
                    0 <= neighbour_x < width and
                    0 <= neighbour_y < height
                )
                disabled = (
                    not outside and
                    cells[neighbour_y][neighbour_x][0] == 0
                )
                if outside or disabled:
                    draw.line(edge, fill=(176, 155, 118), width=1)

    ramp_offsets = {
        1: (0, -1),
        2: (1, 0),
        3: (0, 1),
        4: (-1, 0),
    }
    for y, row in enumerate(ramps):
        for x, direction in enumerate(row):
            if direction not in ramp_offsets:
                continue
            dx, dy = ramp_offsets[direction]
            center_x = margin + (x + 0.5) * cell_pixels
            center_y = margin + (y + 0.5) * cell_pixels
            end_x = center_x + dx * cell_pixels * 0.34
            end_y = center_y + dy * cell_pixels * 0.34
            draw.line(
                (center_x, center_y, end_x, end_y),
                fill=(96, 225, 244),
                width=max(2, cell_pixels // 7),
            )
            perpendicular_x = -dy
            perpendicular_y = dx
            arrow_size = cell_pixels * 0.16
            draw.polygon(
                [
                    (end_x, end_y),
                    (
                        end_x - dx * arrow_size + perpendicular_x * arrow_size,
                        end_y - dy * arrow_size + perpendicular_y * arrow_size,
                    ),
                    (
                        end_x - dx * arrow_size - perpendicular_x * arrow_size,
                        end_y - dy * arrow_size - perpendicular_y * arrow_size,
                    ),
                ],
                fill=(96, 225, 244),
            )

    for chamber_id, name in chamber_names.items():
        positions = [
            (x, y)
            for y, row in enumerate(cells)
            for x, (enabled, candidate_id) in enumerate(row)
            if enabled and candidate_id == chamber_id
        ]
        if not positions:
            continue
        center_x = sum(x for x, _ in positions) / len(positions)
        center_y = sum(y for _, y in positions) / len(positions)
        draw.text(
            (
                margin + center_x * cell_pixels - 54,
                margin + center_y * cell_pixels - 8,
            ),
            f"{chamber_id}: {name}  zoom {chamber_zoom.get(chamber_id, 1.5):.2f}",
            fill=(245, 245, 245),
        )

    origin_x = -width * 64.0 * 0.5
    origin_y = -height * 64.0 * 0.5

    def plan_position(world_x: float, world_y: float) -> tuple[float, float]:
        return (
            margin + ((world_x - origin_x) / 64.0) * cell_pixels,
            margin + ((world_y - origin_y) / 64.0) * cell_pixels,
        )

    _, obstacle_body = section(lines, "OBSTACLES")
    for record in obstacle_body:
        fields = shlex.split(record)
        world_x = float(fields[0])
        world_y = float(fields[1])
        world_w = max(8.0, float(fields[2]))
        world_h = max(8.0, float(fields[3]))
        layer = int(fields[8])
        mode = int(fields[9])
        rotation = float(fields[26]) if len(fields) >= 37 else 0.0
        x, y = plan_position(world_x, world_y)
        half_w = world_w / 64.0 * cell_pixels * 0.5
        half_h = world_h / 64.0 * cell_pixels * 0.5
        if mode == 1:
            color = (235, 235, 235)
            radians = math.radians(rotation)
            cosine = math.cos(radians)
            sine = math.sin(radians)
            corners = []
            for local_x, local_y in ((-half_w, -half_h), (half_w, -half_h), (half_w, half_h), (-half_w, half_h)):
                corners.append((
                    x + local_x * cosine - local_y * sine,
                    y + local_x * sine + local_y * cosine,
                ))
            draw.polygon(corners, outline=color)
        else:
            if layer == 0:
                color = (255, 174, 76)
            elif layer == 2:
                color = (225, 104, 220)
            else:
                color = (116, 220, 130)
            draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)

    _, light_body = section(lines, "LIGHTS")
    for record in light_body:
        fields = shlex.split(record)
        world_x = float(fields[3])
        world_y = float(fields[4])
        radius = float(fields[5])
        x, y = plan_position(world_x, world_y)
        plan_radius = radius / 64.0 * cell_pixels
        draw.ellipse(
            (
                x - plan_radius,
                y - plan_radius,
                x + plan_radius,
                y + plan_radius,
            ),
            outline=(255, 207, 94),
            width=1,
        )
        draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(255, 231, 148))

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output)
    print(f"Rendered level plan: {output}")


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("level", nargs="?", default="levels/level01.mox")
    parser.add_argument("--output", "-o")
    parser.add_argument("--cell-pixels", type=int, default=18)
    args = parser.parse_args(argv)

    level = Path(args.level).resolve()
    output = (
        Path(args.output).resolve()
        if args.output
        else level.with_suffix(".plan.png")
    )
    render(level, output, max(6, args.cell_pixels))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
