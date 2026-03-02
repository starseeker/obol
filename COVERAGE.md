# Obol Code Coverage Report

Generated from an lcov coverage build (`OBOL_COVERAGE=ON`, `OBOL_USE_OSMESA=ON`,
`CMAKE_BUILD_TYPE=Debug`) with the full CTest suite (190 tests) plus 62 unit tests.

**Toolchain:** GCC + lcov 2.0, OSMesa headless rendering

---

## Overall Results

| Metric    | Covered | Total  | Percentage |
|-----------|---------|--------|------------|
| Lines     | 40,377  | 85,652 | **47.1%**  |
| Functions | 7,599   | 15,937 | **47.7%**  |
| Branches  | —       | —      | no data    |

*(Previous baseline: 50.0% lines / 47.1% functions on 104,084 total lines including test
harness code; after filtering to `src/` only the normalized figure is 47.1%.)*

---

## Per-Subsystem Coverage

### Well-Covered Subsystems (≥ 60%)

| Subsystem | Representative Coverage |
|-----------|------------------------|
| `src/details` | ~61–82% (SoConeDetail 82%, SoCubeDetail 75%, SoDetail 32%) |
| `src/elements` (non-GL) | ~85% overall; most element push/pop paths exercised |
| `src/errors` | ~83% (SoDebugError well covered) |
| `src/manips` | ~86% average (SoTransformerManip 93%, most kit-manips 71%) |
| `src/fields/SoFieldContainer` | 82% |
| `src/sensors/SoOneShotSensor` | 80% |
| `src/rendering/SoRenderManager` | 63% |
| `src/actions/SoGetMatrixAction` | 70% |
| `src/actions/SoGetPrimitiveCountAction` | 91% |
| `src/actions/SoWriteAction` | 59% |
| `src/base/SbColor4f` | 81% |
| `src/base/SbBox3d` | 93% |
| `src/base/SbVec2f` | 83% |

---

