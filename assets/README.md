# Greenhouse Botanical Assets

The greenhouse scene uses the following imported botanical models:

- Anthurium Botany 01: central exhibition subject.
- Fern 02: foreground and background foliage.
- Pachira Aquatica 01: tall background focal plant.
- Calathea Orbifolia 01: broad-leaf specimen beside the walkway.
- Potted Split Leaf Monstera: foreground broad-leaf specimen.
- Shrub 03 and Shrub 04: dense rear understory planting.

## Scene layout

- Anthurium occupies the central exhibition area.
- Fern instances provide foreground and background depth.
- Pachira forms the tall rear focal silhouette.
- Calathea adds broad foliage beside the walkway.
- Monstera frames the foreground entrance area.
- Shrub 03 and Shrub 04 form overlapping low vegetation near the rear wall.

## Asset locations

Original OBJ, MTL and source texture files are retained under their
respective directories in `assets/`.

Directories whose names end in `.blend` are asset-package folders,
not standalone Blender project files.

See `assets/ASSET_CREDITS.md` for source URLs, creators, licenses
and local modifications.

## Renderer compatibility

The current OBJ/MTL pipeline supports PPM image textures.

The preparation script converts the texture maps required by the
renderer into P6 PPM while retaining the original JPG and PNG sources.

- Diffuse maps are loaded as RGB textures.
- Opacity maps are converted to PPM and used for leaf cutouts.
- Pachira additionally uses supported roughness and OpenGL normal maps.
- Shrub 03 and Shrub 04 use diffuse and alpha maps.
- Unsupported mask, AO and displacement maps remain archived source assets.

Run the following commands from the repository root:

    py -m pip install Pillow
    py scripts/prepare_botanical_assets.py

The preparation script converts textures, applies required coordinate
transforms and validates OBJ/MTL/texture references.