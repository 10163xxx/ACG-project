# ACG Path Tracer

A CPU-based path tracer developed in **C++17** for the Advanced Computer Graphics (ACG) 2026 course.

The project focuses on building a modular rendering framework from the ground up, covering ray tracing fundamentals, physically based light transport, material models, geometric intersections, and acceleration structures.

## Overview

This project implements a foundation for a Monte Carlo path tracer. The codebase is organized into independent modules to make the renderer easier to test, extend, and maintain.

The current implementation focuses on ray generation, sphere intersections, material scattering, recursive light transport, and PPM image output.

## Features

### Rendering Core
- C++17 rendering framework
- Ray and vector mathematics
- Camera model with configurable field of view and aspect ratio
- Sphere intersection and surface hit records
- Sky gradient background
- Iterative path tracing with configurable maximum depth
- Monte Carlo sampling utilities

### Materials
- **Lambertian** diffuse reflection
- **Metal** reflection
- **Dielectric** transmission and refraction

### Scene and Output
- Procedural material test scene
- PPM image output
- Configurable image resolution and samples per pixel
- Organized output directories for renders and debugging

### Testing and Build System
- CMake-based build configuration
- Unit tests for material behavior
- Modular source layout designed for further development

## Tech Stack

- **Language:** C++17
- **Build System:** CMake 3.20+
- **Output Format:** PPM
- **Target Platform:** Windows (development environment)

## Project Structure

```text
ACG-PathTracer/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── core/           # Vectors, rays, intervals, random sampling
│   ├── geometry/       # Geometric primitives and intersection logic
│   ├── camera/         # Camera and ray generation
│   ├── materials/      # Surface scattering models
│   ├── sampling/       # Sampling and PDF utilities
│   ├── acceleration/   # Acceleration structures
│   ├── integrator/     # Path tracing integrator
│   ├── scene/          # Test scene definitions
│   └── output/         # Image output and output paths
├── tests/              # Unit tests
├── results/
│   ├── debug/          # Debug images
│   ├── tests/          # Test outputs
│   └── renders/        # Rendered images
└── assets/             # Additional project assets
```

Some modules are still under development; the structure is intended to support incremental implementation.

## Build Instructions

### Requirements

- A C++17-compatible compiler
- CMake 3.20 or later
- A supported build toolchain, such as Visual Studio on Windows

### Build on Windows

Open PowerShell in the project root directory and run:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

### Run the Renderer

For a Visual Studio multi-configuration build:

```powershell
.\build\Debug\path_tracer.exe
```

The renderer writes its output to the path configured in `src/main.cpp`, currently:

```text
results/renders/material_test.ppm
```

The PPM image can be opened with an image viewer that supports the PPM format.

### Run Tests

Build the project first, then run:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Development Roadmap

Planned extensions include:

- Axis-aligned bounding boxes (AABBs) and a bounding volume hierarchy (BVH)
- Improved acceleration structures and performance benchmarking
- Additional geometric primitives and mesh support
- More advanced sampling strategies, including multiple importance sampling (MIS)
- Physically based material models and texture mapping
- Depth of field, motion blur, and additional camera effects
- Direct illumination and area-light sampling
- Further validation through unit tests and rendered reference scenes

These items represent development goals and should not be interpreted as completed features.

## Project Goals

The primary goals are to:

1. Build a clear and extensible path tracing architecture.
2. Understand ray-surface intersection and Monte Carlo light transport.
3. Implement physically based material scattering.
4. Improve rendering performance through acceleration structures.
5. Validate rendering components with tests and reproducible scenes.

## Status

**Early development.** The current focus is on establishing a reliable rendering core and a maintainable project structure before adding more advanced rendering features.

## License

No license has been specified yet. Unless a license is added to this repository, all rights remain with the copyright holder.