# Obol Coverage Baseline and Expansion Plan

*Baseline re-captured: 2026-02-24.  Build: Debug + OSMesa, `COIN_COVERAGE=ON`.*
*Previous baseline: 2026-02-23 (31.6 % lines / 40.7 % functions in `src/`).*

---

## Baseline metrics (overall project)

| Metric | Count | Total | Percentage |
|--------|------:|------:|-----------:|
| Lines (all files) | 46,681 | 99,374 | **47.0 %** |
| Lines (`src/` only) | 35,877 | 87,303 | **41.1 %** |
| Functions (`src/` only) | 7,603 | 16,102 | **47.2 %** |

---

## Per-subsystem breakdown (`src/`)

| Subsystem | Lines hit | Total lines | Line % | Funcs hit | Total funcs | Func % |
|-----------|----------:|------------:|-------:|----------:|------------:|-------:|
| `lists` | 365 | 483 | **75.6 %** | 77 | 102 | **75.5 %** |
| `sensors` | 503 | 686 | **73.3 %** | 106 | 144 | **73.6 %** |
| `elements` | 4,262 | 6,431 | **66.3 %** | 1,335 | 1,769 | **75.5 %** |
| `shadows` | 811 | 1,256 | **64.6 %** | 74 | 117 | **63.2 %** |
| `draggers` | 3,308 | 5,499 | **60.2 %** | 398 | 679 | **58.6 %** |
| `shaders` | 970 | 1,704 | **56.9 %** | 268 | 462 | **58.0 %** |
| `details` | 155 | 274 | **56.6 %** | 52 | 110 | **47.3 %** |
| `io` | 1,181 | 2,262 | **52.2 %** | 198 | 352 | **56.2 %** |
| `nodekits` | 1,058 | 2,016 | **52.5 %** | 187 | 344 | **54.4 %** |
| `glue` | 1,100 | 2,315 | **47.5 %** | 96 | 290 | **33.1 %** |
| `misc` | 1,990 | 4,846 | **41.1 %** | 633 | 1,160 | **54.6 %** |
| `threads` | 223 | 542 | **41.1 %** | 46 | 84 | **54.8 %** |
| `engines` | 1,496 | 3,550 | **42.1 %** | 283 | 856 | **33.1 %** |
| `nodes` | 4,341 | 10,384 | **41.8 %** | 995 | 1,983 | **50.2 %** |
| `actions` | 1,290 | 3,120 | **41.3 %** | 262 | 560 | **46.8 %** |
| `rendering` | 1,721 | 4,375 | **39.3 %** | 157 | 650 | **24.2 %** |
| `shapenodes` | 3,051 | 7,733 | **39.5 %** | 316 | 652 | **48.5 %** |
| `caches` | 754 | 2,088 | **36.1 %** | 88 | 196 | **44.9 %** |
| `fields` | 1,826 | 5,400 | **33.8 %** | 800 | 2,183 | **36.6 %** |
| `base` | 3,280 | 9,801 | **33.5 %** | 474 | 1,249 | **38.0 %** |
| `fonts` | 1,082 | 3,145 | **34.4 %** | 90 | 231 | **39.0 %** |
| `projectors` | 241 | 730 | **33.0 %** | 45 | 101 | **44.6 %** |
| `bundles` | 81 | 272 | **29.8 %** | 15 | 40 | **37.5 %** |
| `manips` | 296 | 1,158 | **25.6 %** | 86 | 217 | **39.6 %** |
| `errors` | 103 | 376 | **27.4 %** | 22 | 77 | **28.6 %** |
| `events` | 136 | 585 | **23.2 %** | 50 | 88 | **56.8 %** |
| `geo` | 72 | 622 | **11.6 %** | 29 | 145 | **20.0 %** |
| `hardcopy` | 17 | 1,215 | **1.4 %** | 10 | 158 | **6.3 %** |
| `profiler` | 23 | 2,745 | **0.8 %** | 9 | 305 | **3.0 %** |
| `collision` | 5 | 1,289 | **0.4 %** | 3 | 85 | **3.5 %** |
| `tools` | 0 | 36 | **0.0 %** | 0 | 8 | **0.0 %** |

---

## Top files by uncovered lines (opportunities)

Files with the most uncovered lines that are **realistically testable** (skipping
profiler, hardcopy, collision and large third-party files):

