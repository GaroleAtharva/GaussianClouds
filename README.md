# GaussianClouds
A rendering pipeline for generating and visualizing clouds using Gaussian splatting techniques. It models clouds as Gaussian distributions, offering realistic lighting, smooth transitions, and optimized performance.

## Features
- GPU-accelerated 3D Gaussian Splatting renderer using OpenGL 4.5 compute shaders
- Handles 2M+ splats at interactive frame rates (~40 FPS)
- Full GPU pipeline: preprocessing, bitonic sort, instanced quad rendering — zero CPU per-frame bottleneck
- 2D covariance projection, frustum culling, and depth-sorted alpha blending
- ImGui controls for splat scale, max visible count, camera, and debug stats
- Point cloud renderer fallback mode

## Requirements
- Windows 10/11
- GPU with OpenGL 4.5+ support (NVIDIA recommended)
- CMake 3.24+
- C++17 compiler (Visual Studio 2022/2025)

## How to Build & Run

### 1. Clone the repository
```bash
git clone https://github.com/GaroleAtharva/GaussianClouds.git
cd GaussianClouds
```

### 2. Configure with CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```
Or using Visual Studio's CMake:
```bash
cmake -B build -G "Visual Studio 17 2022"
```

### 3. Build
```bash
cmake --build build --config Release
```

### 4. Run
```bash
./build/bin/Release/GaussianClouds.exe
```

### 5. Load a PLY file
- Use the **"Load PLY"** button in the ImGui panel to load a `.ply` file exported from a 3D Gaussian Splatting training run
- The PLY file should contain Gaussian attributes: positions, spherical harmonics (SH), opacities, scales, and rotations

## Controls
- **Load PLY** — load a Gaussian splatting `.ply` file
- **Use Splat Renderer** — toggle between splat and point cloud rendering
- **Splat Scale** — adjust Gaussian splat size
- **Max Visible Splats** — cap drawn splats (5K–2M)
- **Center Camera on Cloud** — reset camera to point cloud centroid
- **Rotation sliders** — orbit the scene

## Project Structure
```
GaussianClouds/
├── include/          # Header files
├── src/              # C++ source files
│   ├── main.cpp
│   ├── SplatRenderer.cpp    # GPU compute pipeline
│   ├── PLYLoader.cpp        # PLY file parser
│   └── ...
├── shaders/
│   ├── preprocess.comp      # Compute: 2D projection + culling
│   ├── sort.comp            # Compute: bitonic depth sort
│   ├── splat.vert           # Vertex: instanced quad rendering
│   └── splat.frag           # Fragment: Gaussian alpha blending
├── external/         # GLAD OpenGL loader
├── legacy/           # Old test files
└── CMakeLists.txt
```
