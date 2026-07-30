# Optional dungeon authoring tools

The files in this directory are **development utilities only**. The desktop
game, web game, renderer, editor, and `.mox` loader are C++ and do not run
Python. A generated `level01.mox` and all required PNG assets are already
included in the repository.

Python is useful here because large `.mox` grids are difficult to edit safely
by hand. The scripts make the authored layout reproducible, check every ramp
connection, catch missing web assets and render a planning overview. They can
be deleted from a packaged game build without affecting the game.

`build_web.bat` runs the validator when Python is available, but validation is
now optional and the build continues without Python.

## Rebuild the authored dungeon

```powershell
python tools/rebuild_level01_dungeon.py
```

This regenerates the eight-chamber `levels/level01.mox` layout, including:

- chamber ownership and elevations
- correctly oriented lower-cell-to-level+1 ramps
- dedicated directional stair tiles
- dedicated wall materials
- floor variation
- pillars, rubble, chests, urns and floor decals
- animated environment flames and authored lights

The game loads the resulting `.mox` file directly; it does not call this script.

## Validate the level

```powershell
python tools/validate_level.py levels/level01.mox
```

The validator checks:

- section dimensions and record counts
- exact asset filename capitalization for web builds
- atlas dimensions and tile-index ranges
- obstacle and light placement
- every ramp's direction and level+1 destination
- directional stair-tile assignment
- reachability of every chamber through the terrain/ramp network

Pillow is required only for validation of image dimensions:

```powershell
python -m pip install Pillow
```

## Render a planning overview

```powershell
python tools/render_level_plan.py levels/level01.mox
```

This creates `levels/level01.plan.png`. Cyan arrows show authored ramp
directions, yellow circles show light coverage, and colored dots show layered
environment objects. It is a top-down debugging diagram and does not change
the game's camera or perspective.

## Dungeon master atlas

`Assets/tiles/dungeon_master_atlas.png` uses an 8x8 grid:

- tiles `0-43`: varied floor materials
- tiles `44-47`: North, East, South and West stair/ramp materials
- tiles `48-63`: dedicated wall and cliff materials

Keeping floors, directional stairs and walls in one atlas works with the
existing terrain renderer while preventing floor textures from being stretched
across vertical walls.