### Moderate Coverage (30–60%)

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoCallbackAction.cpp` | 50% | Exercised by callback tests |
| `src/actions/SoSearchAction.cpp` | 35% | Deeper search exercised |
| `src/actions/SoHandleEventAction.cpp` | 32% | Event routing partially tested |
| `src/actions/SoGetBoundingBoxAction.cpp` | 31% | BBox traversal partial |
| `src/base/SbMatrix.cpp` | 15% | **489 lines** – major gap; factor/LU/transpose untested |
| `src/base/SbRotation.cpp` | 16% | **192 lines** – slerp/many paths untested |
| `src/base/SbViewVolume.cpp` | 26% | More paths added in unit tests |
| `src/base/SbDPMatrix.cpp` | 53% | Double-precision matrix |
| `src/base/SbDPRotation.cpp` | 28% | Double-precision rotation |
| `src/base/SbDPViewVolume.cpp` | 11% | **402 lines** – major gap |
| `src/base/SbPlane.cpp` | 18% | intersect/offset/transform added |
| `src/base/SbLine.cpp` | 26% | getClosestPoint/setPosDir added |
| `src/base/SbDict.cpp` | 46% | enter/find/remove tested |
| `src/base/SbOctTree.cpp` | 45% | add/find/remove tested |
| `src/base/SbBSPTree.cpp` | 37% | addPoint/findClosest tested |
| `src/rendering/SoGL.cpp` | 49% | **606 lines** – GL dispatch |
| `src/rendering/SoOffscreenRenderer.cpp` | 16% | Off-screen render API |
| `src/sensors/SoSensorManager.cpp` | 19% | Sensor queue management |
| `src/nodes/SoCamera.cpp` | 17% | **203 lines** – orbitCamera/stereo added |
| `src/nodes/SoMaterial.cpp` | 16% | **127 lines** |
| `src/nodes/SoGroup.cpp` | 24% | **164 lines** |

---

### Low / No Coverage (< 30% or zero lines hit)

#### Math & Geometry Primitives (HIGH PRIORITY)

| File | Coverage | Status |
|------|----------|--------|
| `src/base/SbMatrix.cpp` | **15%** | factor/LU/transpose still untested |
| `src/base/SbRotation.cpp` | **16%** | slerp, many quaternion paths untested |
| `src/base/SbDPViewVolume.cpp` | **11%** | 402 lines – only ortho/persp basics covered |
| `src/base/SbTesselator.cpp` | **10%** | beginPolygon/addVertex/endPolygon added |
| `src/base/SbHeap.cpp` | **26%** | add/extractMin/emptyHeap added |
| `src/base/SbClip.cpp` | **13%** | addVertex/clip/getVertex added |
| `src/base/SbFont.cpp` | **17%** | **198 lines** – font API untested |
| `src/base/SbImage.cpp` | **25%** | Image manipulation |
| `src/base/SbCylinder.cpp` | **24%** | intersect added |
| `src/base/SbSphere.cpp` | **27%** | intersect/circumscribe/pointInside added |
| `src/base/SbBox2d.cpp` | **0%** | No tests yet |
| `src/base/SbBox2i32.cpp` | **0%** | No tests yet |
| `src/base/SbBox3i32.cpp` | **0%** | No tests yet |
| `src/base/SbBox3s.cpp` | **0%** | No tests yet |
| `src/base/SbVec2s.cpp` | **0%** | Logic covered via SbVec2s unit tests |
| `src/base/SbVec2i32.cpp` | **0%** | SbVec2i32 unit tests added (header-inline) |
| `src/base/SbVec3b.cpp` | **0%** | Unit tests added (header-inline) |
| `src/base/SbVec3i32.cpp` | **0%** | Unit tests added (header-inline) |
| `src/base/SbVec3s.cpp` | **0%** | Unit tests added (header-inline) |
| `src/base/SbVec4i32.cpp` | **0%** | Unit tests added (header-inline) |

#### Rendering Pipeline (HIGH PRIORITY)

| File | Coverage | Missing |
|------|----------|---------|
| `src/rendering/SoGLRenderAction.cpp` | **21%** | Multi-pass, transparency, depth sorting |
| `src/rendering/SoGLImage.cpp` | **13%** | Texture upload, mipmap, format conversion |
| `src/rendering/SoGLBigImage.cpp` | **12%** | Large texture tiling |
| `src/rendering/SoGLCubeMapImage.cpp` | **19%** | Cube-map textures |
| `src/rendering/SoOffscreenRenderer.cpp` | **16%** | Offscreen render API |
| `src/rendering/SoVBO.cpp` | **16%** | VBO allocation/update |

#### Shape Nodes (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/shapenodes/SoShape.cpp` | **12%** | Base shape class; bbox/prim-count added |
| `src/shapenodes/SoFaceSet.cpp` | **15%** | bbox+primitive count tests added |
| `src/shapenodes/SoIndexedFaceSet.cpp` | **10%** | bbox+primitive count tests added |
| `src/shapenodes/SoAsciiText.cpp` | **12%** | Annotation text |
| `src/shapenodes/SoImage.cpp` | **11%** | Image billboard node |
| `src/shapenodes/SoIndexedLineSet.cpp` | **10%** | Line set |
| `src/shapenodes/SoIndexedTriangleStripSet.cpp` | **12%** | Triangle strip set |
| `src/shapenodes/SoText2.cpp` | **0%** | 2D raster text – entirely untested |
| `src/shapenodes/SoText3.cpp` | **0%** | 3D extruded text – entirely untested |
| `src/shapenodes/SoProceduralShape.cpp` | **14%** | Procedural shape API |

#### Actions (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoAction.cpp` | **17%** | Base action traversal |
| `src/actions/SoGLRenderAction.cpp` | **21%** | Core rendering traversal |
| `src/actions/SoRayPickAction.cpp` | **12%** | **400 lines** – ray picking |

#### Scene Management (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/misc/SoSceneManager.cpp` | **0%** | Scene lifecycle management |
| `src/misc/SoEventManager.cpp` | **0%** | Event dispatch |
| `src/misc/SoLightPath.cpp` | **0%** | Lighting traversal path |

#### Engines (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/engines/SoBoolOperation.cpp` | **26%** | A_AND_B / A_OR_B added |
| `src/engines/SoCalculator.cpp` | **12%** | expression eval partially tested |
| `src/engines/SoEngine.cpp` | **28%** | getOutputs/getOutput added |
| `src/engines/SoInterpolateFloat.cpp` | **~13%** | alpha 0.5 test added |

