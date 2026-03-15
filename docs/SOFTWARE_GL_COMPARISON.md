# Software GL Backend Comparison: OSMesa vs PortableGL

## Executive Summary

This document analyses what it would take to replace OSMesa with
[PortableGL](https://github.com/rswinkle/PortableGL) as Obol's software-rasterized
OpenGL backend, and identifies the major problems and obstacles.

The core challenge is architectural: Obol currently relies heavily on OpenGL's
**fixed-function pipeline** (immediate mode, `glBegin`/`glEnd`, matrix stacks,
`glMaterial`, `glLight`, display lists, accumulation buffer), which was removed in
OpenGL 3.0 and is not implemented by PortableGL.  A compat-layer approach (emulating
fixed-function calls on top of PortableGL) was investigated and found too complex and
too slow.  The correct path is to **modernise Obol's rendering code** to target the
OpenGL 3.x core profile — which PortableGL *does* implement — and only then use
PortableGL as a software backend.

---

## 1.  What is PortableGL?

PortableGL (<https://github.com/rswinkle/PortableGL>) is a single-header C library
that implements a significant subset of the **OpenGL 3.x core profile** in software,
without depending on any platform GL library or GPU driver.

| Property | Value |
|----------|-------|
| API level | OpenGL 3.x core profile |
| GLSL | Yes — user-supplied C shader functions |
| Distribution | Single header (`portablegl.h`) |
| Platform | Any C99/C++ platform (no X11, Wayland, or GPU required) |
| Context | `glContext` struct; `init_glContext()` / `set_glContext()` |
| Fixed-function pipeline | **Not supported** (removed in GL 3.0 core) |
| Display lists | **Not supported** |
| Accumulation buffer | **Not supported** |

### Comparison with OSMesa

| Feature | OSMesa | PortableGL |
|---------|--------|------------|
| OpenGL version | 2.0 + extensions | 3.x core profile |
| Fixed-function pipeline | ✓ (gl{Begin,End,Vertex,Normal,Material,Light,…}) | ✗ |
| Immediate mode | ✓ | ✗ |
| Matrix stacks (glMatrixMode, glPushMatrix, …) | ✓ | ✗ |
| Display lists | ✓ | ✗ |
| Accumulation buffer | ✓ | ✗ |
| VAO / VBO | ✓ (ARB extensions) | ✓ (core) |
| GLSL shaders | ✓ (GLSL 1.10 / ARB_shader_objects) | ✓ (own dialect) |
| FBO offscreen | ✓ (GL_EXT_framebuffer_object) | ✓ (core) |
| 2D/3D textures | ✓ | ✓ |
| Headless operation | ✓ (native, via OSMesaMakeCurrent) | ✓ (pixel buffer, no windowing) |
| Build complexity | High (submodule + name-mangling) | Low (single header) |
| Maintenance | Upstream Mesa (large, slow-moving) | Small single-developer library |
| Performance | Moderate (Mesa software pipeline) | Comparable (custom rasterizer) |
| CI dependency | Requires submodule + name-mangled build | Drop-in header |

---

## 2.  Current Obol OpenGL Usage

The following table maps each fixed-function OpenGL feature Obol uses to the
corresponding modern (GL 3.x core) replacement that must be written.

### 2.1  Geometry Submission — Immediate Mode

**Affected files** (23 source files contain `glBegin` / `glEnd` / `glVertex` / `glNormal`):

| File | Use case |
|------|----------|
| `src/rendering/SoGL.cpp` | Cone, cylinder, sphere, cube primitives |
| `src/shapenodes/SoFaceSet.cpp` | Polygon face rendering |
| `src/shapenodes/SoTriangleStripSet.cpp` | Triangle strip rendering |
| `src/shapenodes/SoQuadMesh.cpp` | Quad mesh rendering |
| `src/shapenodes/SoLineSet.cpp` | Polyline rendering |
| `src/shapenodes/SoIndexedPointSet.cpp` | Point cloud rendering |
| `src/shapenodes/SoText2.cpp` | 2D text bitmaps |
| `src/shapenodes/SoText3.cpp` | 3D extruded text |
| `src/shapenodes/SoAsciiText.cpp` | ASCII text |
| `src/shapenodes/soshape_bigtexture.cpp` | Large-texture tiling |
| `src/shapenodes/soshape_trianglesort.cpp` | Transparency-sorted triangles |
| `src/bundles/SoMaterialBundle.cpp` | Per-vertex material updates |
| `src/caches/SoPrimitiveVertexCache.cpp` | Primitive vertex cache |
| `src/actions/SoGLRenderAction.cpp` | Accumulation buffer passes |
| `src/nodes/SoCamera.cpp` | Camera clear |
| `src/nodes/SoExtSelection.cpp` | Selection highlight |
| `src/hud/SoHUDButton.cpp` | HUD button rendering |

**Replacement**: Pre-tessellate all geometry into CPU-side vertex arrays, upload once
to a VBO, and draw with `glDrawArrays` / `glDrawElements`.  Per-frame VBO updates
required when geometry changes.

### 2.2  Matrix Stacks

**Affected files**: `SoGLModelMatrixElement.cpp`, `SoGLProjectionMatrixElement.cpp`,
`SoGLMultiTextureMatrixElement.cpp`, `SoGLRenderAction.cpp`

Fixed-function calls: `glMatrixMode`, `glPushMatrix`, `glPopMatrix`, `glLoadMatrixf`,
`glMultMatrixf`, `glLoadIdentity`.

**Replacement**: All matrix operations are already computed in CPU-side `SbMatrix`
objects by the scene graph traversal.  The result matrices (MVP, normal matrix) must
be passed to shaders as `uniform mat4` / `uniform mat3`.  The GL matrix stack calls
can be dropped entirely.

### 2.3  Lighting and Materials

**Affected files**: `SoGLLazyElement.cpp`, `SoDirectionalLight.cpp`,
`SoPointLight.cpp`, `SoSpotLight.cpp`, `SoMaterialBundle.cpp`

Fixed-function calls: `glLightfv`, `glLightModeli`, `glMaterialfv`, `glMaterialf`,
`glColorMaterial`, `glShadeModel`, `glEnable(GL_LIGHTING)`.

**Replacement**: Upload light parameters (position, ambient/diffuse/specular,
attenuation, spot) and material parameters (ambient/diffuse/specular/emission,
shininess) as shader uniforms.  Implement Phong shading in the fragment shader.  Two
sets of uniforms required: a `Light[8]` array and a `Material` struct.

### 2.4  Display Lists

**Affected files**: `SoGLDisplayList.cpp`, `SoGLRenderAction.cpp`,
`SoRenderManager.cpp`

Fixed-function calls: `glNewList`, `glEndList`, `glCallList`, `glDeleteLists`,
`glGenLists`.

**Replacement**: Display lists were already the fallback path; the VBO/VAO cache
(`SoVBO`, `SoVertexArrayIndexer`, `SoPrimitiveVertexCache`) is the preferred path.
The display list path can be disabled (or turned into a no-op) for the modern backend.

### 2.5  Accumulation Buffer

**Affected files**: `SoGLRenderAction.cpp`, `SoRenderManager.cpp`

Fixed-function calls: `glAccum`, `GL_ACCUM`.

**Replacement**: Multi-sample anti-aliasing (MSAA) via `GL_SAMPLES` and
`glBlitFramebuffer`, or a manual accumulation pass using a full-screen quad and
additive blending into a floating-point FBO attachment.

### 2.6  Shaders (ARB vs Core)

**Affected files**: `SoGLSLShaderObject.cpp`, `SoGLSLShaderProgram.cpp`

Currently uses `GL_ARB_shader_objects` extension names:
`glCompileShaderARB`, `glLinkProgramARB`, `glUseProgramObjectARB`,
`glVertexAttribPointerARB`, `glEnableVertexAttribArrayARB`.

**Replacement**: Use core GL 3.x function names: `glCompileShader`, `glLinkProgram`,
`glUseProgram`, `glVertexAttribPointer`, `glEnableVertexAttribArray`.

### 2.7  Texture State

Most texture calls (`glTexImage2D`, `glBindTexture`, `glTexParameteri`) are already
core OpenGL and work in GL 3.x.  The only changes needed are:
- Remove `GL_TEXTURE_ENV` / `GL_MODULATE` environment calls (no-op in core).
- Replace fixed-function texture combines (`SoTextureCombine`) with GLSL.
- Replace `glEnable(GL_TEXTURE_2D)` with sampler binding + `texture()` GLSL call.

---

## 3.  PortableGL Shader Dialect

PortableGL does **not** compile text GLSL.  Instead, shaders are C functions that
match a fixed signature:

```c
// Vertex shader: transforms one vertex; receives uniform pointer and vertex
// attribute values; sets gl_Position and varying outputs.
void vert_shader(float* vs_output, void* vertex_attribs, Shader_Builtins* builtins, void* uniforms);

// Fragment shader: receives interpolated varyings; writes gl_FragColor.
void frag_shader(float* fs_input, Shader_Builtins* builtins, void* uniforms);
```

This means Obol's existing GLSL text shaders (`SoShaderProgram`, `SoVertexShader`,
`SoFragmentShader`) **cannot be used directly** with PortableGL.

There are two options:

| Option | Pros | Cons |
|--------|------|------|
| **A. Built-in C shaders** — ship a compiled Phong shader as C code and register it with PortableGL for internal rendering | Fast; no GLSL parsing | User `SoShaderProgram` nodes cannot work |
| **B. GLSL → C transpiler** — at scene-graph construction time, transpile GLSL text to a C function compatible with PortableGL | Preserves user shader nodes | Complex; requires a GLSL parser |

For an initial port, **Option A** is recommended.  `SoShaderProgram` would be silently
ignored in PortableGL builds, falling back to the built-in Phong + texture shader.
This matches the behaviour of the existing Vulkan and raytracing backends.

---

## 4.  Migration Strategy

### Phase 1 — Modernise Rendering (OpenGL 3 Core)

Replace all fixed-function GL calls in Obol with modern equivalents, targeting GL 3.3
core profile.  The existing OSMesa backend continues to work during this phase since
Mesa's OSMesa also supports GL 3.x when built with `--enable-gl3`.

Priority order:

1. **Matrix uniforms** (`SoGLModelMatrixElement`, `SoGLProjectionMatrixElement`,
   `SoGLMultiTextureMatrixElement`) — upload CPU-computed matrices to shader.
   **Status: ✅ Phase 1 PR — `SoGLModelMatrixElement` and `SoGLProjectionMatrixElement`
   now mirror all matrix changes into `SoGLModernState`.**
2. **Built-in Obol shaders** — write `obol_phong.vert` / `obol_phong.frag` that
   replicate the fixed-function Phong model using uniforms.
   **Status: ✅ Phase 1 PR — `src/rendering/obol_modern_shaders.h` provides GLSL 3.30
   Phong and base-colour shader strings; `SoGLModernState` compiles and caches them.**
3. **Geometry streaming** — convert all `glBegin`/`glEnd` paths in `SoGL.cpp` and
   shape nodes to VBO/VAO.
   **Status: ✅ Phase 1 PR — `sogl_render_sphere`, `sogl_render_cone`,
   `sogl_render_cylinder`, and `sogl_render_cube` now use the modern VAO+VBO path
   (via `sogl_render_*_modern`) when `SoGLModernState` is available.  The
   legacy immediate-mode path is retained as a fallback.**
4. **Lighting uniforms** — `SoDirectionalLight`, `SoPointLight`, `SoSpotLight` upload
   to `Light[8]` uniform array.
   **Status: ✅ Phase 1 PR — All three light nodes populate `SoGLModernState::Light`
   structs via `addLight()`.  The data is uploaded by `activatePhong()`.**
5. **Material uniforms** — `SoGLLazyElement` uploads to `Material` uniform struct.
   **Status: ✅ Phase 1 PR — `SoGLLazyElement::send()` mirrors ambient/diffuse/specular/
   emissive/shininess/twoSided into `SoGLModernState::setMaterial()` at the end of each
   lazy-element flush.**
6. **Display list removal** — remove `SoGLDisplayList` or replace with VBO cache hits.
   **Status: 🔲 Not yet started.**
7. **Accumulation buffer** — replace with FBO-based MSAA.
   **Status: 🔲 Not yet started.**
8. **Shader API** — migrate `SoGLSLShaderObject/Program` to core GL 3 entry points.
   **Status: 🔲 Not yet started.**
9. **Remaining geometry** — convert shape nodes (`SoFaceSet`, `SoTriangleStripSet`,
   `SoQuadMesh`, `SoLineSet`, `SoIndexedPointSet`) glBegin/glEnd fallbacks and the
   `SoPrimitiveVertexCache` glVertexPointer/glNormalPointer path to VAO+VBO.
   **Status: ✅ Complete** — all 10 shape nodes modernised; `SoPrimitiveVertexCache`
   uses `glVertexAttribPointer` in `renderTriangles`/`renderLines`/`renderPoints`.
10. **SoGLMultiTextureMatrixElement** — upload texture-matrix uniform.
    **Status: 🔲 Not yet started.**

Estimated scope: ~40 source files, ~5 000–8 000 lines changed.

### New file introduced in Phase 1 (this PR)

| File | Purpose |
|------|---------|
| `src/rendering/SoGLModernState.h` | Per-context modern GL state manager (header) |
| `src/rendering/SoGLModernState.cpp` | Implementation: shader compilation, matrix/material/light tracking, VAO activation |

### Phase 2 — PortableGL Context Manager

Once Obol only uses GL 3 core API, add a PortableGL context manager that:

1. Creates a `glContext` with a software framebuffer.
2. Implements the `SoDB::ContextManager` interface (same as `CoinOSMesaContextManagerImpl`).
3. Provides `getProcAddress()` returning PortableGL function pointers.
4. Registers built-in Phong + texture shader with PortableGL.

See `src/misc/SoDBPortableGL.cpp` and `tests/utils/portablegl_context_manager.h`.

### Phase 3 — Dual Build (System GL + PortableGL)

Extend the existing dual-GL architecture so that PortableGL contexts can coexist with
system-GL contexts in the same process.

Unlike OSMesa/system-GL coexistence, PortableGL uses completely different, non-GL
function names (`init_glContext`, `set_glContext`, …), so **no name-mangling is
required**.  Each PortableGL context carries its own framebuffer and state; making it
"current" via `set_glContext()` is thread-local and does not affect system GL contexts.

---

## 5.  Key Obstacles and Risks

### 5.1  PortableGL Shader Dialect Is Not GLSL

The most significant obstacle: user-facing `SoShaderProgram` / `SoVertexShader`
/ `SoFragmentShader` nodes take GLSL source text, but PortableGL expects C function
pointers.  In PortableGL builds, custom shaders will be silently ignored unless a
GLSL → C transpiler is integrated.

### 5.2  No Display List Support

Some Obol scene graphs may rely on display list caching for interactive performance.
Removing display lists may require ensuring VBO/VAO caching is robust for the same
patterns.

### 5.3  Accumulation Buffer Transparency

`SoGLRenderAction::SORTED_LAYERS_BLEND` uses `glAccum` for multi-sample transparency.
The FBO-based replacement has different performance and quality characteristics.

### 5.4  glShadeModel(GL_FLAT)

Flat shading via `glShadeModel(GL_FLAT)` must be replaced with a `flat` GLSL
interpolation qualifier.  This requires different vertex attributes per primitive
(requires GLSL `flat` output or `provoking vertex` logic).

### 5.5  OSMesa Build Complexity vs PortableGL Simplicity

The current OSMesa integration requires:
- A git submodule with USE_MGL_NAMESPACE name-mangling (≈ 900-line Mesa subset).
- A separate compilation unit (`gl_osmesa.cpp`) with `mgl*` prefixed calls.
- Per-context runtime dispatch (`coingl_is_osmesa_context`).

PortableGL replaces this with a single header include; no name-mangling or runtime
dispatch is needed.  **The migration substantially simplifies the build system.**

### 5.6  Geometry Shader Support

`SoGeometryShader` and bump mapping rely on geometry shaders or multi-pass techniques.
PortableGL does not implement geometry shaders; these features would be unavailable in
PortableGL builds.

---

## 6.  Affected Files by Phase

### Phase 1a — Matrix Uniforms (remove glMatrixMode, glPushMatrix, …)

| File | Change | Status |
|------|--------|--------|
| `src/elements/GL/SoGLModelMatrixElement.cpp` | Upload `uModelView`, `uNormalMatrix` uniforms instead of `glMultMatrixf` | ✅ mirrors to `SoGLModernState` (legacy fixed-function calls still present) |
| `src/elements/GL/SoGLProjectionMatrixElement.cpp` | Upload `uProjection` uniform instead of `glLoadMatrixf` | ✅ mirrors to `SoGLModernState` |
| `src/elements/GL/SoGLMultiTextureMatrixElement.cpp` | Upload texture-matrix uniform instead of `glMatrixMode(GL_TEXTURE)` | 🔲 |
| `src/glue/gl.cpp` | Remove / stub `SoGLContext_glMatrixMode`, `SoGLContext_glPushMatrix`, … | 🔲 (defer until all callers converted) |

### Phase 1b — Built-in Shaders

| File | Purpose | Status |
|------|---------|--------|
| `src/rendering/obol_modern_shaders.h` | GLSL 3.30 vertex + fragment shader source strings | ✅ exists |
| `src/rendering/SoGLModernState.h` | Per-context shader + state manager (header) | ✅ new |
| `src/rendering/SoGLModernState.cpp` | Shader compilation, uniform caching, state upload | ✅ new |

### Phase 1c — Geometry Streaming

| File | Change | Status |
|------|--------|--------|
| `src/rendering/SoGL.cpp` | Convert cone/cyl/sphere/cube to VAO+VBO rendering | ✅ modern path dispatched first; legacy path kept as fallback |
| `src/shapenodes/SoFaceSet.cpp` | Modern VAO+VBO path for OVERALL material (fan tessellation) | ✅ |
| `src/shapenodes/SoTriangleStripSet.cpp` | Modern VAO+VBO path for OVERALL material, GL_TRIANGLE_STRIP | ✅ |
| `src/shapenodes/SoLineSet.cpp` | Modern VAO+VBO path for OVERALL material, GL_LINE_STRIP | ✅ |
| `src/shapenodes/SoPointSet.cpp` | Modern VAO+VBO path (OVERALL + PER_VERTEX colours) | ✅ |
| `src/shapenodes/SoIndexedFaceSet.cpp` | Modern VAO+VBO path for OVERALL material (indexed fan tessellation) | ✅ |
| `src/shapenodes/SoIndexedLineSet.cpp` | Modern VAO+VBO path for OVERALL material, GL_LINE_STRIP | ✅ |
| `src/shapenodes/SoIndexedTriangleStripSet.cpp` | Modern VAO+VBO path for OVERALL material, GL_TRIANGLE_STRIP | ✅ |
| `src/shapenodes/SoQuadMesh.cpp` | Modern VAO+VBO path — rows rendered as GL_TRIANGLE_STRIP (replaces GL_QUAD_STRIP) | ✅ |
| `src/shapenodes/SoIndexedPointSet.cpp` | Modern VAO+VBO path for GL_POINTS (Phong shader, OVERALL material) | ✅ |
| `src/caches/SoPrimitiveVertexCache.cpp` | Modern VAO+VBO path via `glVertexAttribPointer` in `renderTriangles`/`renderLines`/`renderPoints` | ✅ |
| `src/rendering/CoinOffscreenGLCanvas.cpp` | Fix destructContext() to unbind FBO before deregistering context | ✅ |
| `src/rendering/obol_modern_shaders.h` | Fix no-lights Phong path to include emission term | ✅ |

### Phase 1d — Lighting / Material Uniforms

| File | Change | Status |
|------|--------|--------|
| `src/nodes/SoDirectionalLight.cpp` | Upload to `uLights[i]` uniform | ✅ populates `SoGLModernState::addLight()` |
| `src/nodes/SoPointLight.cpp` | Same | ✅ |
| `src/nodes/SoSpotLight.cpp` | Same | ✅ |
| `src/elements/GL/SoGLLazyElement.cpp` | Upload `uMat*` uniforms; remove `glMaterialfv`, `glShadeModel`, `glColorMaterial` | ✅ mirrors material via `setMaterial()` at end of `send()` |

### Phase 1e — Display List Removal

| File | Change | Status |
|------|--------|--------|
| `src/elements/GL/SoGLDisplayList.cpp` | Disable / no-op in modern GL build | 🔲 |
| `src/actions/SoGLRenderAction.cpp` | Remove accumulation-buffer pass | 🔲 |

### Phase 2 — PortableGL Context

| File | Purpose | Status |
|------|---------|--------|
| `src/misc/SoDBPortableGL.cpp` | `SoDB::ContextManager` implementation using PortableGL | ✅ exists (getPixels removed) |
| `tests/utils/portablegl_context_manager.h` | Test/example PortableGL context manager | ✅ exists |
| `cmake/FindPortableGL.cmake` | CMake find module for the single header | ✅ exists |
| `.gitmodules` | `external/portablegl` submodule entry | ✅ exists |

### Phase 3 — Build System

| File | Change | Status |
|------|--------|--------|
| `CMakeLists.txt` | Add `OBOL_USE_PORTABLEGL` option; remove OSMesa dependency paths | ✅ exists |
| `src/CMakeLists.txt` | Conditionally compile `SoDBPortableGL.cpp` | ✅ exists |

---

## 7.  Relationship to Existing Backends

After the migration, the backend matrix becomes:

| Backend | Build Flag | GL API | Context Creator |
|---------|-----------|--------|----------------|
| System OpenGL (GPU) | `OBOL_USE_SYSTEM_GL=ON` | Any GL version | `FLTKContextManager`, Qt context, etc. |
| PortableGL (software) | `OBOL_USE_PORTABLEGL=ON` | GL 3.3 core | `CoinPortableGLContextManager` |
| No OpenGL | both OFF | n/a | Custom (`SoCallbackAction`, Vulkan, …) |

OSMesa is **replaced** by PortableGL for the software-rasterizer role.  The dual-GL
mode (system GL + PortableGL) remains possible using PortableGL's thread-local context
approach with no name-mangling.

---

## 8.  References

- PortableGL repository: <https://github.com/rswinkle/PortableGL>
- OSMesa documentation: <https://docs.mesa3d.org/osmesa.html>
- OpenGL 3.3 core profile specification: <https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf>
- `docs/DUAL_GL_ARCHITECTURE.md` — Current OSMesa dual-GL architecture
- `docs/BACKEND_SURVEY.md` — Obol backend feature matrix
- `src/misc/SoDBOSMesa.cpp` — Current OSMesa context manager (reference implementation)
- `tests/utils/osmesa_context_manager.h` — OSMesa test context manager
