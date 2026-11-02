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