#### Sensors (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/sensors/SoSensorManager.cpp` | **19%** | processTimerQueue/processDelayQueue exercised |
| `src/sensors/SoTimerSensor.cpp` | **30%** | Timer callbacks |
| `src/sensors/SoPathSensor.cpp` | **0%** | Path-attachment sensor |
| `src/sensors/SoIdleSensor.cpp` | **0%** | Idle callback |

#### Shaders (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/shaders/SoShader.cpp` | **6%** | Shader init |
| `src/shaders/SoShaderObject.cpp` | **20%** | Shader object |
| `src/shaders/SoGLSLShaderObject.cpp` | **18%** | GLSL object |
| `src/shaders/SoGLARBShaderObject.cpp` | **0%** | Legacy ARB shaders |

#### Profiler Subsystem (LOW – may be kept for dev use only)

| File | Coverage | Notes |
|------|----------|-------|
| `src/profiler/SbProfilingData.cpp` | **0%** | Profiling data |
| `src/profiler/SoProfilerOverlayKit.cpp` | **0%** | Visual overlay |

#### HUD Subsystem (LOW)

| File | Coverage | Notes |
|------|----------|-------|
| `src/hud/SoHUD.cpp` | **25%** | HUD root node |
| `src/hud/SoHUDKit.cpp` | **31%** | |

---

## Progress Summary (Test Additions This Session)

| Iteration | New Tests | Total | Coverage (src/) |
|-----------|-----------|-------|-----------------|
| Baseline  | 0         | 25    | ~50.0% (mixed) |
| Iter 1    | +21       | 36    | — |
| Iter 2    | +11       | 47    | — |
| Iter 3    | +7        | 54    | 47.0% |
| Iter 4    | +8        | 62    | 47.1% |

**62 unit tests now pass (0 failures).** The unit tests cover:
- All SbVec2/3/4 integer/byte variants (SbVec2s, SbVec2i32, SbVec3s, SbVec3i32, SbVec4i32, SbVec3b)
- SbMatrix (multMatrixVec, multDirMatrix, multLeft, setScale, equals)
- SbRotation (invert, getValue(4f), getValue(matrix), setValue(matrix), operator[], *=, ==)
- SbViewVolume (projectToScreen, getWorldToScreenScale, narrow, getPlane, getMatrices)
- SbDPViewVolume (ortho, perspective, getSightPoint, projectToScreen, getMatrices)
- SbLine (getClosestPoint, getClosestPoints, setPosDir)
- SbPlane (3-point, intersect, offset, transform, plane-plane intersect, ==)
- SbSphere (circumscribe, pointInside, intersect 1/2 arg)
- SbCylinder (intersect)
- SbHeap (add, extractMin, getMin, emptyHeap)
- SbTesselator (quad/triangle tessellation)
- SbOctTree (addItem, findItems by point/box, removeItem)
- SbBSPTree (addPoint, findPoint, findClosest, dedup)
- SbClip (addVertex, clip by plane, getVertex)
- SbDict (enter, find, remove, applyToAll)
- SbImage (2D/3D set/getValue, equality)
- SbColor (HSV, packed), SbColor4f
- SoGroup/SoSeparator (hierarchy, caching modes, bbox)
- SoPerspectiveCamera / SoOrthographicCamera (viewAll, getViewVolume, orbitCamera, stereo)
- SoFaceSet / SoIndexedFaceSet (bbox, primitive count)
- SoSearchAction (type/name/ALL/derived, searchingAll)
- SoCallbackAction (pre/post callbacks, triangle callback)
- SoGetBoundingBoxAction (sphere/cube/faceSet)
- SoGetPrimitiveCountAction (triangles/lines)
- SoGetMatrixAction (translation node, path-based)
- SoHandleEventAction (viewport, event, handled, pickRadius, apply)
- SoRayPickAction (sphere/cube picks)
- SoWriteAction + SoDB::readAll (round-trip)
- SoPath (copy, findFork, containsNode, truncate)
- SoField connection (engine→field, field→field, disconnect)
- SoComposeVec3f, SoInterpolateFloat, SoBoolOperation (A_AND_B, A_OR_B)
- SoFieldSensor, SoNodeSensor
- SoInfo, SoAnnotation, SoFont, SoDrawStyle, SoComplexity
- SoNode (setName/getByName, isOfType, copy)
- SbViewportRegion (aspect, setViewportPixels, setPixelsPerInch)
- SbString, SbName, SbTime
- SbXfBox3f, SbBox3d, SbBox2f, SbBox2s

