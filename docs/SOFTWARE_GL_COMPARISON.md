# OSMesa vs PortableGL as Obol Software Fallback Layers

This document compares the two software-rasterisation backends currently available
in Obol — OSMesa and PortableGL — across three axes:

1. **Performance** — raw throughput, scalability, rendering quality
2. **Maintainability** — code owned by Obol, integration complexity, dependency footprint
3. **Feature coverage** — what each backend supports natively, what Obol's compat
   layer had to supply, and what is still missing

---

## Background and Design Intent

Both backends pursue the same goal: let Obol render scenes to a pixel buffer without
requiring a display server, a GPU, or system OpenGL drivers.  They approach that goal
from opposite ends of the design spectrum.

| Aspect | OSMesa | PortableGL |
|---|---|---|
| Origin | Mesa 7.0.4 (2007), maintained fork in `external/osmesa/` | `portablegl.h` v0.100.0 (rswinkle), vendored in `external/portablegl/` |
| Design goal | Portable, reliable, maintenance-minimal Mesa subset | Minimal OpenGL 3.x core in a single header |
| Lines of code (external dep) | ~13 M bytes (379 source files) | ~381 KB (one header) |
| API surface | OpenGL 2.0 + fixed-function + selected extensions | OpenGL 3.x core, no fixed-function, no GLSL compiler |
| Language | C89/C++ | C99/C++11 |
| Build complexity | Separate CMake sub-project (external/osmesa) | `#define PORTABLEGL_IMPLEMENTATION` in one TU |

---

## a) Performance

### OSMesa

The starseeker fork of OSMesa is based on Mesa 7.0.4 (2007).  Its stated goal is
**reliability and portability, not speed**:

> "The goals of this particular fork are NOT speed, but rather a portable, reliable,
> self contained, cross platform rendering method."  — external/osmesa/README

Concretely:

- **Single-threaded** scalar C rasteriser.  No SIMD, no tiling, no parallel draw calls.
- **No JIT or LLVM** — this is the key difference from modern mesa/llvmpipe.  Software
  rasterisation is straightforward C loops over pixels.
- **Depth precision**: 24-bit depth buffer (configurable at context creation time via
  `OSMesaCreateContextExt`).
- **Antialiasing**: FXAA post-processing is available via `OSMesaFXAAEnable()`.
- **GLSL shaders** are compiled and run by Mesa's own GLSL compiler (not JIT-compiled —
  Mesa 7.0.4's GLSL runs as interpreted bytecode on the software driver).

For Obol's typical use case (geometry-heavy engineering scenes, Phong shading, shadow
maps at modest resolutions ≤2048×2048), OSMesa is **adequate but not fast**.  Expect
render times 10–50× slower than hardware GL for complex scenes.  Shadow-map passes (two
full render passes per shadow light) hurt particularly because each pass traverses the
whole scene graph.

### PortableGL

PortableGL is also single-threaded and unoptimised by default, but the code base is
much smaller and can be more easily modified:

- **Single-threaded** fixed-function C rasteriser.  Each `pglDrawArrays` call is a tight
  inner loop over triangles with software perspective interpolation.
- **No SIMD, no tiling**.  The upstream README explicitly notes this as a future
  contribution target (multi-threaded tile rasterisation).
- **No depth precision choice** — 32-bit float depth buffer baked in.
- **No antialiasing** — FXAA would have to be added to Obol's compat layer.
- **No shader JIT** — C-function shaders are compiled into the binary at build time.
  This is actually faster than Mesa 7.0.4's GLSL interpreter for simple shaders
  because there is zero interpreter overhead.  For complex scenes the CPU shaders in
  `portablegl_obol_shaders.h` are tight C loops.

**Summary**: For simple scenes (flat or Gouraud shading, no shadows) PortableGL's C
shaders outperform OSMesa's GLSL interpreter.  For complex scenes with many lights and
shadow passes the difference is small — both are CPU-bound.  For scenarios requiring
accurate depth buffers, high precision rendering, or multisample antialiasing, OSMesa
wins on correctness.

### Verdict — Performance

