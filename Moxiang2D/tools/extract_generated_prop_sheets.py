#!/usr/bin/env python3
"""Extract the five generated 4x4 dungeon prop sheets into individual PNGs.

This is an optional authoring tool. The game loads the extracted PNG files at
runtime and does not require Python. Each 1024x1024 source sheet is divided into
sixteen 256x256 cells, trimmed by alpha, padded, and saved under
Assets/environment/generated/.
"""
from __future__ import annotations

import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "Assets" / "environment" / "generated_sheets"
OUTPUT_ROOT = ROOT / "Assets" / "environment" / "generated"

SHEETS: dict[str, list[str]] = {
    "structural_obstacles_4x4.png": [
        "structural_short_square_pillar",
        "structural_damaged_square_pillar",
        "structural_short_round_column",
        "structural_broken_round_column",
        "structural_low_rect_pedestal",
        "structural_low_square_pedestal",
        "structural_dragon_support_block",
        "structural_boundary_post",
        "structural_collapsed_pillar_base",
        "structural_half_cracked_column",
        "structural_carved_cover_block",
        "structural_broken_support",
        "structural_low_wall_fragment",
        "structural_damaged_wall_fragment",
        "structural_guardian_lion_pedestal",
        "structural_brazier_pedestal",
    ],
    "ritual_props_4x4.png": [
        "ritual_amber_brazier",
        "ritual_cyan_brazier",
        "ritual_extinguished_brazier",
        "ritual_incense_altar",
        "ritual_offering_table",
        "ritual_seal_pedestal",
        "ritual_rune_pedestal",
        "ritual_gong_stand",
        "ritual_chained_post",
        "ritual_incense_burner",
        "ritual_stone_basin",
        "ritual_cracked_platform",
        "ritual_candle_cluster",
        "ritual_sealed_relic",
        "ritual_damaged_shrine_base",
        "ritual_spirit_lantern",
    ],
    "storage_props_4x4.png": [
        "storage_chest_plain_closed",
        "storage_chest_plain_open",
        "storage_chest_bronze_closed",
        "storage_chest_bronze_open",
        "storage_chest_ceremonial",
        "storage_chest_broken",
        "storage_large_urn",
        "storage_small_jar",
        "storage_cracked_urn",
        "storage_shattered_urn",
        "storage_crate",
        "storage_reinforced_crate",
        "storage_damaged_crate",
        "storage_collapsed_crate",
        "storage_scroll_bundle",
        "storage_supply_basket",
    ],
    "chamber_ornaments_4x4.png": [
        "ornament_dragon_relief_monument",
        "ornament_circular_ritual_altar",
        "ornament_rectangular_ceremonial_altar",
        "ornament_guardian_statue",
        "ornament_sword_shrine",
        "ornament_ancient_bell",
        "ornament_rune_obelisk",
        "ornament_damaged_rune_obelisk",
        "ornament_chained_relic",
        "ornament_spiritual_fountain",
        "ornament_small_sarcophagus",
        "ornament_broken_monument",
        "ornament_incense_shrine",
        "ornament_prison_memorial",
        "ornament_boss_emblem_pedestal",
        "ornament_damaged_dragon_altar",
    ],
    "debris_props_4x4.png": [
        "debris_loose_stones",
        "debris_rubble_pile",
        "debris_broken_floor_slab",
        "debris_wall_bricks",
        "debris_pillar_fragments",
        "debris_ceramic_fragments",
        "debris_wooden_boards",
        "debris_broken_crate",
        "debris_fallen_plaque",
        "debris_tablet_fragment",
        "debris_chain_coil",
        "debris_bones_rubble",
        "debris_torn_cloth_stones",
        "debris_weapon_fragments",
        "debris_ash_pile",
        "debris_moss_stones",
    ],
}


def trim_cell(cell: Image.Image, padding: int = 6) -> Image.Image:
    alpha = cell.getchannel("A")
    bbox = alpha.point(lambda value: 255 if value > 3 else 0).getbbox()
    if bbox is None:
        return cell
    left, top, right, bottom = bbox
    left = max(0, left - padding)
    top = max(0, top - padding)
    right = min(cell.width, right + padding)
    bottom = min(cell.height, bottom + padding)
    return cell.crop((left, top, right, bottom))


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    manifest: list[dict[str, object]] = []

    for sheet_name, asset_names in SHEETS.items():
        source_path = SOURCE_ROOT / sheet_name
        if not source_path.exists():
            raise FileNotFoundError(source_path)

        with Image.open(source_path).convert("RGBA") as sheet:
            if sheet.width % 4 or sheet.height % 4:
                raise ValueError(f"{sheet_name} must divide evenly into 4x4 cells")
            cell_width = sheet.width // 4
            cell_height = sheet.height // 4

            for index, asset_name in enumerate(asset_names):
                column = index % 4
                row = index // 4
                box = (
                    column * cell_width,
                    row * cell_height,
                    (column + 1) * cell_width,
                    (row + 1) * cell_height,
                )
                extracted = trim_cell(sheet.crop(box))
                output_path = OUTPUT_ROOT / f"{asset_name}.png"
                extracted.save(output_path, optimize=True)
                manifest.append({
                    "name": asset_name,
                    "source": sheet_name,
                    "cell": [column, row],
                    "path": output_path.relative_to(ROOT).as_posix(),
                    "size": [extracted.width, extracted.height],
                })

    (OUTPUT_ROOT / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Extracted {len(manifest)} generated props to {OUTPUT_ROOT}")


if __name__ == "__main__":
    main()