---

## Priority Queue for Adding Tests (Updated)

### Tier 1 — Critical (highest ROI remaining)

1. **`SbMatrix` deeper** — 15% of 489 lines. Still missing: `factor()`,
   `LUDecompose()`, `LUBackSub()`, `det()`, `transpose()`.
2. **`SoRayPickAction`** — 12% of 400 lines. Add more picking tests with
   different shapes, pick radius, sorted picks.
3. **`SoGLRenderAction`** — 21% of 347 lines. Needs transparency sort modes,
   render passes.
4. **`SbDPViewVolume`** — 11% of 402 lines. Add narrow/getAlignRotation/projectPointToLine.

### Tier 2 — Important (next pass)

5. **`SbRotation`** — 16% of 192 lines. Add slerp, more quaternion ops.
6. **`SoCamera`** — 17% of 203 lines. Add viewBoundingBox, project/unproject.
7. **`SoFaceSet` / `SoIndexedFaceSet`** — Only bbox tested; add normal binding, GLRender.
8. **`SoText2` / `SoText3`** — 0%. Add minimal construction + bbox tests.

### Tier 3 — Worthwhile (as time allows)

9. **Projectors** — SbCylinderSheetProjector 0%, SbSphereSheetProjector 0%.
10. **`SoSensorManager`** — 19%; processIdleQueue, reschedule paths.
11. **Shader subsystem** — SoShaderObject 20%, SoGLSLShaderObject 18%.
12. **`SoSceneManager`** — 0%; lifecycle tests (setSceneGraph, render, event).

### Tier 4 — Deferred

13. **Profiler** — 8 files at 0%. Consider whether profiling is a supported feature.
14. **HUD** — 23–31%; needs visual + layout unit tests.
15. **Shadows** — visual regression test exists but unit tests missing.

---

## Cleanup Done Before This Run

Before the lcov run, the following fully-stubbed / never-implemented APIs were
removed from the build to eliminate false "uncovered" noise:

| Removed | Reason |
|---------|--------|
| `SoByteStream` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranSender` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranReceiver` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranscribe.h` | Header that only forwarded to the two removed classes |
| `SoNormalGenerator::setNumNormals()` | Called `OBOL_OBSOLETED()` only |
| `SoNormalGenerator::setNormal()` | Called `OBOL_OBSOLETED()` only |

---

## How to Reproduce

```bash
# Install lcov (if not present)
sudo apt-get install lcov

# Configure
cmake /path/to/obol \
  -DOBOL_USE_OSMESA=ON \
  -DOBOL_BUILD_TESTS=ON \
  -DOBOL_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -B build_coverage

# Build and run coverage
cmake --build build_coverage -- -j4
cmake --build build_coverage --target coverage

