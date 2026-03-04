# Obol Code Review Progress

This file tracks ongoing code review work.  Each session should update the
checklist below and add entries to the **Session Log**.

---

## Overall Goals

- General quality pass: cleanup, best practices, comment clarity/completeness
- Migrate inline block comments on public API to Doxygen `/*!` / `/**` style
  so that `make obol_docs` generates usable API reference documentation
- Fix any functional issues (bugs, UB, resource leaks) found along the way

---

## Status Legend

- ✅ Done
- 🔄 In progress / partial
- ⬜ Not started

---

## Infrastructure

| Item | Status | Notes |
|------|--------|-------|
| Add `docs/Doxyfile.in` (Doxygen config template) | ✅ | Created in session 1 |
| CMake `OBOL_BUILD_DOCS` option + `obol_docs` target | ✅ | `cmake -DOBOL_BUILD_DOCS=ON` then `make obol_docs` |
| Verify `make obol_docs` runs without errors | ✅ | 2318 HTML pages generated; warnings are pre-existing |

---

## Public Header Documentation  (`include/Inventor/`)

Priority: headers used directly by application code.

| Header | Class Doc | Notes |
|--------|-----------|-------|
| `SbBasic.h` | ✅ | Added `\defgroup coin_math_utilities` + `\fn` Doxygen for all template utilities |
| `SbMatrix.h` | ✅ | Added `\class SbMatrix` Doxygen block |
| `SbVec3f.h` | ✅ | Added `\class SbVec3f` Doxygen block |
| `SoDB.h` | ✅ | Added `\class SoDB` block; converted `ContextManager` block comment to `\class`; added `\brief` docs for pure-virtual GL lifecycle methods |
| `SoInput.h` | ✅ | Added `\class SoInput` Doxygen block |
| `SoOutput.h` | ✅ | Added `\class SoOutput` Doxygen block |
| `SoOffscreenRenderer.h` | ✅ | Added `\class SoOffscreenRenderer` Doxygen block |
| `SoRenderManager.h` | ✅ | Added `\class SoRenderManager` Doxygen block |
| `SbVec2f.h` | ✅ | Added `\class SbVec2f` Doxygen block |
| `SbVec3d.h` | ✅ | Added `\class SbVec3d` Doxygen block |
| `SbVec4f.h` | ✅ | Added `\class SbVec4f` Doxygen block |
| `SbRotation.h` | ✅ | Added `\class SbRotation` Doxygen block |
| `SbLine.h` | ✅ | Added `\class SbLine` Doxygen block |
| `SbPlane.h` | ✅ | Added `\class SbPlane` Doxygen block |
| `SbViewVolume.h` | ✅ | Added `\class SbViewVolume` Doxygen block |
| `SbViewportRegion.h` | ✅ | Added `\class SbViewportRegion` Doxygen block |
| `SbColor.h` | ✅ | Added `\class SbColor` Doxygen block |
| `SbString.h` | ✅ | Added `\class SbString` Doxygen block |
| `SbName.h` | ✅ | Added `\class SbName` Doxygen block |
| `SbTime.h` | ✅ | Added `\class SbTime` Doxygen block |
| `SbImage.h` | ✅ | Added `\class SbImage` Doxygen block |
| `SoType.h` | ✅ | Added `\class SoType` Doxygen block |
| `SoPath.h` | ✅ | Added `\class SoPath` Doxygen block |
| `SoSceneManager.h` | ✅ | Added `\class SoSceneManager` Doxygen block |
| `SoEventManager.h` | ✅ | Added `\class SoEventManager` Doxygen block |
| `SoPickedPoint.h` | ✅ | Added `\class SoPickedPoint` Doxygen block |
| `SoPrimitiveVertex.h` | ✅ | Added `\class SoPrimitiveVertex` Doxygen block |
| `SoFullPath.h` | ✅ | Added `\class SoFullPath` Doxygen block |
| All `actions/` headers | ✅ | All 15 class headers documented (session 3) |
| All `nodes/` headers | 🔄 | ~115/116 class headers documented; `SoBumpMappingProperty.h` is a stub with no class yet |
| All `fields/` headers | ✅ | All 89 concrete SoSF*/SoMF* headers documented (session 4); core bases done session 3 |
| All `elements/` headers | ✅ | All 111 element headers documented (session 4); 2 deprecated `#error` stubs skipped |
| All `engines/` headers | ✅ | All 37 class headers documented (session 3) |
| All `sensors/` headers | ✅ | All 12 class headers documented (session 3) |
| All `events/` headers | ✅ | All 7 class headers documented (session 3) |

