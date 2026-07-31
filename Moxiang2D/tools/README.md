# Optional dungeon authoring tools

The files in this directory are **development utilities only**. The desktop
game, web game, renderer, Build Mode editor and `.mox` loader are C++. Python
is not required to play the generated level.

Python is used here because a large authored `.mox` grid contains thousands of
terrain, wall, chamber, decal and prop values. The scripts make that data
reproducible and catch broken ramps, missing assets and invalid decal anchors
before a build.

`build_web.bat` runs the validator when Python is available. If Python is not
installed, validation is skipped and compilation continues.

## Rebuild the linear dungeon

From the `Moxiang2D` project folder:

```powershell
python tools/rebuild_level01_dungeon.py
```

The generator writes `levels/level01.mox` as a six-chamber, south-to-north
progression:

1. Gate of Returning Ash
2. Ember Oath Hall
3. Sunken Dragon Court
4. Flooded Reliquary
5. Azure Ritual Sanctum
6. Throne of the Iron Warden

The south edge is the level entrance. Each north doorway leads directly to the
next chamber. The boss arena is the largest chamber and has the widest camera
framing.

The generated level includes:

- raised-terrain chamber walls and individually textured cliff faces
- correct lower-cell-to-level+1 directional ramps
- varied floor materials and dedicated wall materials
- flat gates at chamber entrances
- terrain-aware large floor decals
- threshold and platform trim decals
- 80 perspective-correct generated props throughout chamber interiors and edges
- foreground framing objects
- warm and cool flickering light rigs
- per-chamber ambient color, vignette, camera zoom and focus offsets

The game loads the generated `.mox` directly and never calls this script at
runtime.


## Extract the generated prop sheets

Five 4x4 source sheets are stored under
`Assets/environment/generated_sheets/`. To rebuild the individual runtime PNGs:

```powershell
python tools/extract_generated_prop_sheets.py
```

This writes 80 transparent props to `Assets/environment/generated/` and creates
a manifest. The level generator uses every extracted prop at least once. The
source sheets are retained so the extraction is deterministic and can be
repeated after replacing a sheet with improved artwork.

The generated sheets are used for upright structural, ritual, storage, focal
ornament and debris props. Flat floor decals, gates, wall details and foreground
framing continue to use the existing projection-specific authored assets.

## Build Mode decal workflow

Build Mode now has dedicated tools:

- **Place Floor Decal**
- **Erase Floor Decal**
- **Move / Select Floor Decal**

A decal can be adjusted after placement:

- position and visual size
- rotation
- tint and opacity
- regular sprite-sheet source cell
- exact pixel source rectangle for irregular sheets
- captured terrain elevation
- whether it may appear on ramps
- whether it is clipped to its placement elevation

With **Clip To Placement Elevation** enabled, the renderer divides the decal by
terrain cells and rejects pieces that belong to another elevation, another
chamber or a ramp. This prevents a large emblem from floating across raised
platforms or cliff edges. Large decals still render normally when their size
fits entirely on one flat platform.

The **Authored Decal Library** in Build Mode provides direct presets for the
extracted medallions, symbols, doorway thresholds and long trim strips under
`Assets/environment/authored/`.

## Chamber camera framing

Every chamber has two Build Mode controls:

- **Chamber Camera Zoom**
- **Camera Focus Offset**

The gameplay camera interpolates gradually when the active chamber changes.
The Hybrid 3D camera samples vertical terrain height from the player rather than
from the focus-shifted camera target, preventing raised north walls from causing
vertical jitter while the player pushes against collision.
Smaller zoom values show more of a large arena; larger values bring the camera
closer in compact rooms. Build Mode retains manual camera control.

## Validate the level

```powershell
python tools/validate_level.py levels/level01.mox
```

The validator checks:

- section dimensions and record counts
- exact asset filename capitalization for web builds
- atlas dimensions and tile-index ranges
- version 13 chamber camera metadata
- version 13 obstacle, visual-offset and collider-offset metadata
- decal source rectangles and anchor elevations
- obstacle and light placement
- every ramp direction and level+1 destination
- directional stair-tile assignment
- reachability of every chamber through the terrain/ramp network
- collision-aware progression checks so props cannot seal a connector or exit

Pillow is required for image-dimension validation and for regenerating authored
asset sizes:

```powershell
python -m pip install Pillow
```

## Render a planning overview

```powershell
python tools/render_level_plan.py levels/level01.mox
```

This creates `levels/level01.plan.png`. Cyan arrows show ramps, yellow circles
show light coverage, outlined rectangles show decal coverage and colored dots
show prop layers. Chamber labels include the authored camera zoom. The image is
a top-down debugging diagram and does not change the game's perspective.

## Dungeon master atlas

`Assets/tiles/dungeon_master_atlas.png` uses an 8x8 grid:

- tiles `0-43`: varied floor materials
- tiles `44-47`: North, East, South and West stair/ramp materials
- tiles `48-63`: dedicated wall and cliff materials

Floor textures, directional stairs and wall faces share one runtime atlas while
remaining separate material ranges, preventing floor artwork from being
stretched across vertical walls.