| Criterion | OSMesa | PortableGL |
|---|---|---|
| Per-pixel throughput | Moderate (C loops, no SIMD) | Comparable (C loops, no SIMD) |
| Shader cost | Higher (GLSL interpreter overhead in Mesa 7.0.4) | Lower (pre-compiled C functions) |
| Shadow-map pass | Works natively | Infrastructure complete; depth FBO redirects back buffer to depth texture |
| Depth precision | 24-bit integer (accurate) | 32-bit float (good for most uses) |
| Multisampling | Not supported (GL_EXT_framebuffer_multisample: MISSING) | Not supported |
| Antialiasing | FXAA built-in (`OSMesaFXAAEnable`) | Not built-in; would need compat-layer addition |
| Threading potential | Hard (Mesa state machine) | Easier (small codebase; tile-parallel feasible) |
| **Overall** | **Mature, adequate** | **Comparable, more improvable** |

---

## b) Maintainability

### OSMesa — Code Obol Owns

The integration code for OSMesa in Obol is small and clean:

| File | LOC | Role |
|---|---|---|
| `src/misc/SoDBOSMesa.cpp` | 148 | `SoDB::ContextManager` impl, wraps `OSMesaCreateContextExt` |
| `src/glue/gl_osmesa.cpp` | 100 | Re-compiles `gl.cpp` with OSMesa headers + `SOGL_PREFIX_STR=osmesa_` |
| `tests/utils/osmesa_context_manager.h` | 130 | Standalone header-only manager for tests/viewers |

Total Obol-owned OSMesa glue: **~380 lines**.  The integration is a thin shim: OSMesa
exposes a standard `gl*` API (via `mgl*` name mangling), so `gl.cpp` compiles against
it unmodified.  There is no compatibility layer, no interceptor, no shader translation.

The **external dependency** (13 MB of Mesa source) is large but it is a stable,
frozen snapshot.  The starseeker fork has already removed everything that is not needed
for the software rasteriser.  Upstream changes are irrelevant unless a critical bug
is found.  The CMake sub-project builds cleanly.

### PortableGL — Code Obol Owns

Because PortableGL lacks fixed-function state, GLSL, and several other features Obol
uses, Obol must supply a significant compatibility layer:

| File | LOC | Role |
|---|---|---|
| `src/misc/portablegl_compat_state.h` | 317 | `ObolPGLCompatState` struct + matrix/light/material helpers |
| `src/misc/portablegl_compat_funcs.cpp` | 390 | Interceptors for glMatrixMode, glLightfv, glMaterialfv, glBegin/End, etc. |
| `src/misc/portablegl_shader_registry.cpp` | 314 | GLSL interceptor, shader classification, `pglCreateProgram` wiring |
| `src/misc/portablegl_obol_shaders.h` | 534 | Six C-function shaders (Flat, Gouraud, Phong, Tex-Replace, Tex-Phong, Depth) |
| `src/misc/portablegl_impl.cpp` | 46 | Single `PORTABLEGL_IMPLEMENTATION` TU |
| `src/misc/SoDBPortableGL.cpp` | ~400 | `SoDB::ContextManager` impl, proc-address table |
| `src/glue/gl_portablegl.cpp` | 95 | Re-compiles `gl.cpp` with portablegl headers + `SOGL_PREFIX_STR=portablegl_` |
| `external/portablegl/portablegl_gl_mangle.h` | 281 | `gl* → pgl_gl*` rename macros for dual builds |
| `tests/utils/portablegl_context_manager.h` | 155 | Header-only manager for tests/viewers |

Total Obol-owned PortableGL glue: **~2,530 lines**.  That is 6–7× more code to maintain
than the OSMesa glue.  Each change to Obol's shader system (new uniform, new shader
variant, new lighting model) requires corresponding changes in the compat layer.

The **external dependency** (one 381 KB header) is trivially small.  No CMake
sub-project, no separate build step.  The PortableGL API is also small enough to audit
completely.

### Key Maintenance Risks

**OSMesa:**
- Mesa 7.0.4 is old (2007) C code.  New compiler warnings and static-analysis findings
  appear occasionally and need to be triaged.
- The GLSL compiler in Mesa 7.0.4 does not support GLSL 1.20+; if Obol ever upgrades
  its shaders to use GLSL 1.20 features, OSMesa will silently fail to compile them.
- Any OSMesa bug is a bug inside 13 MB of C code that Obol does not know deeply.