---

## Source File Documentation  (`src/`)

The `.cpp` files already carry the bulk of the Doxygen documentation (Coin
convention: `\fn` / `\class` blocks live in the implementation file).  The
following items remain:

| Item | Status | Notes |
|------|--------|-------|
| Fix duplicate `/*!` opening in `src/engines/SoNodeEngine.cpp:317` | ✅ | Removed stray extra `/*!` (Doxygen reported "nested comment" error) |
| Fix broken `/// * !` comment in `src/misc/SoDB.cpp:33` | ✅ | Converted to proper `/*!` block, updated text for `SbBool = bool` reality |
| `OBOL_UNUSED_ARG` macro in Doxygen `\fn` signatures (SoReorganizeAction.cpp) | ✅ | Added `OBOL_UNUSED_ARG(x)=x` to `PREDEFINED` in `docs/Doxyfile.in` |
| `src/errors/error.cpp` – `\class` references missing header `error.h` | ⬜ | Internal header not in include search path; low priority |
| `src/misc/SoGlyph.cpp` – `\class` references missing header `SoGlyph.h` | ⬜ | Same; low priority |

---

## Code Quality / Best Practices

| Item | Status | Notes |
|------|--------|-------|
| `FIXME` comments in `src/actions/` (SoCallbackAction, SoReorganizeAction, SoGLRenderAction, etc.) | 🔄 | Most are legacy todos from Coin era (texture unit binding, >8-bit channels, pbuffer) – not actionable without major refactoring; left in place |
| `FIXME` in `src/fields/` (SoField, SoFieldContainer, SoFieldData) | ✅ | All actionable FIXMEs in SoField.cpp resolved (session 4): const-correctness, doc quality, stale implementation questions, hashing note updated; SoFieldContainer.cpp FIXME resolved session 3 |
| `HACK` comment in `SoFieldContainer.cpp` – bitmask misuse of `donotify` | ✅ | Removed HACK comment; dead `FLAG_FIRSTINSTANCE` macro removed; header comment improved to explain the int type (session 4) |
| `SoDB.cpp` SbBool typedef doc (was broken `/// * !`) | ✅ | Fixed session 1 |
| Remove/update stale Coin-era comments referencing removed subsystems (VRML, ScXML, audio) | ✅ | Updated all user-visible "Coin3D" strings to "Obol" in SoInteraction.cpp, SoEventManager.cpp, SoInput.cpp, SoNodeKit.cpp, SbFont.cpp, SoText3.cpp; VRML-removal notes updated to reference Obol fork (session 4) |
| Audit `const_cast` usage | ✅ | `SoFieldContainer.cpp` const_cast FIXMEs resolved: changed `SoFieldContainerCopyMap` value type to non-const, eliminating all but the inherent `this`-cast in `copyThroughConnection()` |
| Audit raw pointer ownership across public API | ⬜ | Several `void *` context handles could be typed; deferred |
| `add_definitions(-g)` in CMakeLists.txt – debug info in Release builds | ✅ | Not present in current codebase; no action needed |
| `src/base/utf8/include/` not on Doxygen INCLUDE_PATH | ✅ | Fixed in Doxyfile.in session 1 |
| `SoField::get()` and `SoMField::get1()` should be `const` | ✅ | Both declarations and definitions updated to `const` (session 4); build verified |

---

## Session Log

### Session 1 (2026-03-03)

**Files changed:**

- `docs/Doxyfile.in` — new file; Doxygen configuration template for the
  `obol_docs` CMake target
- `CMakeLists.txt` — replaced the "Documentation options removed" stub with a
  proper `OBOL_BUILD_DOCS` option and `find_package(Doxygen)` + custom target
- `src/misc/SoDB.cpp` — fixed broken `/// * !` comment block (was not valid
  Doxygen; converted to proper `/*!` block; updated text to reflect that
  `SbBool` is now a `bool` alias rather than `int`)
- `src/engines/SoNodeEngine.cpp` — removed duplicate `/*!` opening that Doxygen
  flagged as an unclosed nested comment