# Open report
xdg-open build_coverage/coverage_report/index.html
```


---

## Per-Subsystem Coverage

### Well-Covered Subsystems (≥ 60%)

| Subsystem | Representative Coverage |
|-----------|------------------------|
| `src/details` | ~61–82% (SoConeDetail 82%, SoCubeDetail 75%, SoDetail 32%) |
| `src/elements` (non-GL) | ~85% overall; most element push/pop paths exercised |
| `src/errors` | ~83% (SoDebugError well covered) |
| `src/manips` | ~86% average (SoTransformerManip 93%, most kit-manips 71%) |
| `src/fields/SoFieldContainer` | 82% |
| `src/sensors/SoOneShotSensor` | 80% |
| `src/rendering/SoRenderManager` | 63% |
| `src/actions/SoGetMatrixAction` | 70% |
| `src/actions/SoWriteAction` | 59% |
| `src/base/SbColor` | 71%, `SbColor4f` 81%, `SbTime` 67% |

---

### Moderate Coverage (30–60%)

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoCallbackAction.cpp` | 50% | Needs callback path tests |
| `src/actions/SoSearchAction.cpp` | 35% | Needs deeper search tests |
| `src/actions/SoHandleEventAction.cpp` | 37% | Event routing untested |
| `src/actions/SoGetBoundingBoxAction.cpp` | 32% | BBox traversal partial |
| `src/base/SbMatrix.cpp` | 16% | **447 lines** – major gap |
| `src/base/SbRotation.cpp` | 17% | **173 lines** – major gap |
| `src/base/SbViewVolume.cpp` | 26% | View volume operations |
| `src/base/SbDPMatrix.cpp` | 53% | Double-precision matrix |
| `src/base/SbDPRotation.cpp` | 33% | Double-precision rotation |
| `src/base/SbDPViewVolume.cpp` | 11% | **402 lines** – major gap |
| `src/base/SbPlane.cpp` | 20% | Plane/intersection ops |
| `src/base/SbLine.cpp` | 26% | Line/intersection ops |
| `src/base/SbBSPTree.cpp` | 59% | |
| `src/base/SbFont.cpp` | 17% | **198 lines** – font API |
| `src/base/SbImage.cpp` | 30% | Image manipulation |
| `src/rendering/SoGL.cpp` | 49% | **606 lines** – GL dispatch |
| `src/rendering/SoOffscreenRenderer.cpp` | 22% | Off-screen render API |
| `src/rendering/SoGLImage.cpp` | 13% | **463 lines** – texture mgmt |
| `src/rendering/SoGLBigImage.cpp` | 12% | **281 lines** |
| `src/rendering/SoVBO.cpp` | 16% | Vertex buffer objects |
| `src/rendering/SoVertexArrayIndexer.cpp` | 28% | |
| `src/sensors/SoSensorManager.cpp` | 19% | Sensor queue management |
| `src/sensors/SoDataSensor.cpp` | 29% | |
| `src/nodes/SoCamera.cpp` | 17% | **197 lines** – core viewing |
| `src/nodes/SoMaterial.cpp` | 16% | **127 lines** – core material |
| `src/nodes/SoGroup.cpp` | 24% | **164 lines** – scene graph core |
| `src/nodes/SoExtSelection.cpp` | 14% | **557 lines** – selection |
| `src/draggers/SoDragger.cpp` | 24% | Base dragger |
| `src/draggers/SoTrackballDragger.cpp` | 15% | |
| `src/nodekits/SoBaseKit.cpp` | 23% | **302 lines** – kit framework |
| `src/io/SoInput.cpp` | ~40% | File reading |

---

### Low / No Coverage (< 30% or zero lines hit)

#### Math & Geometry Primitives (HIGH PRIORITY)