**PortableGL:**
- The shader classifier in `portablegl_shader_registry.cpp` uses heuristic keyword
  matching to pick a C shader from a fixed set.  Any new Obol shader variant not in
  the fixed set will silently render wrong.
- The FBO compat layer now supports colour, depth-texture, and depth-renderbuffer
  attachments.  Shadow passes use `GL_DEPTH_ATTACHMENT` + `OBOL_PGL_SHADER_DEPTH`
  which writes depth-as-greyscale-RGBA; the Phong shadow-lookup shader still needs
  wiring to read the depth texture and compare.
- The immediate-mode emulation (`glBegin`/`glEnd`) buffers vertices and calls
  `glDrawArrays` at `glEnd` — correct, but adds per-frame allocation overhead.
- Thread-local `g_cur_compat` works for single-threaded rendering but needs care for
  multi-threaded scene traversal (`OBOL_THREADSAFE=ON`).

### Verdict — Maintainability

| Criterion | OSMesa | PortableGL |
|---|---|---|
| Obol-owned glue code | ~380 lines | ~2,530 lines |
| External dep size | 13 MB (stable, frozen) | 381 KB (actively developed) |
| Shader changes require Obol change | No (GLSL compiled dynamically) | Yes (new C-function shader + classifier) |
| Bugs in external dep | Hard to fix (C89 Mesa internals) | Easy to fix (small, readable C99) |
| Upstream evolution | None expected (frozen fork) | Active (upstream adding features) |
| **Overall** | **Lower ongoing maintenance** | **Higher, but improvable with upstream help** |

---

## c) Feature Coverage

### OpenGL Version and Profile

| Feature | OSMesa | PortableGL |
|---|---|---|
| Reported GL version | `2.0 Mesa 7.0.4` | `OpenGL ES 3.0 (PortableGL)` |
| Fixed-function pipeline | ✅ Native | ❌ Via Obol compat layer |
| GLSL compiler | ✅ `GL_ARB_shading_language_100` | ❌ No (C-function shaders only) |
| ARB shader objects | ✅ `GL_ARB_shader_objects` | ✅ Intercepted by registry |
| Vertex/fragment shaders | ✅ `GL_ARB_vertex_shader`, `GL_ARB_fragment_shader` | ✅ C-function shaders |
| Geometry shaders | ❌ | ❌ |
| Compute shaders | ❌ | ❌ |
| GLSL 1.20+ | ❌ (Mesa 7.0.4 limitation) | ❌ (not applicable) |

### Geometry and Rasterisation

| Feature | OSMesa | PortableGL |
|---|---|---|
| Triangle rasterisation | ✅ | ✅ |
| Line rendering | ✅ | ✅ |
| Point rendering | ✅ | ✅ |
| Vertex buffer objects (VBO) | ✅ `GL_ARB_vertex_buffer_object` | ✅ |
| Vertex array objects (VAO) | ✅ `GL_ARB_vertex_array_object` | ✅ |
| Index buffers | ✅ | ✅ |
| Instanced rendering | ✅ | ✅ |

### Texture Support

| Feature | OSMesa | PortableGL |
|---|---|---|
| 2D textures | ✅ | ✅ |
| 3D textures | ✅ `GL_EXT_texture3D` | ✅ |
| Cube map textures | ✅ `GL_ARB_texture_cube_map` | ✅ |
| Multi-texturing | ✅ `GL_ARB_multitexture` | ✅ |
| Texture arrays | ✅ | ✅ |
| Depth textures | ✅ `GL_ARB_depth_texture` | ✅ Depth written as greyscale RGBA via `OBOL_PGL_SHADER_DEPTH` |
| Mipmaps | ✅ | ✅ |
| sRGB textures | ✅ `EXT_texture_sRGB` | ❌ |

### Framebuffer Objects (FBOs)