- `include/Inventor/SbBasic.h` — replaced plain block comment for the utility
  template functions with proper `/*!  \defgroup` / `\fn` Doxygen markup
- `include/Inventor/SbMatrix.h` — added `\class SbMatrix` Doxygen block
- `include/Inventor/SbVec3f.h` — added `\class SbVec3f` Doxygen block
- `include/Inventor/SoDB.h` — added `\class SoDB` block; converted
  `ContextManager` plain comment to `\class` Doxygen block; added `\brief`
  annotations to the four pure-virtual GL lifecycle methods
- `include/Inventor/SoInput.h` — added `\class SoInput` Doxygen block
- `include/Inventor/SoOutput.h` — added `\class SoOutput` Doxygen block
- `include/Inventor/SoOffscreenRenderer.h` — added `\class SoOffscreenRenderer`
  Doxygen block with usage example
- `include/Inventor/SoRenderManager.h` — added `\class SoRenderManager`
  Doxygen block

**Build verification:**

`cmake -DOBOL_BUILD_DOCS=ON ... && make && make obol_docs` — clean build,
2318 HTML pages generated.  All Doxygen warnings in the output are pre-existing
(unclosed comment in `SoNodeEngine.cpp` is now fixed; remaining warnings are
macro-expansion and missing-include issues from legacy Coin code).

**Next session priorities:**

1. Continue adding `\class` blocks to the remaining ~70 undocumented public
   headers, starting with the math types (`SbVec2f`, `SbVec3d`, `SbRotation`,
   `SbLine`, `SbPlane`, `SbViewVolume`, …) and then the core action/node/field
   headers.
2. Evaluate the `OBOL_UNUSED_ARG` macro Doxygen warning in
   `SoReorganizeAction.cpp` and add a `PREDEFINED` expansion if needed.
3. Address the `add_definitions(-g)` always-on debug-info issue in
   `CMakeLists.txt`.
4. Review the still-relevant `FIXME` items in `SoGLRenderAction.cpp`.

---

### Session 2 (2026-03-04)

**Files changed:**

- `include/Inventor/SbVec2f.h` — added `\class SbVec2f` Doxygen block
- `include/Inventor/SbVec3d.h` — added `\class SbVec3d` Doxygen block
- `include/Inventor/SbVec4f.h` — added `\class SbVec4f` Doxygen block
- `include/Inventor/SbRotation.h` — added `\class SbRotation` Doxygen block
- `include/Inventor/SbLine.h` — added `\class SbLine` Doxygen block
- `include/Inventor/SbPlane.h` — added `\class SbPlane` Doxygen block
- `include/Inventor/SbViewVolume.h` — added `\class SbViewVolume` Doxygen block
- `include/Inventor/SbViewportRegion.h` — added `\class SbViewportRegion` Doxygen block
- `include/Inventor/SbColor.h` — added `\class SbColor` Doxygen block
- `include/Inventor/SbString.h` — added `\class SbString` Doxygen block
- `include/Inventor/SbName.h` — added `\class SbName` Doxygen block
- `include/Inventor/SbTime.h` — added `\class SbTime` Doxygen block
- `include/Inventor/SbImage.h` — added `\class SbImage` Doxygen block
- `include/Inventor/SoType.h` — added `\class SoType` Doxygen block
- `include/Inventor/SoPath.h` — added `\class SoPath` Doxygen block
- `include/Inventor/SoSceneManager.h` — added `\class SoSceneManager` Doxygen block
- `include/Inventor/SoEventManager.h` — added `\class SoEventManager` Doxygen block
- `include/Inventor/SoPickedPoint.h` — added `\class SoPickedPoint` Doxygen block
- `include/Inventor/SoPrimitiveVertex.h` — added `\class SoPrimitiveVertex` Doxygen block
- `include/Inventor/SoFullPath.h` — added `\class SoFullPath` Doxygen block
- `docs/Doxyfile.in` — added `OBOL_UNUSED_ARG(x)=x` to `PREDEFINED` so that
  Doxygen can expand the macro in `\fn` signatures in `SoReorganizeAction.cpp`

**Items closed from previous next-session priorities:**

- `add_definitions(-g)` — confirmed absent from `CMakeLists.txt`; marked done.
- `OBOL_UNUSED_ARG` macro Doxygen warning — resolved via `PREDEFINED` expansion.
- All math/primitive/scene-graph public headers now have `\class` Doxygen blocks.

