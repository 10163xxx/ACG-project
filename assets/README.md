# Greenhouse Scene

The greenhouse scene uses two imported botanical assets:

- Anthurium Botany 01: central exhibition subject.
- Fern 02: foreground and background vegetation.

## Scene layout

- The anthurium is placed on the central exhibition plinth.
- Fern instances are placed in planters on both sides.
- The greenhouse architecture and lighting remain procedural.

## Asset locations

The original OBJ, MTL, Blender and texture files are retained under:

- `assets/anthurium_botany_01_2k.blend/`
- `assets/fern_02_2k.blend/`

The OBJ loader normalizes imported models to a requested world-space
height while preserving UVs, normals and material assignments.

See `assets/ASSET_CREDITS.md` for asset sources and licenses.

## Texture limitation

The current scene integration uses the MTL base colors.
The original JPG, PNG and EXR texture maps are retained but are not
yet connected to the renderer's PPM-only image texture loader.
Alpha cutouts and full PBR texture reproduction remain future work.