| File | Coverage | Missing |
|------|----------|---------|
| `src/base/SbMatrix.cpp` | **16%** | Inverse, decompose, factor, rotate, translate methods |
| `src/base/SbRotation.cpp` | **17%** | Quaternion slerp, axis-angle, matrix conversions |
| `src/base/SbViewVolume.cpp` | **26%** | Project, unproject, plane extraction |
| `src/base/SbDPViewVolume.cpp` | **11%** | Double-precision equivalent |
| `src/base/SbPlane.cpp` | **20%** | intersect(), getDistance(), transform() |
| `src/base/SbLine.cpp` | **26%** | getClosestPoint(), intersect() |
| `src/base/SbCylinder.cpp` | **26%** | intersect(), getAxis() |
| `src/base/SbSphere.cpp` | **31%** | intersect(), circumscribe() |
| `src/base/SbClip.cpp` | **14%** | Sutherland-Hodgman clipping |
| `src/base/SbTesselator.cpp` | **0%** | Polygon triangulation |
| `src/base/SbOctTree.cpp` | **0%** | Spatial partitioning |
| `src/base/SbHeap.cpp` | **0%** | Priority queue |
| `src/base/SbDict.cpp` | **0%** | Hash dictionary |
| `src/base/SbXfBox3d.cpp` | **0%** | Transformed double-precision bbox |
| `src/base/SbBox2d.cpp` | **0%** | Double-precision 2D box |
| `src/base/SbBox2i32.cpp` | **0%** | Integer 2D box |
| `src/base/SbBox3d.cpp` | **0%** | Double-precision 3D box |
| `src/base/SbBox3i32.cpp` | **0%** | Integer 3D box |
| `src/base/SbBox3s.cpp` | **0%** | Short integer 3D box |
| `src/base/SbVec2f.cpp` | **0%** | 2D float vector |
| `src/base/SbVec2s.cpp` | **0%** | 2D short vector |
| `src/base/SbVec2i32.cpp` | **0%** | 2D int32 vector |
| `src/base/SbVec2b.cpp` | **0%** | 2D byte vector |
| `src/base/SbVec2ub.cpp` | **0%** | 2D unsigned byte vector |
| `src/base/SbVec2us.cpp` | **0%** | 2D unsigned short vector |
| `src/base/SbVec2ui32.cpp` | **0%** | 2D uint32 vector |
| `src/base/SbVec3b.cpp` | **0%** | 3D byte vector |
| `src/base/SbVec3i32.cpp` | **0%** | 3D int32 vector |
| `src/base/SbVec3s.cpp` | **0%** | 3D short vector |
| `src/base/SbVec3ub.cpp` | **0%** | 3D unsigned byte vector |
| `src/base/SbVec3ui32.cpp` | **0%** | 3D unsigned int32 vector |
| `src/base/SbVec3us.cpp` | **0%** | 3D unsigned short vector |
| `src/base/SbVec4b.cpp` | **0%** | 4D byte vector |
| `src/base/SbVec4i32.cpp` | **0%** | 4D int32 vector |
| `src/base/SbVec4s.cpp` | **0%** | 4D short vector |
| `src/base/SbVec4ub.cpp` | **0%** | 4D unsigned byte vector |
| `src/base/SbVec4ui32.cpp` | **0%** | 4D unsigned int32 vector |
| `src/base/SbVec4us.cpp` | **0%** | 4D unsigned short vector |

#### Rendering Pipeline (HIGH PRIORITY)

| File | Coverage | Missing |
|------|----------|---------|
| `src/rendering/SoGLRenderAction.cpp` | **21%** | Multi-pass, transparency, depth sorting |
| `src/rendering/SoGLImage.cpp` | **13%** | Texture upload, mipmap, format conversion |
| `src/rendering/SoGLBigImage.cpp` | **12%** | Large texture tiling |
| `src/rendering/SoGLCubeMapImage.cpp` | **19%** | Cube-map textures |
| `src/rendering/SoOffscreenRenderer.cpp` | **22%** | Offscreen render API |
| `src/rendering/SoVBO.cpp` | **16%** | VBO allocation/update |
| `src/rendering/CoinOffscreenGLCanvas.cpp` | **14%** | GL canvas management |
| `src/rendering/SoGLDriverDatabase.cpp` | **20%** | GL feature detection |

#### Shape Nodes (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/shapenodes/SoText2.cpp` | **0%** | 2D raster text – entirely untested |
| `src/shapenodes/SoText3.cpp` | **0%** | 3D extruded text – entirely untested |
| `src/shapenodes/SoAsciiText.cpp` | **12%** | Annotation text |
| `src/shapenodes/SoShape.cpp` | **12%** | Base shape class |
| `src/shapenodes/SoImage.cpp` | **11%** | Image billboard node |
| `src/shapenodes/SoFaceSet.cpp` | **16%** | Non-indexed face set |
| `src/shapenodes/SoIndexedFaceSet.cpp` | **11%** | Core polygon mesh node |
| `src/shapenodes/SoIndexedLineSet.cpp` | **10%** | Line set |
| `src/shapenodes/SoProceduralShape.cpp` | **14%** | Procedural shape API |
| `src/shapenodes/SoIndexedMarkerSet.cpp` | **0%** | Marker set |
| `src/shapenodes/SoIndexedPointSet.cpp` | **0%** | Point set |
| `src/shapenodes/SoIndexedTriangleStripSet.cpp` | **0%** | Triangle strip set |

#### Actions (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoAction.cpp` | **17%** | Base action traversal |
| `src/actions/SoGLRenderAction.cpp` | **21%** | Core rendering traversal |
| `src/actions/SoRayPickAction.cpp` | **12%** | **398 lines** – ray picking |
| `src/actions/SoGetPrimitiveCountAction.cpp` | ~\* | Primitive counting |

