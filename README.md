# ACG Path Tracer

A CPU-based path tracer developed in **C++17** for the Advanced Computer Graphics (ACG) 2026 course.

The project implements a modular Monte Carlo path tracer with ray-based light transport, procedural textures, normal mapping, and a BVH acceleration structure using the binned Surface Area Heuristic (SAH).

## Implemented Features

### Rendering Core
- C++17 rendering framework
- Rays, vectors, camera utilities, and surface intersection records
- Sphere geometry and sky-gradient background
- Iterative path tracing with configurable maximum path depth
- Stochastic subpixel sampling for anti-aliasing
- Gamma correction and PPM image output

### Materials and Texture Mapping
- Lambertian diffuse reflection
- Metal reflection
- Dielectric transmission and refraction
- Solid-color textures
- UV-coordinate checker textures
- UV-coordinate gradient textures
- Procedural 3D checker textures
- Procedural UV-based normal textures
- Normal-mapped Lambertian material with tangent-space to world-space normal transformation
- A material-test scene demonstrating diffuse, reflective, transmissive, textured, and normal-mapped surfaces

### Acceleration Structure
- Axis-aligned bounding boxes (AABBs)
- **Binned SAH BVH**
- Small multi-primitive leaves selected using an SAH cost estimate
- Median-split fallback for degenerate centroid distributions
- BVH correctness tests and a benchmark comparing linear traversal with BVH traversal

### Testing
The project includes CMake-based test targets for:
- Material behavior
- Geometry intersections
- BVH hit-result consistency
- Texture evaluation and normal-mapping behavior

## Project Structure

```text
ACG-project/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── core/           # Vectors, rays, intervals, random sampling
│   ├── geometry/       # Geometry, AABBs, hittable interfaces
│   ├── camera/         # Camera and ray generation
│   ├── materials/      # Materials, procedural textures, normal mapping
│   ├── sampling/       # Sampling and PDF utilities
│   ├── acceleration/   # BVH and SAH acceleration
│   ├── integrator/     # Path tracing integrator
│   ├── scene/          # Test scene definitions
│   └── output/         # Image output and output paths
├── tests/              # Unit tests and BVH benchmark
├── scripts/            # PPM-to-JPG conversion and render automation
├── results/
│   └── renders/        # Rendered image outputs
└── assets/             # Additional project assets
```

## Build and Test on Windows

### Requirements
- CMake 3.20 or later
- A C++17-compatible compiler
- A supported build toolchain, such as Visual Studio
- Python 3 and Pillow for automatic JPG conversion

### Configure and Build

Run these commands from the project root in PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

### Run Unit Tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

A successful test run should report that