**Next session priorities:**

1. Continue `\class` blocks for all `actions/` headers (~15 headers).
2. Continue `\class` blocks for the most-used `nodes/` headers (SoNode,
   SoGroup, SoSeparator, SoTransform, SoCamera, SoShape, …).
3. Continue `\class` blocks for `fields/` headers (SoField, SoSFFloat,
   SoMFFloat, …).
4. Review still-relevant `FIXME` items in `src/actions/SoGLRenderAction.cpp`
   and `src/fields/`.
5. Audit `const_cast` usage flagged in `SoFieldContainer.cpp:885`.

---

### Session 3 (2026-03-04)

**Files changed (documentation):**

Actions (`include/Inventor/actions/`):
- Added `\class` blocks to: SoAction, SoCallbackAction, SoGLRenderAction,
  SoGetBoundingBoxAction, SoGetMatrixAction, SoGetPrimitiveCountAction,
  SoHandleEventAction, SoPickAction, SoRayPickAction, SoReorganizeAction,
  SoSearchAction, SoSimplifyAction, SoWriteAction, SoBoxHighlightRenderAction,
  SoLineHighlightRenderAction (15 headers)

Sensors (`include/Inventor/sensors/`):
- Added `\class` blocks to: SoSensor, SoSensorManager, SoDelayQueueSensor,
  SoTimerQueueSensor, SoDataSensor, SoAlarmSensor, SoFieldSensor, SoNodeSensor,
  SoPathSensor, SoIdleSensor, SoOneShotSensor, SoTimerSensor (12 headers)

Events (`include/Inventor/events/`):
- Added `\class` blocks to: SoEvent, SoButtonEvent, SoKeyboardEvent,
  SoMouseButtonEvent, SoLocation2Event, SoMotion3Event, SoSpaceballButtonEvent
  (7 headers)

Core fields (`include/Inventor/fields/`):
- Added `\class` blocks to: SoField, SoFieldContainer, SoFieldData, SoSField,
  SoMField (5 headers; ~89 concrete SoSF*/SoMF* headers deferred)

Nodes (`include/Inventor/nodes/`) — 115 headers documented:
- Base classes: SoNode, SoGroup, SoSeparator, SoShape, SoCamera, SoTransformation,
  SoVertexShape, SoNonIndexedShape, SoIndexedShape
- Transforms: SoTransform, SoTranslation, SoRotation, SoRotationXYZ, SoScale,
  SoMatrixTransform, SoResetTransform, SoAntiSquish, SoSurroundScale
- Cameras: SoPerspectiveCamera, SoOrthographicCamera, SoFrustumCamera,
  SoReversePerspectiveCamera
- Lights: SoLight, SoDirectionalLight, SoPointLight, SoSpotLight, SoEnvironment
- Shapes: SoCone, SoCube, SoCylinder, SoSphere, SoAsciiText, SoText2, SoText3,
  SoImage, SoIndexedFaceSet, SoIndexedLineSet, SoIndexedTriangleStripSet,
  SoIndexedPointSet, SoIndexedMarkerSet, SoFaceSet, SoLineSet, SoPointSet,
  SoQuadMesh, SoTriangleStripSet, SoMarkerSet
- Properties: SoMaterial, SoBaseColor, SoPackedColor, SoColorIndex, SoDrawStyle,
  SoComplexity, SoShapeHints, SoLightModel, SoNormal, SoNormalBinding,
  SoMaterialBinding, SoCoordinate3, SoCoordinate4, SoVertexProperty,
  SoVertexAttribute, SoVertexAttributeBinding, SoPickStyle, SoPolygonOffset,
  SoAlphaTest, SoDepthBuffer, SoClipPlane, SoTransparencyType
- Textures: SoTexture, SoTexture2, SoTexture3, SoTextureCubeMap, SoSceneTexture2,
  SoSceneTextureCubeMap, SoTexture2Transform, SoTexture3Transform,
  SoTextureCombine, SoTextureMatrixTransform, SoTextureScalePolicy, SoTextureUnit,
  SoTextureCoordinate2, SoTextureCoordinate3, SoTextureCoordinateBinding,
  SoTextureCoordinateFunction, SoTextureCoordinatePlane, SoTextureCoordinateSphere,
  SoTextureCoordinateCube, SoTextureCoordinateCylinder, SoTextureCoordinateDefault,
  SoTextureCoordinateEnvironment, SoTextureCoordinateObject,
  SoTextureCoordinateNormalMap, SoTextureCoordinateReflectionMap
