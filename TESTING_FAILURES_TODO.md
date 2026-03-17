# PortableGL Test Failures — Investigation Log

Last updated: 2026-03-17  
Build: `OBOL_USE_PORTABLEGL=ON OBOL_USE_SYSTEM_GL=OFF`

---

## Summary

PortableGL is GL 3.3 core (no immediate-mode, no fixed-function pipeline).  
Most failures fall into four root-cause buckets:

| Bucket | Description |
|--------|-------------|
| **A**  | Background gradient rendered as solid colour (wrong attrib index + hasColors=false) |
| **B**  | `buildGradientScene` SoPackedColor byte-order bug (AARRGGBB vs RRGGBBAA) |
| **C**  | Point-set rendering: `colorPerVertex` edge-case or VAO/EBO interaction |
| **D**  | Texture / FBO features not yet ported to the modern GL path |

---

## Known Failures

### 1. `render_background_gradient` — FAIL
**Test:** `render_background_gradient.cpp` — calls `renderer->setBackgroundGradient(bottom, top)`, renders a scene with four coloured shapes.  
**Symptom:** Gradient check failed — top blue ≈ 47.9, bottom blue ≈ 56.4 (gradient absent or inverted).  Observed all pixels ≈ (12, 12, 63) — effectively the GL clear colour, no gradient visible.

**Root cause identified:**  
In `SoOffscreenRenderer.cpp` (the GL3 gradient background path, lines ~757–820), the code:
1. Calls `ms_grad->activateBaseColor()` with **no arguments** (`hasColors = false`).  
   The shader therefore outputs the `baseColor` uniform (default diffuse ≈ grey) rather than per-vertex gradient colours.
2. Puts colour data at vertex attrib **index 1**, but the BaseColor shader (`obol_modern_shaders.h` / `obol_portablegl_shaders.h`) expects colour at attrib **index 3**.

**Fix required (SoOffscreenRenderer.cpp):**
- Change `glVertexAttribPointer(1, 4, ...)` → `glVertexAttribPointer(3, 4, ...)`
- Change `ms_grad->activateBaseColor()` → `ms_grad->activateBaseColor(true, false, false)`
- Disable attrib 1 (`glDisableVertexAttribArray(1)`) since the shader doesn't use it.

**Status:** ❌ Not yet fixed.

---

### 2. `render_gradient` image test — FAIL (RMSE ≈ 80.89)
**Test:** `render_gradient.cpp` — renders `createGradient` scene (which internally calls `buildGradientScene` using SoFaceSet + SoPackedColor).  
**Symptom:** Gradient quad renders as reddish instead of blue.  Control shows a sphere on a blue gradient background.

**Root cause identified:**  
`buildGradientScene` in `tests/testlib/test_scenes.cpp` uses:
```cpp
const uint32_t bottomBlue = 0xff0d0d33u;
const uint32_t topBlue    = 0xff335999u;
```
These were written assuming **AARRGGBB** (alpha-first) packed format.  
SoPackedColor's `orderedRGBA` field uses **RRGGBBAA** (alpha-last).  
So `0xff0d0d33` is interpreted as R=255, G=13, B=13, A=51 (red, semi-transparent), not dark blue.

**Fix required (tests/testlib/test_scenes.cpp):**
```cpp
const uint32_t bottomBlue = 0x0d0d33ffu;  // RRGGBBAA: R=13,G=13,B=51,A=255
const uint32_t topBlue    = 0x335999ffu;  // RRGGBBAA: R=51,G=89,B=153,A=255
```

**Status:** ❌ Not yet fixed.

---

### 3. `render_point_set` — FAIL (red=0, green=0, blue=0, bright=0)
**Test:** `render_point_set.cpp` — four points with PER_VERTEX packed colours (red, green, blue, white).  
**Symptom:** All pixels black.

