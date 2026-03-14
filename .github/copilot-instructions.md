# GitHub Copilot Instructions for Obol

## Development Environment Setup

### Required System Packages

Before building Obol, install the following development packages. On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential \
    libx11-dev libxext-dev libxi-dev libxrandr-dev \
    libxcursor-dev libxft-dev libxinerama-dev \
    libgl-dev libglu1-mesa-dev mesa-common-dev \
    libfontconfig-dev libfreetype-dev \
    libfltk1.3-dev \
    qt6-base-dev qt6-base-private-dev libqt6opengl6-dev \
    libembree-dev \
    vulkan-sdk libvulkan-dev glslang-tools \
    libpng-dev \
    xvfb
```

On Fedora/RHEL/Rocky:

```bash
sudo dnf install -y \
    cmake gcc-c++ \
    libX11-devel libXext-devel libXi-devel libXrandr-devel \
    libXcursor-devel libXft-devel libXinerama-devel \
    mesa-libGL-devel mesa-libGLU-devel \
    fontconfig-devel freetype-devel \
    fltk-devel \
    qt6-qtbase-devel \
    embree-devel \
    vulkan-devel glslang \
    libpng-devel \
    xorg-x11-server-Xvfb
```

### Package Groups

| Group | Packages | Purpose |
|-------|----------|---------|
| X11 | `libx11-dev`, `libxext-dev`, `libxi-dev`, `libxrandr-dev`, `libxcursor-dev`, `libxft-dev`, `libxinerama-dev` | X Window System display and input |
| Mesa/OpenGL | `libgl-dev`, `libglu-dev`, `mesa-common-dev` | OpenGL rendering (system GL backend) |
| FLTK | `libfltk1.3-dev` | FLTK GUI toolkit for `obol_viewer` and `obol_minimalist_viewer` |
| Qt6 | `qt6-base-dev`, `libqt6opengl6-dev` | Qt6 GUI toolkit for `obol_qt_viewer` and `qt_obol_example` |
| Embree | `libembree-dev` (Embree 4) | Intel Embree CPU ray-tracing panel in viewers |
| Vulkan SDK | `vulkan-sdk`, `libvulkan-dev`, `glslang-tools` | Vulkan hardware-rasterisation panel in viewers |

> **Note:** FLTK must be installed as a system package. The project no longer
> ships a bundled copy in `external/fltk`. If `find_package(FLTK)` fails, the
> FLTK-based viewers will be disabled automatically.

### OSMesa (Optional Headless Rendering)

For headless/offscreen rendering without a display server, the project includes
a bundled OSMesa via the `external/osmesa` git submodule:

```bash
git submodule update --init --recursive
```

### Build

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DOBOL_BUILD_TESTS=ON
cmake --build build -- -j$(nproc)
```

For headless/CI testing with OSMesa:

```bash
cmake -S . -B build -DOBOL_USE_OSMESA=ON -DOBOL_BUILD_TESTS=ON
```

### Running Tests

```bash
cd build
# With a real display:
ctest --output-on-failure
# Headless (Xvfb):
xvfb-run -a ctest --output-on-failure
```

## Repository Layout

```
obol/
├── src/          Core Obol library (Open Inventor subset)
├── include/      Public headers
├── tests/        Test runners and visual regression tests
│   ├── testlib/  libObolEx: shared scene catalog + test registry
│   ├── utils/    Context managers and test utilities
│   ├── rendering/ Visual regression tests
│   └── tools/    Unit tests for utility subsystems
├── examples/     GUI viewers and code examples
│   ├── fltk/     FLTK-based viewers (obol_viewer, obol_minimalist_viewer)
│   ├── qt/       Qt6-based viewer + widget example
│   └── Mentor/   Open Inventor Mentor book examples
└── external/     Third-party dependencies (osmesa submodule, nanort, lodepng)
```

## Code Conventions

- C++17 required; use standard library features (e.g. `std::optional`, `std::string_view`)
- CMake 3.16+ with modern target-based builds (`target_link_libraries`, `target_include_directories`)
- All shared scene/test utilities go in `libObolEx` (`tests/testlib/`) so both tests and examples can link against them
- GUI viewers belong in `examples/` (not `tests/`)
- Headless/CLI test runners belong in `tests/`