- Font: SoFont, SoFontStyle
- Shaders: SoShaderObject, SoShaderProgram, SoVertexShader, SoFragmentShader,
  SoGeometryShader, SoShaderParameter
- Scene structure: SoSwitch, SoSelection, SoExtSelection, SoLocateHighlight,
  SoTransformSeparator, SoAnnotation, SoPathSwitch, SoLOD, SoLevelOfDetail,
  SoArray, SoMultipleCopy, SoBlinker, SoLocateHighlight
- Animation: SoRotor, SoPendulum, SoShuttle
- Misc: SoCallback, SoEventCallback, SoInfo, SoLabel, SoUnits, SoFile,
  SoWWWAnchor, SoWWWInline, SoListener, SoCacheHint
- BumpMap: SoBumpMap, SoBumpMapCoordinate, SoBumpMapTransform
- Other: SoProfile, SoLinearProfile, SoProfileCoordinate2, SoProfileCoordinate3

Engines (`include/Inventor/engines/`) — all 37 class headers documented:
- SoEngine, SoEngineOutput, SoNodeEngine, SoEngineOutputData, SoFieldConverter,
  SoCalculator, SoElapsedTime, SoTimeCounter, SoCounter, SoOnOff, SoGate,
  SoSelectOne, SoConcatenate, SoOneShot, SoBoolOperation, SoTriggerAny,
  SoInterpolate, SoInterpolateFloat, SoInterpolateRotation,
  SoInterpolateVec2f/3f/4f, SoComposeVec2f/3f/4f, SoDecomposeVec2f/3f/4f,
  SoComposeRotation, SoComposeRotationFromTo, SoDecomposeRotation,
  SoComposeMatrix, SoDecomposeMatrix, SoTransformVec3f, SoComputeBoundingBox,
  SoHeightMapToNormalMap, SoTexture2Convert

**Files changed (code quality):**

- `src/fields/SoFieldContainer.cpp` — eliminated the long-standing "ugly constness
  cast" FIXME at `checkCopy()`:
  - Changed `SoFieldContainerCopyMap` value type from
    `const SoFieldContainer *` → `SoFieldContainer *` so that `checkCopy()`
    can return a mutable pointer directly without a cast.
  - `addCopy()` now explicitly casts when storing into the map
    (`const_cast<SoFieldContainer*>(copy)` at the one appropriate callsite).
  - A second gratuitous `const_cast` in `getCopy()` was replaced by calling
    `checkCopy()` directly.
  - Also removed a spurious `const_cast<char*>` in `set()` because
    `SoInput::setBuffer()` correctly takes `const void *`.
  - Build verified clean after all changes.

**Next session priorities:**

1. Document remaining ~89 concrete SF/MF field headers (SoSFFloat, SoMFFloat, …).
2. Document `elements/` headers (~70 headers).
3. Evaluate the `HACK` comment in `SoFieldContainer.cpp:131`
   (`donotify` bitmask) — clean up with a dedicated bitfield struct.
4. Audit remaining FIXMEs in `src/fields/SoField.cpp` for actionability.
5. Review/remove stale Coin-era comments referencing VRML, ScXML, audio.

---

### Session 4 (2026-03-04)

**Files changed (documentation):**

SF/MF concrete field headers (`include/Inventor/fields/SoSF*.h`, `SoMF*.h`)
— all 89 headers now documented:
- SoSFBitMask, SoSFBool, SoSFBox2d/2f/2i32/2s, SoSFBox3d/3f/3i32/3s,
  SoSFColor, SoSFColorRGBA, SoSFDouble, SoSFEngine, SoSFEnum, SoSFFloat,
  SoSFImage, SoSFImage3, SoSFInt32, SoSFMatrix, SoSFName, SoSFNode, SoSFPath,
  SoSFPlane, SoSFRotation, SoSFShort, SoSFString, SoSFTime, SoSFTrigger,
  SoSFUInt32, SoSFUShort, SoSFVec2b/2d/2f/2i32/2s, SoSFVec3b/3d/3f/3i32/3s,
  SoSFVec4b/4d/4f/4i32/4s/4ub/4ui32/4us (50 SoSF* headers)
