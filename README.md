# ACG Path Tracer

A CPU-based path tracer developed in **C++17** for the Advanced Computer Graphics (ACG) 2026 course.

The project is built from scratch as a modular Monte Carlo path tracer. Its goal is to produce a static image using ray-based light transport rather than rasterization.

## Implemented Features

### Rendering Core
- C++17 rendering framework
- Ray, vector, interval, and camera utilities
- Sphere intersection and surface hit records
- Sky-gradient background
- Iterative path tracing with configurable maximum depth in the integrator
- Stochastic subpixel sampling (anti-aliasing)
- PPM image output with display gamma correction

### Materials and Scene
- Lambertian diffuse reflection
- Metal reflection
- Dielectric transmission and refraction
- Procedural material-test scene

### Acceleration
- Axis-aligned bounding boxes (AABBs)
- **Binned Surface Area Heuristic (SAH) BVH**
- Small multi-primitive leaves selected using an SAH cost estimate
- Balanced median-split fallback for degenerate centroid distributions

### Testing and Build
- CMake-based build configuration
- Unit tests for materials, geometry, and BVH hit-result consistency

## Project Structure

```text
ACG-project/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── core/           # Vectors, rays, intervals, random sampling
│   ├── geometry/       # AABBs, hittable interface, primitives
│   ├── camera/         # Camera and ray generation
│   ├── materials/      # Surface scattering models
│   ├── sampling/       # Sampling and PDF utilities
│   ├── acceleration/   # Acceleration structures (SAH BVH)
│   ├── integrator/     # Path tracing integrator
│   ├── scene/          # Scene definitions
│   └── output/         # Image output and output paths
├── tests/              # Unit tests
├── results/            # Rendered images and test outputs
└── assets/             # Additional project assets
```

## Build and Run on Windows

Requirements: CMake 3.20+, a C++17-compatible compiler, and a supported build toolchain such as Visual Studio.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\path_tracer.exe
```

The renderer currently writes a PPM image to the output path configured in `src/main.cpp` (currently `results/renders/material_test.ppm`). Use an image viewer that supports PPM, or convert the file to PNG for easier viewing.

## Course Requirements and Roadmap

The course requires a path-tracing renderer that produces a static image. The current implementation covers the path-tracing foundation, diffuse/specular/transmissive materials, procedural scene creation, AABB/BVH acceleration, and stochastic anti-aliasing. The following are planned extensions and are **not yet claimed as implemented**:

- Benchmark linear traversal against SAH BVH and record build/render time and primitive-test counts
- Aesthetic final scene with clearly documented original or externally sourced assets
- Advanced sampling: importance sampling, Russian Roulette, and multiple importance sampling (MIS)
- A non-trivial custom material, such as Principled BSDF or a layered material
- Procedural color texture with explicit texture-coordinate mapping; potentially normal mapping
- Area-light sampling and HDR environment lighting
- Optional effects such as depth of field, motion blur, or volume rendering, depending on time

## Validation Notes

`tests/test_bvh.cpp` compares hit/miss results and nearest-hit distances between linear `HittableList` traversal and the SAH BVH over hit, miss, overlapping-geometry, and axis-parallel ray cases. Passing unit tests validates correctness for these test cases; performance improvements must be demonstrated separately with a reproducible benchmark.

## License

No license has been specified yet. Unless a license is added to this repository, all rights remain with the copyright holder.