#### Scene Management (HIGH PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/misc/SoSceneManager.cpp` | **0%** | Scene lifecycle management |
| `src/misc/SoEventManager.cpp` | **0%** | Event dispatch (SCXML stubs cleaned) |
| `src/misc/SoLightPath.cpp` | **0%** | Lighting traversal path |
| `src/misc/SoLockManager.cpp` | **0%** | Thread safety |

#### Sensors (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/sensors/SoSensorManager.cpp` | **19%** | Queue processing |
| `src/sensors/SoTimerSensor.cpp` | **30%** | Timer callbacks |
| `src/sensors/SoTimerQueueSensor.cpp` | **34%** | |
| `src/sensors/SoPathSensor.cpp` | **0%** | Path-attachment sensor |
| `src/sensors/SoIdleSensor.cpp` | **0%** | Idle callback |

#### Projectors (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/projectors/SbProjector.cpp` | **19%** | Base projector class |
| `src/projectors/SbCylinderSectionProjector.cpp` | **22%** | |
| `src/projectors/SbCylinderSheetProjector.cpp` | **0%** | |
| `src/projectors/SbSphereSheetProjector.cpp` | **0%** | |

#### Shaders (MEDIUM PRIORITY)

| File | Coverage | Notes |
|------|----------|-------|
| `src/shaders/SoShader.cpp` | **6%** | Shader init |
| `src/shaders/SoShaderObject.cpp` | **20%** | Shader object |
| `src/shaders/SoGLSLShaderObject.cpp` | **18%** | GLSL object |
| `src/shaders/SoGLSLShaderProgram.cpp` | **19%** | GLSL program |
| `src/shaders/SoGLARBShaderObject.cpp` | **0%** | Legacy ARB shaders |
| `src/shaders/SoGLARBShaderParameter.cpp` | **0%** | |

#### Profiler Subsystem (LOW – may be kept for dev use only)

| File | Coverage | Notes |
|------|----------|-------|
| `src/profiler/SbProfilingData.cpp` | **0%** | Profiling data |
| `src/profiler/SoProfilerOverlayKit.cpp` | **0%** | Visual overlay |
| `src/profiler/SoProfilerStats.cpp` | **0%** | Statistics |
| `src/profiler/SoProfilingReportGenerator.cpp` | **0%** | Report output |
| `src/profiler/SoScrollingGraphKit.cpp` | **0%** | Scrolling graph |
| `src/profiler/SoProfilerTopEngine.cpp` | **0%** | Top-N engine |
| `src/profiler/SoProfilerTopKit.cpp` | **0%** | Top-N kit |
| `src/profiler/SoProfilerVisualizeKit.cpp` | **0%** | Visualization |

#### HUD Subsystem (LOW)

| File | Coverage | Notes |
|------|----------|-------|
| `src/hud/SoHUD.cpp` | **25%** | HUD root node |
| `src/hud/SoHUDButton.cpp` | **23%** | |
| `src/hud/SoHUDKit.cpp` | **31%** | |
| `src/hud/SoHUDLabel.cpp` | **28%** | |

---

## Priority Queue for Adding Tests

The following areas are ordered by impact (coverage gap × lines affected × API importance):

### Tier 1 — Critical (add tests immediately)

1. **`SbMatrix` / `SbRotation` / `SbViewVolume`** — core math used everywhere.
   - `SbMatrix`: 16% coverage of 447 lines. Missing: `inverse()`, `factor()`,
     `decompose()`, rotation/translation constructors, `transpose()`, LU solve.
   - `SbRotation`: 17% of 173 lines. Missing: slerp, axis-angle construction,
     `getValue(axis, radians)`, conversion to/from matrix.
   - `SbViewVolume`: 26% of project/unproject/planes.
   - `SbDPViewVolume`: 11% of 402 lines — double-precision equivalent.
   - Suggested file: `tests/base/test_sb_math_complete.cpp`

2. **`SoGLRenderAction` / `SoRayPickAction`** — core action traversal.
   - `SoGLRenderAction`: 21% of 344 lines. Missing: transparency sort modes,
     render passes, abort callbacks, caching.
   - `SoRayPickAction`: 12% of 398 lines. Missing: pick radius, sorted picks,
     scene-graph picking with various shape types.
   - Suggested file: `tests/actions/test_glrender_complete.cpp` and
     `tests/actions/test_raypick_complete.cpp`