| Feature | OSMesa | PortableGL |
|---|---|---|
| `GL_EXT_framebuffer_object` | ✅ | ✅ Colour + depth attachments |
| Colour attachment | ✅ | ✅ via `pglSetTexBackBuffer` |
| Depth texture attachment | ✅ | ✅ depth-as-RGBA via `pglSetTexBackBuffer` + `OBOL_PGL_SHADER_DEPTH` |
| Depth renderbuffer attachment | ✅ | ✅ tracked for FBO-completeness; not sampled (sufficient for `SoSceneTexture2`) |
| Stencil attachment | ✅ | ✅ depth_rbo slot tracks stencil for FBO-completeness |
| Multiple render targets | ❌ `GL_ARB_framebuffer_object: MISSING` | ❌ |
| Multisampled FBO | ❌ `GL_EXT_framebuffer_multisample: MISSING` | ❌ |
| `GL_ARB_framebuffer_object` | ❌ | ❌ |

> **Note on FBO preference in Obol's dispatch layer**: `gl.cpp` already tries
> `GL_ARB_framebuffer_object` (OpenGL 3.0 core) first before falling back to
> `GL_EXT_framebuffer_object`.  The function pointers (`glGenFramebuffers`,
> `glBindFramebuffer`, etc.) stored in `SoGLContext` are loaded from whichever
> path succeeds, so callers in `CoinOffscreenGLCanvas` and `SoSceneTexture2`
> automatically use ARB when available without any code change.

> **Note on PortableGL depth FBOs**: When a texture is attached at
> `GL_DEPTH_ATTACHMENT`, PortableGL redirects its RGBA back buffer to that
> texture.  `OBOL_PGL_SHADER_DEPTH` writes `(depth, depth, depth, 1.0)` into
> each pixel.  Shadow shaders then read the texture's R channel as depth.
> PortableGL's own internal zbuf continues to perform depth testing correctly
> during rasterisation regardless of what the back buffer points to.

### Shadow Maps — Critical for SoShadowGroup

| Feature | OSMesa | PortableGL |
|---|---|---|
| `GL_ARB_shadow` | ✅ | ❌ Not implemented |
| `GL_ARB_shadow_ambient` | ✅ | ❌ |
| Depth texture (`GL_ARB_depth_texture`) | ✅ | ✅ via depth-as-RGBA FBO path |
| Depth-only FBO pass | ✅ (via EXT/ARB FBO) | ✅ `GL_DEPTH_ATTACHMENT` redirects back buffer to depth texture |
| Renderbuffer depth attachment | ✅ | ✅ tracked for FBO-completeness; PortableGL zbuf used for rasterisation |
| `SoShadowGroup` functional | ✅ (tested) | ⚠️ Infrastructure complete; Phong shadow-lookup needs `ObolPGLCompatState::shadows` wiring |

Shadow rendering is one of the most important Obol features that requires careful
attention.  OSMesa supports it fully.  The PortableGL compat layer now has depth
FBO infrastructure in place; the remaining work is wiring the shadow matrix and
shadow depth-texture sampling into the Phong C shader.

### Matrix and Fixed-Function State

| Feature | OSMesa | PortableGL |
|---|---|---|
| `glMatrixMode` / `glLoadMatrixf` | ✅ native | ✅ via compat layer |
| Matrix stacks | ✅ native (GL_MODELVIEW_MATRIX_STACK_DEPTH) | ✅ 32-deep stacks in `ObolPGLCompatState` |
| `glLightfv` / `glMaterialfv` | ✅ native | ✅ via compat layer |
| `glBegin` / `glEnd` | ✅ native | ✅ via compat layer (buffered → DrawArrays) |
| `glColorMaterial` | ✅ native | ⚠️ Intentional no-op (material set via `glMaterialfv`) |
| `glFog*` | ✅ native | ✅ `pgl_igl_Fog{f,i,fv,iv}` + `GL_FOG` Enable/Disable; applied in Gouraud, Phong, textured-Phong shaders |

### Pixel Read-Back

| Feature | OSMesa | PortableGL |
|---|---|---|
| `glReadPixels` | ✅ (formats: all standard) | ✅ via compat layer (RGBA/UNSIGNED_BYTE only) |
| Row flip (GL bottom-up vs app top-down) | ✅ native | ✅ handled in `pgl_igl_ReadPixels` |
| `GL_PACK_ROW_LENGTH` | ✅ | ✅ via `pgl_igl_PixelStorei` interceptor |

### Rendering Quality Extras

