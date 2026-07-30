# Dungeon environment tools

These scripts support the first atmosphere-authoring milestone. Run them from
inside the `Moxiang2D` project directory.

## Rebuild the environment sprites

```powershell
python tools/build_environment_assets.py
```

This crops reusable ornaments from the existing authored dungeon decal sheet,
creates the 4x4 animated azure-flame sheet, and refreshes
`Assets/environment/environment_manifest.json`.

## Reapply the sample atmosphere pass

```powershell
python tools/apply_level01_atmosphere.py
```

This deterministically upgrades `levels/level01.mox` to level format 11 and
adds the sample chamber styles, layered environment objects, and authored
lights. It is safe to rerun: the script replaces its own sections rather than
appending duplicates.

## Validate a level before building

```powershell
python tools/validate_level.py levels/level01.mox
```

The validator checks section counts, version-11 obstacle and light records,
chamber references, sprite-sheet grids, missing files, and exact filename
capitalization. Exact capitalization matters for the web build even when a
Windows desktop build appears to work.

`build_web.bat` runs this validation automatically before Emscripten compiles
the game.

## Render a chamber-planning preview

```powershell
python tools/render_level_plan.py levels/level01.mox
```

This writes `levels/level01.plan.png`, a debugging plan that shows chamber
boundaries, elevation changes, layered environment markers, and light radii.
It does not change the runtime camera or game perspective.

Pillow is required by the asset builder, validator, and preview renderer:

```powershell
python -m pip install Pillow
```