3. **`SoText2` / `SoText3` / `SoAsciiText`** — text rendering entirely untested.
   - All three text nodes have 0–12% coverage.
   - Suggested: add visual regression tests in `tests/rendering/` for each.

4. **`SoShape` base class** — 12% coverage means the entire shape rendering
   infrastructure is poorly tested.
   - Missing: `generatePrimitives()` path, `computeBBox()`, pick path,
     `shouldGLRender()`, `beginSolidShape()`.
   - Suggested: `tests/nodes/test_shape_base.cpp`

### Tier 2 — Important (next sprint)

5. **`SoOffscreenRenderer`** — 22% of 245 lines. The public API for off-screen
   rendering (used heavily by tests) is itself under-tested.
   - Missing: multiple tile rendering, `getComponents()`, pixel format,
     `writeToRGB()`, `writeToFile()`.
   - Suggested: `tests/rendering/test_offscreen_renderer.cpp`

6. **`SoSceneManager` / `SoEventManager`** — 0% coverage. These are the
   scene lifecycle managers used by every application.
   - Suggested: `tests/misc/test_scene_event_manager.cpp`

7. **`SoFaceSet` / `SoIndexedFaceSet` / `SoIndexedLineSet`** — 10–16% coverage.
   The most common mesh types are barely tested beyond construction.
   - Missing: material/normal/texcoord binding variations, pick paths,
     bounding box computation.
   - Suggested: `tests/nodes/test_mesh_nodes.cpp`

8. **`SoCamera`** — 17% of 197 lines. Core viewing node.
   - Missing: `viewAll()`, `pointAt()`, `getViewVolume()`, `project()`,
     `unproject()`.
   - Suggested: `tests/nodes/test_camera.cpp`

9. **`SoMaterial`** — 16% of 127 lines. Foundational appearance node.

10. **SbVec integer/byte variants** — ~30 vector type files at 0%.
    - These are simple; a single parametric test file can cover all of them.
    - Suggested: `tests/base/test_sb_vec_variants.cpp`

### Tier 3 — Worthwhile (as time allows)

11. **Projectors** (SbCylinderSheetProjector 0%, SbSphereSheetProjector 0%,
    others 20–35%) — used by draggers.
12. **`SbTesselator` / `SbOctTree` / `SbDict` / `SbHeap`** — 0% utility classes.
13. **`SoSensorManager`** — 19%; sensor queue flushing not tested.
14. **`SoGLImage`** — 13% of 463 lines; texture lifecycle.
15. **Shader subsystem** — `SoShaderObject` 20%, `SoGLSLShaderObject` 18%.

### Tier 4 — Deferred (profiler / HUD / shadows)

16. **Profiler** — 8 files at 0%. Consider whether profiling is a supported
    feature before investing test effort here.
17. **HUD** — 23–31%; the overlay HUD is tested visually, but unit tests for
    layout logic are missing.
18. **Shadows** — shadow nodes at 0% unit-test; visual regression test exists
    (`render_shadow`) but only covers one configuration.

---

## Cleanup Done Before This Run

Before the lcov run, the following fully-stubbed / never-implemented APIs were
removed from the build to eliminate false "uncovered" noise:

| Removed | Reason |
|---------|--------|
| `SoByteStream` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranSender` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranReceiver` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranscribe.h` | Header that only forwarded to the two removed classes |
| `SoNormalGenerator::setNumNormals()` | Called `OBOL_OBSOLETED()` only |
| `SoNormalGenerator::setNormal()` | Called `OBOL_OBSOLETED()` only |

---

## How to Reproduce

```bash
# Install lcov (if not present)
sudo apt-get install lcov

# Configure
cmake /path/to/obol \
  -DOBOL_USE_OSMESA=ON \
  -DOBOL_BUILD_TESTS=ON \
  -DOBOL_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -B build_coverage

# Build and run coverage
cmake --build build_coverage -- -j4
cmake --build build_coverage --target coverage

# Open report
xdg-open build_coverage/coverage_report/index.html
```
