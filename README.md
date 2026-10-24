
# ACG Path Tracer

A CPU-based path tracer developed in C++17 for the Advanced Computer Graphics (ACG) 2026 course.

The project focuses on physically based light transport, material evaluation, texture mapping, geometric acceleration, and rendering scenes from external OBJ/MTL assets.

## Implemented Features

### Rendering and Light Transport

- CPU-based path tracing for static image rendering
- Iterative path tracing with configurable maximum path depth
- Monte Carlo sampling for anti-aliasing and light transport
- Direct lighting and multiple importance sampling (MIS)
- Gamma correction and PPM image output
- Automated PPM-to-JPG conversion

### Geometry and Acceleration

- Ray and surface intersection infrastructure
- Sphere and triangle-based mesh geometry
- Axis-aligned bounding boxes (AABBs)
- Bounding Volume Hierarchy (BVH) acceleration
- Binned Surface Area Heuristic (SAH) for BVH construction
- Small multi-primitive leaves and median-split fallback

### Materials and Textures

- Lambertian diffuse reflection
- Metal reflection
- Dielectric transmission and refraction
- Principled BSDF material support
- Emissive materials and area lights
- Solid-color and procedural textures
- UV-coordinate checker and gradient textures
- Procedural UV-based normal textures
- Tangent-space normal mapping with transformation into world space
- Texture filtering and mipmap support

### External Assets and Lighting

- OBJ mesh loading
- MTL material loading
- Imported mesh materials and textures
- Environment-map lighting
- Support for rendering scenes assembled from external assets

### Testing

The project includes automated tests covering implemented components, including:

- Material behavior and BSDF evaluation
- Geometry intersections
- Texture evaluation and normal mapping
- BVH correctness
- Area-light behavior
- Mixed-scene integration
- OBJ/MTL loading and integration

## Project Structure

```text
ACG-project/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── core/           # Vectors, rays, intervals and sampling utilities
│   ├── geometry/       # Geometry, AABBs and intersection interfaces
│   ├── camera/         # Camera and ray generation
│   ├── materials/      # Materials, BSDFs and textures
│   ├── sampling/       # Sampling and PDF utilities
│   ├── acceleration/   # BVH and SAH acceleration
│   ├── integrator/     # Path tracing and light transport
│   ├── scene/          # Scene definitions and asset integration
│   └── output/         # Image output utilities
├── tests/              # Tests and benchmarks
├── scripts/
│   ├── render.ps1      # Rendering automation
│   └── convert_ppm_to_jpg.py
├── assets/
│   └── models/         # External models and related assets
└── results/
    └── renders/        # Rendered images
```

## Requirements

- CMake 3.20 or later
- A C++17-compatible compiler
- A supported build toolchain, such as Visual Studio on Windows
- Python 3
- Pillow for JPG conversion

## Build on Windows

Run the following commands from the project root in PowerShell.

Configure the project:

```powershell
cmake -S . -B build
```

Build the Release configuration:

```powershell
cmake --build build --config Release
```

## Render Scenes

The project provides a PowerShell script to automate rendering and JPG conversion.

### OBJ Scene

Run:

```powershell
.\scripts\render.ps1 -Config Release -Scene OBJ
```

By default, the script loads:

```text
assets/models/test_model.obj
```

The OBJ file and its referenced MTL files and textures must be available at the expected paths.

The default output files are:

```text
results/renders/obj_scene.ppm
results/renders/obj_scene.jpg
```

The script renders the scene and then converts the PPM image to JPG.

### Other Scenes

Render the showcase scene:

```powershell
.\scripts\render.ps1 -Config Release -Scene Showcase
```

Render the UV test scene:

```powershell
.\scripts\render.ps1 -Config Release -Scene UVTest
```

Render the volume test scene:

```powershell
.\scripts\render.ps1 -Config Release -Scene VolumeTest
```

To build before rendering, add `-Build`:

```powershell
.\scripts\render.ps1 -Config Release -Scene OBJ -Build
```

### Specify a Different OBJ File

Use `-ObjPath` to select a different model relative to the project root:

```powershell
.\scripts\render.ps1 -Config Release -Scene OBJ -ObjPath "assets\models\my_model.obj"
```

## Run Tests

Run the test suite using CTest:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

To build the test targets first:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Development Workflow

1. Implement or refine a rendering feature.
2. Build the Release configuration.
3. Run the relevant automated tests.
4. Render a representative scene using `scripts/render.ps1`.
5. Inspect the generated image to evaluate visual correctness.
6. Update the documentation when functionality or workflows change.

The project prioritizes correctness of rendering algorithms, physically based material behavior, and reliable integration of external scene assets.