| Feature | OSMesa | PortableGL |
|---|---|---|
| FXAA post-processing | ✅ `OSMesaFXAAEnable()` | ❌ |
| Multisampled AA | ❌ | ❌ |
| Depth-of-field | ❌ | ❌ |
| HDR framebuffers | ❌ | ❌ |

### Side-by-Side with System GL (Dual Build)

| Feature | OSMesa | PortableGL |
|---|---|---|
| Symbol isolation mechanism | `USE_MGL_NAMESPACE` / `<OSMesa/gl_mangle.h>` → `mgl*` | `PGL_PREFIX_GL` / `portablegl_gl_mangle.h` → `pgl_gl*` |
| Dual-build CMake option | `OBOL_BUILD_DUAL_GL` | `OBOL_BUILD_DUAL_PORTABLEGL` |
| Compilation unit | `src/glue/gl_osmesa.cpp` | `src/glue/gl_portablegl.cpp` |
| Runtime context dispatch | `coingl_register_osmesa_context()` | `coingl_register_portablegl_context()` |
| Status | ✅ Production-ready | ✅ Infrastructure complete (rendering quality limited by compat layer) |

---

## Summary Matrix

| | OSMesa | PortableGL |
|---|---|---|
| **Performance** | Moderate (C rasteriser, GLSL interpreter) | Comparable (C rasteriser, pre-compiled shaders) |
| **Dependency size** | 13 MB (frozen) | 381 KB (active upstream) |
| **Obol glue size** | ~380 lines | ~2,530 lines |
| **GLSL shaders** | ✅ Native | ❌ C-function shaders only |
| **Shadow maps** | ✅ Full | ⚠️ Infrastructure complete; Phong shadow-lookup needs wiring |
| **FBO (EXT/ARB)** | ✅ Colour + depth | ✅ Colour, depth-texture, depth-renderbuffer |
| **Fog** | ✅ Native | ✅ Via compat layer + shaders |
| **GL_PACK_ROW_LENGTH** | ✅ Native | ✅ Via `pgl_igl_PixelStorei` interceptor |
| **Fixed-function** | ✅ Native | ✅ Via compat layer |
| **Side-by-side system GL** | ✅ (`OBOL_BUILD_DUAL_GL`) | ✅ (`OBOL_BUILD_DUAL_PORTABLEGL`) |
| **Multi-threading** | Hard | Possible (small codebase) |
| **Upstream evolution** | None (frozen fork) | Active (proposed contributions pending) |
| **Production ready for Obol** | ✅ Yes | ⚠️ Shadow-lookup wiring remaining |

---

## Recommendation

**OSMesa remains the recommended software fallback** for production Obol builds because:

1. It compiles Obol's GLSL shaders natively — no compat layer required.
2. Shadow maps (`SoShadowGroup`) work out of the box.
3. FBOs (EXT/ARB path) work for render-to-texture (`SoSceneTexture2`).
4. The external dependency is frozen and well-tested.
5. The Obol integration code is small (~380 lines) and rarely needs to change.

**PortableGL is the better choice when**:

1. The deployment target has no shared libraries at all (embedded / WASM) — PortableGL
   compiles to a self-contained binary with no external deps.
2. The deployment target is memory-constrained — the 381 KB header link is vastly
   smaller than Mesa.
3. Custom shader performance is important — once Obol's C shaders are compiled in, they
   run faster than Mesa 7.0.4's GLSL interpreter.
4. You want to co-exist with system GL in a single process with minimal dependencies
   (`OBOL_BUILD_DUAL_PORTABLEGL`) — the symbol-isolation mechanism is now equivalent to
   the OSMesa dual-GL build.

**Remaining work to make PortableGL production-ready for Obol:**

- [ ] Wire shadow matrix + depth-texture sampling into `obol_phong_fs` via
      `ObolPGLCompatState::shadows` so `SoShadowGroup` can use the depth-as-RGBA texture.
- [ ] Validate `SoShadowGroup` end-to-end with the depth FBO.
- [ ] Test multi-threaded render traversal (`OBOL_THREADSAFE=ON`) with the
      `g_cur_compat` thread-local.
- [ ] Send upstream PRs to [rswinkle/PortableGL](https://github.com/rswinkle/PortableGL)
      for `portablegl_gl_mangle.h`, `PGL_PREFIX_GL`, and the compat-state additions.