**Root cause:** Under investigation.  The VERTEXARRAY path in `SoShape::shouldGLRender` routes to `SoPrimitiveVertexCache::renderPoints`, which uses `enableModernAttribs` + `activateBaseColor(color, …)` + `pointindexer->render(glue, TRUE, contextid)`. Line set rendering (same code path, different primitive type) WORKS in sogl_bindings test, suggesting the issue is specific to `GL_POINTS` rendering in PGL or to `colorPerVertex()` returning false for this scene. The `arrays &= ~NORMAL` fix (commit 12d11a27) was applied correctly.

**Possible causes:**
- PGL `draw_point` clipZ-mask check may clip all points if projection is wrong for this ortho camera setup.
- `colorPerVertex()` may be returning false (all 4 packed colours could hash to the same firstcolor if `packedptr` isn't pointing to the right data at `open()` time).
- Point size of 20 pixels may interact with some PGL viewport boundary.

**Status:** ❌ Under investigation.

---

### 4. `render_indexed_point_set` — FAIL (no red pixels)
**Test:** `render_indexed_point_set.cpp` — SoIndexedPointSet with emissive red material.  
**Symptom:** No red pixels found.

**Root cause:** Same point-rendering issue as #3.

**Status:** ❌ Under investigation (blocked by #3).

---

### 5. `render_sogl_bindings` (point-set variants) — FAIL
**Test:** `render_sogl_bindings.cpp` — all 8 SoPointSet binding variants produce `nonbg=0`; SoIndexedLineSet variants pass.  
**Symptom:** Point sets produce zero non-background pixels; line sets work.

**Root cause:** Same as #3. Line rendering (`GL_LINES`) works; point rendering (`GL_POINTS`) does not.

**Status:** ❌ Blocked by #3.

---

### 6. `render_checker_texture` — FAIL (all white, no black)
**Test:** Checkerboard-textured cube via procedural SoTexture2.  
**Symptom:** Cube renders but texture is missing (all white, checker pattern absent).

**Root cause:** SoTexture2 upload or texture sampling not working in the PGL modern shader path. The modern Phong/BaseColor shaders have `hasTexCoords`/`hasTexture` uniforms that need to be set correctly, and the texture sampler path in PGL may not be exercised.

**Status:** ❌ Not yet investigated in depth.

---

### 7. `render_scene_texture` — FAIL (scene appears blank)
**Test:** Flat quad with SoSceneTexture2 render-to-texture (cone sub-scene).  
**Symptom:** Scene appears blank.

**Root cause:** SoSceneTexture2 requires FBO render-to-texture.  FBO support in PGL is limited; the render-to-texture sub-pass likely silently fails.

**Is it possible with GL3/PGL?** PGL does include basic FBO functions but their completeness is limited. This may require a redesign to use PGL's native framebuffer or to skip in PGL builds.

**Status:** ❌ Not yet investigated. May require xfail or redesign.

---

### 8. `render_image_node` — FAIL (no red/green pixels)
**Test:** SoImage node — red/green checkerboard image centered in viewport.  
**Symptom:** No coloured pixels found.

**Root cause:** SoImage renders via `glBitmap`/`glDrawPixels` in the legacy path, neither of which exists in PGL. The modern path for SoImage needs to upload to a texture and draw a quad.

**Is it possible with GL3/PGL?** Yes, but requires a modern GL path for SoImage.

**Status:** ❌ Needs modern GL path for SoImage.

---

### 9. `render_shadow_advanced` — FAIL
**Test:** Advanced shadow mapping.  
**Symptom:** Execution failure / crash.

**Root cause:** Shadow mapping requires depth textures and FBO shadow passes which depend on features beyond PGL's current capability.

**Is it possible with GL3/PGL?** Unlikely without significant PGL enhancement; recommend xfail.

**Status:** ❌ Likely needs xfail.

---

### 10. `render_offscreen_advanced` — FAIL (segfault / execution failure)
**Test:** Advanced offscreen renderer test.  
**Symptom:** Crashes.

**Root cause:** Unknown — needs investigation. Likely FBO-related or multi-context.

**Status:** ❌ Not yet investigated.

---

### 11. `render_quad_mesh_deep` test 7 (wireframe) — FAIL
**Test:** 4×4 SoQuadMesh with PER_FACE colours; test 7 is wireframe style.  
**Symptom:** Wireframe case produces no non-background pixels.

**Root cause:** `SoDrawStyle::LINES` rendering via SoQuadMesh in the modern path may not activate BaseColor correctly for wireframe mode, or `glPolygonMode(GL_LINE)` doesn't exist in PGL (GL3 core removed polygon mode for front/back independently; PGL may stub or skip it).

**Status:** ❌ Under investigation.

---

### 12. Image comparison tests with RMSE slightly above threshold

| Test | RMSE | Threshold | Notes |
|------|------|-----------|-------|
| `render_lighting_image_test` | ~15.3 | 5.0 | Phong shading difference |
| `render_texture_image_test` | ~18.5 | 15.0 | Texture sampling difference |
| `render_indexed_line_set_image_test` | ~19.9 | 15.0 | Close; may be threshold issue |
| `render_quad_viewport_lod_image_test` | ~21.8 | 20.0 | Close |
| `render_environment_image_test` | ~47.8 | 20.0 | Env map / reflection |
| `render_texture3_image_test` | ~56.7 | 20.0 | 3D texture |
| `render_bump_map_image_test` | ~67.7 | 20.0 | Normal-map shading |
| `render_texture_transform_image_test` | ~93.1 | 20.0 | Texture matrix |
| `render_alpha_test_image_test` | ~197.0 | 20.0 | Alpha test |

Most of these are likely due to texture features not yet working (bugs #6, #7 above) or small rendering differences between PGL's C-function shader and the GLSL reference.

**Status:** ❌ Dependent on texture fixes; thresholds may need tuning for PGL.

---

### 13. `render_gl_big_image` — FAIL
**Test:** Large image rendered via legacy GL path (glBegin/glVertex).  
**Symptom:** Execution failure.

**Root cause:** Uses immediate-mode GL (`glBegin`/`glEnd`/`glVertex`) which does not exist in GL3 core or PGL.

**Is it possible with GL3/PGL?** Needs a modern GL reimplementation (VAO/VBO quad with a texture).

**Status:** ❌ Needs modern GL redesign.

---

## Fixes Applied in This Branch

- `src/shapenodes/SoShape.cpp`: Fixed `arrays &= NORMAL` → `arrays &= ~NORMAL` (commit 12d11a27) — was causing point/line arrays flag to be set to just the NORMAL bit instead of clearing it.
- `src/elements/GL/SoGLViewingMatrixElement.cpp`: Upload VIEW*MODEL to SoGLModernState so scenes without an explicit SoTransform get correct MV matrix (commit 583b9987).
- `src/shapenodes/SoFaceSet.cpp`, `SoIndexedFaceSet.cpp`: Added PER_VERTEX colour support in the modern GL path (commit 583b9987).

## Pending Fixes (Next Steps)

1. **Gradient background (SoOffscreenRenderer.cpp):** Move gradient colour to attrib 3, call `activateBaseColor(true, false, false)`.
2. **buildGradientScene (test_scenes.cpp):** Fix SoPackedColor byte order: `0xff0d0d33u` → `0x0d0d33ffu`, `0xff335999u` → `0x335999ffu`.
3. **Point set debugging:** Add targeted debug output to `renderPoints` to trace why `GL_POINTS` produces black output while `GL_LINES` works.
4. **Texture path:** Investigate SoTexture2 / SoGLImage upload in PGL context.
5. **xfail candidates:** `render_shadow_advanced`, `render_gl_big_image`, `render_scene_texture` (FBO), `render_image_node` (glDrawPixels).