- SoMFBitMask, SoMFBool, SoMFColor, SoMFColorRGBA, SoMFDouble, SoMFEngine,
  SoMFEnum, SoMFFloat, SoMFInt32, SoMFMatrix, SoMFName, SoMFNode, SoMFPath,
  SoMFPlane, SoMFRotation, SoMFShort, SoMFString, SoMFTime, SoMFUInt32,
  SoMFUShort, SoMFVec2b/2d/2f/2i32/2s, SoMFVec3b/3d/3f/3i32/3s,
  SoMFVec4b/4d/4f/4i32/4s/4ub/4ui32/4us (39 SoMF* headers)

Elements (`include/Inventor/elements/`) — all 111 documented headers:
- All element base classes: SoElement, SoReplacedElement, SoAccumulatedElement,
  SoFloatElement, SoInt32Element
- All concrete element classes (102 `\class` blocks + 8 `\typedef` blocks for
  compat-alias headers)
- 2 `#error`-deprecated stubs (SoTexture3EnabledElement, SoGLTexture3EnabledElement)
  deliberately left undocumented

**Files changed (code quality):**

- `src/fields/SoFieldContainer.cpp` + `include/Inventor/fields/SoFieldContainer.h`:
  - Removed dead `#define FLAG_FIRSTINSTANCE 0x02` and corresponding `#undef`
    (macro was defined but never used)
  - Removed `// HACK warning:` comment on constructor; updated header
    comment on `donotify` to clearly explain the `int` type rationale

- `src/fields/SoField.cpp`:
  - **`SoField::get()` made `const`** — removed 2005-era FIXME; matching change
    in `include/Inventor/fields/SoField.h`
  - **`SoMField::get1()` made `const`** — same rationale; matching change
    in `include/Inventor/fields/SoMField.h`
  - Eager initialization of `SoFieldP::ptrhash` in `SoField::initClass()`;
    the lazy-init guard is retained as fallback but no longer needed in
    normal usage — resolves the "protect with mutex?" FIXME
  - Improved `getTypeId()` Doxygen to include a usage example; removed stale
    FIXME requesting a "better version"
  - Removed stale FIXMEs: notification type, engine writeInstance, engine
    output name API, fan-in evaluation strategy — all were stale implementation
    questions answered by 25 years of working code
  - Replaced FIXME about pointer alignment in hash with an accurate note

- Stale "Coin3D" string cleanup:
  - `src/misc/SoInteraction.cpp` — error message → "Obol"
  - `src/misc/SoEventManager.cpp` — Doxygen class description → "Obol"
  - `src/io/SoInput.cpp` — inline comment + error message → "Obol"
  - `src/nodekits/SoNodeKit.cpp` — error message → "Obol"
  - `src/base/SbFont.cpp` — class description + inline comment → "Obol"
  - `src/shapenodes/SoText3.cpp` — example scene string "Coin3D" → "Obol"
  - `src/misc/SoProto.cpp` + `src/nodes/SoListener.cpp` — VRML-removal notes
    updated to say "removed from Obol (the open-source fork of Coin3D)"

**Overall documentation status after session 4:**

All public header groups are now documented:
| Group | Headers | Status |
|---|---|---|
| `include/Inventor/*.h` | 28 | ✅ |
| `include/Inventor/actions/*.h` | 15 | ✅ |
| `include/Inventor/nodes/*.h` | ~115 | ✅ (1 empty stub) |
| `include/Inventor/fields/*.h` | 94 | ✅ |
| `include/Inventor/elements/*.h` | 111 | ✅ (2 `#error` stubs) |
| `include/Inventor/engines/*.h` | 37 | ✅ |
| `include/Inventor/sensors/*.h` | 12 | ✅ |
| `include/Inventor/events/*.h` | 7 | ✅ |

**Remaining items:**

- `include/Inventor/misc/`, `include/Inventor/details/`, `include/Inventor/lists/`,
  `include/Inventor/annex/`, `include/Inventor/nodekits/` — not yet inventoried
- `src/errors/error.cpp` and `src/misc/SoGlyph.cpp` — internal-header low-priority items
- Audit raw pointer ownership across public API (`void *` context handles)