| File | Uncovered | Total | Line % |
|------|----------:|------:|-------:|
| `fonts/struetype.h` (font rasterizer) | 1,648 | 2,269 | 27 % |
| `nodes/SoExtSelection.cpp` | 1,085 | 1,089 | 0 % |
| `glue/gl.cpp` (GL dispatch) | 930 | 1,969 | 53 % |
| `actions/SoGLRenderAction.cpp` | 607 | 862 | 30 % |
| `rendering/SoRenderManager.cpp` | 573 | 573 | 0 % |
| `draggers/SoTransformerDragger.cpp` | 550 | 1,062 | 48 % |
| `io/SoInput.cpp` | 489 | 870 | 44 % |
| `misc/SoProto.cpp` | 480 | 506 | 5 % |
| `nodekits/SoBaseKit.cpp` | 446 | 894 | 50 % |
| `shapenodes/SoImage.cpp` | 426 | 430 | 1 % |
| `shapenodes/SoShape.cpp` | 415 | 758 | 45 % |
| `rendering/SoOffscreenRenderer.cpp` | 398 | 550 | 28 % |
| `shapenodes/SoText3.cpp` | 391 | 762 | 49 % |
| `shadows/SoShadowGroup.cpp` | 388 | 1,124 | 65 % |
| `rendering/SoGLImage.cpp` | 388 | 855 | 55 % |
| `rendering/SoGLBigImage.cpp` | 376 | 392 | 4 % |
| `rendering/SoGL.cpp` | 370 | 927 | 60 % |
| `shapenodes/SoAsciiText.cpp` | 368 | 372 | 1 % |
| `fields/SoField.cpp` | 362 | 879 | 59 % |
| `base/SbDPMatrix.cpp` | 355 | 666 | 47 % |

---

## Expansion plan (priority order)

Each item lists the planned test, the subsystem it improves, and an estimated
lines-uncovered gain.

### Tier 1 — High impact, low effort (~10–15 % total coverage gain)

These render a scene or exercise well-understood APIs and can be written quickly
as self-validating rendering tests.

| Priority | Test to add | Subsystem(s) improved | Estimated gain | Status |
|----------|-------------|----------------------|----------------|--------|
| 1 | **`render_indexed_face_set`** — large `SoIndexedFaceSet` scene with normals, texture coords, `SoCoordinate3`, `SoNormal`, `SoTextureCoordinate2` | `shapenodes`, `bundles`, `elements` | +300 lines | ✅ Done |
| 2 | **`render_quad_mesh`** — `SoQuadMesh` grid with per-vertex material binding | `shapenodes`, `nodes` | +300 lines | ✅ Done |
| 3 | **`render_indexed_line_set`** — `SoIndexedLineSet` multi-segment polylines | `shapenodes` | +350 lines | ✅ Done |
| 4 | **`render_point_set`** — `SoPointSet` + `SoVertexProperty` large point cloud | `shapenodes`, `nodes` | +180 lines | ✅ Done |
| 5 | **`render_lod`** — `SoLOD` and `SoLevelOfDetail` switching between representations | `nodes` | +250 lines | ✅ Done |
| 6 | **`render_scene_texture`** — `SoSceneTexture2` offscreen render-to-texture | `nodes`, `rendering` | +570 lines | ✅ Done |
| 7 | **`render_shadow`** — `SoShadowGroup` + `SoShadowStyle` with a casting sphere | `shadows` | +1,000 lines | ✅ Done |
| 8 | **`render_array_multiple_copy`** — `SoArray` and `SoMultipleCopy` instanced geometry | `nodes` | +180 lines | ✅ Done |

> **Note on control images (Tier 1):** Control images for all Tier 1 tests were generated with
> the bundled OSMesa backend.  PNG control images are stored in `tests/control_images/` and
> registered as image-comparison regression tests in `tests/rendering/CMakeLists.txt`.  libpng
> is now built from `external/libpng` (no system `libpng-dev` required); `rgb_to_png` and
> `image_comparator` are always built with full PNG support.

### Tier 2 — Medium impact, medium effort (~5–8 % gain)

Require slightly more complex scenes or specific conditions:

| Priority | Test to add | Subsystem(s) improved | Estimated gain | Status |
|----------|-------------|----------------------|----------------|--------|
| 9 | **`render_shape_hints`** — `SoShapeHints` (SOLID/COUNTERCLOCKWISE/CONVEX) effect on backface culling and normals | `nodes`, `shapenodes` | +180 lines | ✅ Done |
| 10 | **`render_environment`** — `SoEnvironment` fog, ambient colour | `nodes` | +140 lines | ✅ Done |
| 11 | **`render_texture3`** — `SoTexture3` (3-D texture) applied to a cube | `nodes`, `rendering` | +185 lines | ✅ Done |
| 12 | **`render_bump_map`** — `SoBumpMap` + `SoBumpMapCoordinate` on a sphere | `nodes`, `rendering` | +125 lines | ✅ Done |
| 13 | **`render_texture_transform`** — `SoTexture2Transform` scale/rotate/translate | `nodes` | +90 lines | ✅ Done |
| 14 | **`render_depth_buffer`** — `SoDepthBuffer` enable/disable/func modes | `nodes` | +80 lines | ✅ Done |
| 15 | **`render_alpha_test`** — `SoAlphaTest` threshold scene | `nodes` | +50 lines | ✅ Done |
| 16 | **Action integration** — `SoRayPickAction` against a complex scene (face set + transforms); validate picked path, point, detail | `actions`, `misc` | +400 lines | ✅ Done (added to `tests/actions/test_actions_suite.cpp`) |
| 17 | **Matrix math** — `SbDPMatrix` operations: mult, invert, decompose | `base` | +450 lines | ✅ Done (added to `tests/base/test_sb_types.cpp`) |
| 18 | **`SbMatrix` full suite** — det, factor, inverse, multVecMatrix, multDirMatrix | `base` | +300 lines | ✅ Done (added to `tests/base/test_sb_types.cpp`) |
| 19 | **`SoField` connection / notification** — connect/disconnect, isConnected, notify propagation | `fields`, `misc` | +350 lines | ✅ Done (`tests/fields/test_field_connections.cpp`) |
| 20 | **`SoNodeKit` traversal** — instantiate a nodekit, set part, traverse with `SoGetBoundingBoxAction` | `nodekits` | +450 lines | ✅ Done (`tests/nodes/test_nodekit_traversal.cpp`) |

> **Note on control images (Priority 9–15):** Control images for Tier 2 rendering tests were
> generated using the bundled OSMesa backend (`COIN3D_USE_OSMESA=ON`) because the GLX CI
> environment had difficulties with context setup.  These should be regenerated from a GLX
> environment when possible (see `tests/rendering/generate_controls.sh`).

### Tier 3 — Lower priority / harder to test

| Priority | Area | Reason / approach |
|----------|------|-------------------|
| 21 | `draggers/` (16 % line coverage) | Need display + event simulation; start with simple draggers (`SoTranslatePlaneDragger`) | ✅ Done (`tests/draggers/test_draggers.cpp`) |
| 22 | `manips/` (4 %) | Built on draggers; test manipulator attach/detach lifecycle | ✅ Done (`tests/manips/test_manips.cpp`) |
| 23 | `rendering/SoOffscreenRenderer.cpp` (26 %) | Test getBuffer(), getViewportRegion(), multiple renders with same renderer | ✅ Done (`tests/rendering/render_offscreen.cpp`) |
| 24 | `glue/gl.cpp` (45 %) | Exercises GL entry-point dispatch; covered by rendering tests, diminishing returns |
| 25 | `io/SoInput.cpp` + `SoOutput.cpp` | Add edge cases: binary read, nested files, invalid inputs |
| 26 | `fonts/` (34 %) | SoText2 / SoText3 visual tests once font baseline settled |
| 27 | `profiler/` (1 %) | Enable profiling in a render loop and verify statistics |
| 28 | `collision/` (0 %) | `SoIntersectionDetectionAction` against a scene with known overlaps |

---

## How to regenerate the baseline

```bash
# Full workflow:
cmake -S . -B build_cov \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCOIN3D_USE_OSMESA=ON \
    -DCOIN_COVERAGE=ON

cmake --build build_cov --parallel $(nproc)
cmake --build build_cov --target coverage

# Open the report:
xdg-open build_cov/coverage_report/index.html
```

The `coverage` target resets gcov counters, runs the full CTest suite (continuing
even if some tests fail), captures lcov data, strips system/third-party paths, and
emits an HTML report to `<build>/coverage_report/`.

---

## Coverage targets

| Milestone | Line % goal | Notes |
|-----------|------------:|-------|
| Baseline (2026-02-23) | 31.6 % | `src/` only; initial baseline |
| Re-baseline (2026-02-24, current) | **41.1 %** | `src/` only; after all Tier 1 + Tier 2 + Tier 3 work |
| Next target | ~ 55 % | Focus on rendering, manips, geo, base |
| Long-term goal | ≥ 70 % | Covers all actively-maintained APIs |
