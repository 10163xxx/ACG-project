# Asset Credits

This file records the provenance and licenses of external assets.

Do not assume that an asset is free to redistribute without checking
its original source and license.

| Asset | Source / Author | License | Notes |
|---|---|---|---|
| studio_small_01_2k.hdr | To be verified | To be verified | Existing environment map |
| test_model.obj | Project test asset; authorship to be confirmed | To be confirmed | OBJ loader test |
| checker.ppm | To be verified | To be verified | Test texture |
| normal.ppm | To be verified | To be verified | Test normal map |
| bump_test.ppm | To be verified | To be verified | Mipmap test texture |
| uv_test.obj | Project test asset; authorship to be confirmed | To be confirmed | UV mapping test |

For every new external asset, record:

- Asset name and local path
- Original download URL
- Author or creator
- License and redistribution requirements
- Any modifications made to the original asset

## External Model Sources

### Anthurium Botany 01
- Source: https://polyhaven.com/a/anthurium_botany_01
- Authors: Rico Cilliers (modeling), Rob Tuytel (scanning)
- License: CC0 1.0 Universal
- Original files: OBJ, MTL, Blender project and PBR textures
- Modifications: Imported into the ACG Path Tracer scene.

### Fern 02
- Source: https://polyhaven.com/a/fern_02
- Authors: Rico Cilliers (modeling), Rob Tuytel (scanning)
- License: CC0 1.0 Universal
- Original files: OBJ, MTL, Blender project and PBR textures
- Modifications: Imported into the ACG Path Tracer scene.

### Kloofendal 48d Partly Cloudy (Pure Sky)
- Source: https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky
- Authors: Greg Zaal (original), Jarod Guest (sky edits)
- License: CC0 1.0 Universal
- Local path: assets/environments/kloofendal_48d_partly_cloudy_puresky_2k.hdr
- Format: Radiance HDR, equirectangular environment map
- Modifications: Downloaded in HDR format for environment lighting.

### Calathea Orbifolia 01
- Source: https://polyhaven.com/a/calathea_orbifolia_01
- Authors: Rico Cilliers (modeling), Rob Tuytel (scanning)
- License: CC0 1.0 Universal
- Original files: OBJ, MTL, Blender project and PBR textures
- Modifications: Diffuse and alpha textures converted to P6 PPM for the renderer; MTL updated for diffuse, opacity and supported roughness maps; selected plant components imported into the greenhouse scene.

### Pachira Aquatica 01
- Source: https://polyhaven.com/a/pachira_aquatica_01
- Authors: Rico Cilliers (modeling), Rob Tuytel (scanning)
- License: CC0 1.0 Universal
- Original files: OBJ, MTL, Blender project and PBR textures
- Modifications: Selected one tree from the multi-tree OBJ; diffuse, alpha and roughness textures converted to P6 PPM; MTL updated for the renderer; imported into the greenhouse scene.

### Shrub 03
- Source: https://polyhaven.com/a/shrub_03
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/shrub_03_2k.blend/
- Original files: OBJ, MTL and PBR textures
- Modifications: Diffuse and alpha textures converted to P6 PPM for the renderer. The source geometry is retained in its original Y-up orientation.

### Shrub 04
- Source: https://polyhaven.com/a/shrub_04
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/shrub_04_2k.blend/
- Original files: OBJ, MTL and PBR textures, including the original mask map
- Modifications: Diffuse and alpha textures converted to P6 PPM for the renderer. The original mask map is retained but is not currently used by the renderer. The source geometry is retained in its original Y-up orientation.

### Potted Split Leaf Monstera
- Source: https://www.innerscene.com/tools/library/3d-parts/potted-split-leaf-monstera-c29ea101
- Creator / source: Innerscene (source OBJ metadata identifies Innerscene Corp.)
- License: CC0 1.0 Universal
- Local paths:
  - assets/potted-split-leaf-monstera.obj
  - assets/monstera.mtl
- Original files: OBJ geometry
- Modifications: Source geometry converted from Z-up to Y-up by the asset preparation script. A project-authored MTL supplies simple pot, soil, stem and foliage materials because the source OBJ has no accompanying texture package.

### Moss 01
- Source: https://polyhaven.com/a/moss_01
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/moss_01_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and alpha textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Rock Moss Set 01
- Source: https://polyhaven.com/a/rock_moss_set_01
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/rock_moss_set_01_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py; diffuse and roughness textures converted to P6 PPM for the renderer; MTL rewritten for the renderer; the normal map is not currently used. The Blender project itself is kept locally and not committed.

### Shrub 01
- Source: https://polyhaven.com/a/shrub_01
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/shrub_01_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and alpha textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Periwinkle Plant
- Source: https://polyhaven.com/a/periwinkle_plant
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/periwinkle_plant_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and opacity textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Shrub 02
- Source: https://polyhaven.com/a/shrub_02
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/shrub_02_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and alpha textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Nettle Plant
- Source: https://polyhaven.com/a/nettle_plant
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/nettle_plant_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and alpha textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Weed Plant 02
- Source: https://polyhaven.com/a/weed_plant_02
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/weed_plant_02_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD0 objects only); diffuse and alpha textures converted to P6 PPM for the renderer; MTL rewritten for the renderer. The Blender project itself is kept locally and not committed.

### Island Tree 02
- Source: https://polyhaven.com/a/island_tree_02
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/island_tree_02_2k.blend/
- Original files: Blender project with PBR textures
- Modifications: Mesh objects exported to OBJ with the headless Blender script tools/export_blend_obj.py (LOD1 objects only, keeping the file under the repository size limit); trunk, branch and leaf textures converted to P6 PPM for the renderer (leaf alpha as opacity mask); MTL rewritten for the renderer; used as exterior background trees. The Blender project itself is kept locally and not committed.

### Rainforest Trail (environment)
- Source: https://polyhaven.com/a/rainforest_trail
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/environments/rainforest_trail_2k.hdr
- Format: Radiance HDR, equirectangular environment map
- Modifications: none; replaces the previous partly-cloudy sky as the scene environment.

### Mossy Forest (environment, backup)
- Source: https://polyhaven.com/a/mossy_forest
- Author: Rico Cilliers
- License: CC0 1.0 Universal
- Local path: assets/environments/mossy_forest_2k.hdr
- Format: Radiance HDR, equirectangular environment map
- Modifications: none; kept as an alternative environment.
