# Build Options

This document describes CMake build options available in Obol.

## Core Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Build shared library when ON, static library when OFF |
| `OBOL_BUILD_TESTS` | `ON` | Build unit tests |
| `OBOL_BUILD_VIEWER` | auto | Build the FLTK scene viewer (`obol_viewer`); auto-enabled if FLTK is found |

## OpenGL Backend Options

Obol supports multiple OpenGL backend configurations.

### System OpenGL / OSMesa backends

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_USE_OSMESA` | `OFF` | Build against OSMesa only (headless/offscreen rendering, no system GL) |
| `OBOL_USE_SYSTEM_ONLY` | `OFF` | Build against system OpenGL only (no OSMesa fallback) |
| `OBOL_BUILD_DUAL_GL` | `OFF` | Build with both system OpenGL and OSMesa backends in a single library |

When all three are `OFF` (the default), CMake auto-detects available backends: dual mode if both system OpenGL and OSMesa are present, otherwise whichever is available.

`OBOL_USE_OSMESA` and `OBOL_BUILD_DUAL_GL` are mutually exclusive.

### PortableGL backend (experimental)

[PortableGL](https://github.com/rswinkle/PortableGL) is a single-header CPU software
renderer implementing an OpenGL 3.x-style core profile.  It requires no system
libraries and produces a fully self-contained binary — useful for embedded targets,
WebAssembly, and environments with no display server.

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_USE_PORTABLEGL` | `OFF` | Build against PortableGL only (no system GL or OSMesa) |
| `OBOL_BUILD_DUAL_PORTABLEGL` | `OFF` | Build with both system OpenGL (primary) and PortableGL in a single library |

`OBOL_USE_PORTABLEGL` is mutually exclusive with all other backend options.

`OBOL_BUILD_DUAL_PORTABLEGL` requires system OpenGL and PortableGL to be present
(`external/portablegl/portablegl.h`).

> **Status**: PortableGL is experimental.  Basic geometry, lighting, and texturing work
> via an Obol-supplied compatibility layer.  Shadow maps (`SoShadowGroup`) are not yet
> functional (depth-only FBO support is incomplete).  See
> [docs/SOFTWARE_GL_COMPARISON.md](SOFTWARE_GL_COMPARISON.md) for a full feature
> comparison with OSMesa.

## Feature Options

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_THREADSAFE` | `ON` | Enable thread-safe render traversals |
| `USE_EXCEPTIONS` | `ON` | Compile with C++ exceptions |

## Component Options

| Option | Default | Description |
|--------|---------|-------------|
| `HAVE_NODEKITS` | `ON` | Enable NodeKit support |
| `HAVE_DRAGGERS` | `ON` | Enable Dragger support |
| `HAVE_MANIPULATORS` | `ON` | Enable Manipulator support |
| `OBOL_PROFILING` | `OFF` | Enable profiling subsystem |

## Build Optimization Options

### Precompiled Headers

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_USE_PRECOMPILED_HEADERS` | `ON` | Use precompiled headers to speed up compilation (requires CMake 3.16+) |

The precompiled header (`src/misc/CoinPCH.h`) includes commonly used standard C++ library headers and is applied to the main `Obol` library target and all object library subdirectories. On CMake versions older than 3.16 this option is automatically disabled with a warning.

**Performance impact** (GCC 13.3.0, Ubuntu, `-j4`):
- Without PCH: ~3m30s
- With PCH: ~2m53s (~18% faster, ~37 seconds saved)

### Code Coverage

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_COVERAGE` | `OFF` | Enable code coverage instrumentation (GCC/Clang only); provides a `coverage` target that runs CTest and generates an lcov HTML report |

## Build Types

Set via `CMAKE_BUILD_TYPE`: `Release` (default), `Debug`, `MinSizeRel`, `RelWithDebInfo`.

## Example Build Commands

**Standard release build:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Release
cmake --build build_dir -- -j4
```

**Development build with debug info:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Debug
cmake --build build_dir -- -j4
```

**Minimal release build (no tests):**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Release -DOBOL_BUILD_TESTS=OFF
cmake --build build_dir -- -j4
```

**Static library build:**
```bash
cmake -S . -B build_dir -DBUILD_SHARED_LIBS=OFF
cmake --build build_dir -- -j4
```

**Headless/offscreen-only build:**
```bash
cmake -S . -B build_dir -DOBOL_USE_OSMESA=ON
cmake --build build_dir -- -j4
```

**PortableGL software-only build (no system GL deps):**
```bash
cmake -S . -B build_dir -DOBOL_USE_PORTABLEGL=ON
cmake --build build_dir -- -j4
```

**Dual-backend build (system GL primary + PortableGL software fallback):**
```bash
cmake -S . -B build_dir -DOBOL_BUILD_DUAL_PORTABLEGL=ON
cmake --build build_dir -- -j4
```