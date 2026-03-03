# Test Consolidation Plan

## Overview

This document tracks the ongoing reorganization of the Obol test suite into the
proposed two-tier framework. Sessions should update the status tables and
check off completed items as work progresses.

## Target Architecture

```
obol_unittest     – non-graphical unit tests; subcommand pattern
                    Usage: obol_unittest [list|run <name|cat>|help]

obol_render       – rendering/visual tests; produces control images
                    Usage: obol_render [list|run <name> <outpath>|run_all <dir>|help]
                    Single image OR a series of images per test.

obol_viewer       – interactive FLTK viewer (unchanged)
                    Shows all render tests with full interactivity.
```

**Key constraint**: every `obol_render` test must also work in `obol_viewer` with
meaningful interactivity.  Tests that currently show only a static scene must be
updated to include interactive elements (pick targets, draggers, animated content,
etc.) so the viewer user can see the feature in action.

---

## Implementation Status

### Phase 1 — Infrastructure (New Executables)

- [x] Add `render_sequence` field to `TestEntry` (test_registry.h)
- [x] Create `tests/obol_unittest.cpp`
- [x] Create `tests/obol_render.cpp`
- [x] Update `tests/CMakeLists.txt` to build `obol_unittest` and `obol_render`
- [x] Update `tests/rendering/CMakeLists.txt` — point `add_rendering_image_test`
      at `obol_render` instead of `obol_test`

### Phase 2 — Shadow Test Consolidation

The `render_shadow.cpp` test uses `ObolTest::Scenes::createShadow` (testlib
factory) and is already `add_testlib_rendering_test`.

The `render_shadow_advanced.cpp` test builds its **own** 4-scenario scene
inline, duplicating scene-graph logic already present in
`ObolTest::Scenes::createShadowAdvanced`.  The `obol_viewer` renders
`createShadowAdvanced` correctly.

- [x] Refactor `render_shadow_advanced.cpp` to use `createShadowAdvanced`
      factory for its primary image and retain inline sub-tests as validation
      passes only (no duplicated scene graph).
- [x] Change `add_rendering_test(render_shadow_advanced)` →
      `add_testlib_rendering_test(render_shadow_advanced)`.

### Phase 3 — Standalone → Testlib Render Test Conversions

The 62 standalone `add_rendering_test` tests below each build their own scene
graph, despite having matching `create_scene` factories registered in the
`TestRegistry`.  Converting them to use the shared factory ensures the viewer
and the CLI image-generation path render **identical scenes**.

Priority order: tests whose standalone scenes already closely match the factory
output go first (marked **easy**).  Tests that run multiple sub-scenarios and
cannot be represented as a single scene go last (marked **hard**).

| Render test                        | Factory in testlib?       | Difficulty | Status       |
|------------------------------------|---------------------------|------------|--------------|
| render_shadow_advanced             | createShadowAdvanced      | medium     | ✅ Done       |
| render_scene_texture               | createSceneTexture        | easy       | ☐ Pending   |
| render_scene_texture_multi_mgr     | (no matching factory yet) | hard       | ☐ Pending   |
| render_shape_hints                 | createShapeHints          | easy       | ☐ Pending   |
| render_environment                 | createEnvironment         | easy       | ☐ Pending   |
| render_texture3                    | createTexture3            | easy       | ☐ Pending   |
| render_bump_map                    | createBumpMap             | easy       | ☐ Pending   |
| render_texture_transform           | createTextureTransform    | easy       | ☐ Pending   |
| render_depth_buffer                | createDepthBuffer         | easy       | ☐ Pending   |
| render_alpha_test                  | createAlphaTest           | easy       | ☐ Pending   |
| render_cubemap                     | createCubemap             | easy       | ☐ Pending   |
| render_gl_big_image                | createGLBigImage          | easy       | ☐ Pending   |
| render_gl_features                 | createGLFeatures          | easy       | ☐ Pending   |
| render_multi_texture               | createMultiTexture        | easy       | ☐ Pending   |
| render_procedural_shape            | createProceduralShape     | easy       | ☐ Pending   |
| render_shader_program              | createShaderProgram       | easy       | ☐ Pending   |
| render_sorender_manager            | createSoRenderManager     | easy       | ☐ Pending   |
| render_image_deep                  | createImageDeep           | easy       | ☐ Pending   |
| render_quad_mesh_deep              | createQuadMeshDeep        | easy       | ☐ Pending   |
| render_sogl_bindings               | createSOGLBindings        | easy       | ☐ Pending   |
| render_material_binding            | createMaterialBinding     | easy       | ☐ Pending   |
| render_switch_visibility           | createSwitchVisibility    | easy       | ☐ Pending   |
| render_offscreen                   | createOffscreen           | easy       | ☐ Pending   |
| render_offscreen_advanced          | createOffscreenAdvanced   | medium     | ☐ Pending   |
| render_render_manager_full         | createRenderManagerFull   | medium     | ☐ Pending   |
| render_glrender_deep               | createGLRenderDeep        | medium     | ☐ Pending   |
| render_glrender_action_modes       | createGLRenderActionModes | medium     | ☐ Pending   |
| render_view_volume_ops             | createViewVolumeOps       | medium     | ☐ Pending   |
| render_field_connections           | createFieldConnections    | medium     | ☐ Pending   |
| render_engine_converter            | createEngineConverter     | medium     | ☐ Pending   |
| render_engine_interaction          | createEngineInteraction   | medium     | ☐ Pending   |
| render_sensors_rendering           | createSensorsRendering    | medium     | ☐ Pending   |
| render_sensor_interaction          | createSensorInteraction   | medium     | ☐ Pending   |
| render_path_operations             | createPathOperations      | medium     | ☐ Pending   |
| render_write_read_action           | createWriteReadAction     | medium     | ☐ Pending   |
| render_search_action               | createSearchAction        | medium     | ☐ Pending   |
| render_text3_parts                 | createText3Parts          | medium     | ☐ Pending   |
| render_arb8_draggers               | createArb8Draggers        | medium     | ☐ Pending   |
| render_arb8_edit_cycle             | createArb8EditCycle       | medium     | ☐ Pending   |
| render_rt_proxy_shapes             | createRTProxyShapes       | medium     | ☐ Pending   |
| render_raypick_shapes              | createRaypickShapes       | medium     | ☐ Pending   |
| render_ext_selection               | createExtSelection        | medium     | ☐ Pending   |
| render_ext_selection_events        | createExtSelectionEvents  | medium     | ☐ Pending   |
| render_nodekit_interaction         | createNodeKitInteraction  | medium     | ☐ Pending   |
| render_scene_interaction           | createSceneInteraction    | medium     | ☐ Pending   |
| render_camera_interaction          | createCameraInteraction   | medium     | ☐ Pending   |
| render_hud_interaction             | createHUDInteraction      | easy       | ☐ Pending   |
| render_event_callback_interaction  | createEventCallbackInteraction | medium | ☐ Pending |
| render_selection_interaction       | createSelectionInteraction | medium    | ☐ Pending   |
| render_pick_interaction            | createPickInteraction     | medium     | ☐ Pending   |
| render_pick_filter                 | createPickFilter          | medium     | ☐ Pending   |
| render_lod_picking                 | createLODPicking          | medium     | ☐ Pending   |
| render_callback_action             | createCallbackAction      | hard       | ☐ Pending   |
| render_callback_action_deep        | createCallbackActionDeep  | hard       | ☐ Pending   |
| render_callback_node               | createCallbackNode        | hard       | ☐ Pending   |
| render_event_propagation           | createEventPropagation    | hard       | ☐ Pending   |
| render_bbox_action                 | createBBoxAction          | hard       | ☐ Pending   |
| render_background_gradient         | createBackgroundGradient  | easy       | ☐ Pending   |
| render_viewport                    | (no matching factory yet) | hard       | ☐ Pending   |
| render_quad_viewport               | (no matching factory yet) | hard       | ☐ Pending   |

### Phase 4 — Interactive Elements for Visual Tests

Every `obol_render` test must be interactable in `obol_viewer`.  Most tests
already have `has_interactive = true` and include pick targets or camera
controls.  The list below tracks tests that may need explicit interaction
support added (e.g., picking callback, visible state change, animated element).

**Currently `has_interactive = false`:**
- `hud` (the HUDOverlay factory) – needs button callbacks or animated HUD text

**Tests where current interactive support may be insufficient** (static scene,
nothing to "do" in the viewer beyond camera orbit):
- `gradient` – pure background; needs a foreground interactive element
- `colored_cube` – smoke test; fine for basic orbit but needs pick target
- `sphere_position` – diagnostic check; fine as-is
- `checker_texture` – needs at least a pick-to-toggle behavior
- `switch_visibility` – should show a way to toggle the switch interactively
- `shape_hints` – could benefit from a pick-to-toggle hints button
- `environment` – should show animated fog density or light change
- `depth_buffer` – needs an interactive depth comparison element
- `alpha_test` – needs pick-to-toggle alpha threshold visualization
- `view_volume_ops` – camera manipulation already interactive; fine as-is

**Tests with strong interactive requirements (render_sequence needed):**

These tests verify *behavioral* changes and need multiple frames to show
interactions.  `obol_render` should produce a frame sequence; `obol_viewer`
should support the corresponding live interactions.

| Test                         | Interaction sequence needed                                      |
|------------------------------|------------------------------------------------------------------|
| render_pick_interaction      | frame 0: no selection; frame 1: sphere picked (highlighted)     |
| render_pick_filter           | frame 0: no selection; frame 1: sphere picked; frame 2: blocked |
| render_selection_interaction | frame 0: no sel; frame 1: sphere sel; frame 2: multi-sel        |
| render_lod_picking           | frame 0: close; frame 1: medium dist; frame 2: far dist         |
| render_draggers              | frame 0: default pos; frame 1: translated; frame 2: rotated     |
| render_simple_draggers       | frame 0: default; frame 1: drag applied                         |
| render_light_manips          | frame 0: default; frame 1: light moved                          |
| render_manip_sequences       | frame 0: base; frame 1: manip attached; frame 2: moved          |
| render_arb8_draggers         | frame 0: default; frame 1: corner dragged                       |
| render_arb8_edit_cycle       | frame 0: step 0; frame 1-3: edit steps                          |
| render_camera_interaction    | frame 0: perspective; frame 1: ortho; frame 2: zoomed           |
| render_scene_interaction     | frame 0: base; frame 1: node added; frame 2: node removed       |
| render_ext_selection         | frame 0: no sel; frame 1: lasso; frame 2: bbox                  |
| render_ext_selection_events  | frame 0: no sel; frame 1: rect sel; frame 2: part bbox          |

### Phase 5 — Viewport / Quad-Viewport Tests (New Factories Needed)

These tests lack matching testlib factories entirely:

- `render_viewport` — needs `createViewport` factory
- `render_quad_viewport` — needs `createQuadViewport` factory  
- `render_scene_texture_multi_mgr` — needs `createSceneTextureMultiMgr` factory

---

## Current Known Issues

### Shadow Tests

The `render_shadow_advanced.cpp` standalone test creates `SoShadowGroup` with
`getCoinHeadlessContextManager()` in each sub-test, whereas
`ObolTest::Scenes::createShadowAdvanced` creates it without any argument.
The viewer successfully renders `createShadowAdvanced`, confirming the factory
is correct.  The standalone test's extra `getCoinHeadlessContextManager()` call
was added to work around an earlier issue and is no longer needed (the context
manager is set globally by `initCoinHeadless()` before any rendering).

**Action:** Done – see Phase 2 ✅.

### Missing `render_sequence` Support

Currently `TestEntry::create_scene` returns a single static `SoSeparator*`.
There is no mechanism to drive a sequence of rendered frames from a single test
registration.  Adding `render_sequence` (Phase 1, implemented) provides this
capability; individual tests still need their sequence callbacks populated
(Phase 4).

---

## Test Category Inventory

### Non-Graphical Unit Tests (→ obol_unittest)

Total: ~213 tests across 21 categories.

| Category   | Count | Example tests                                                  |
|------------|-------|----------------------------------------------------------------|
| Actions    | ~15   | bbox_action, ray_pick, search_action, callback_action         |
| Base       | ~30   | vec2f, vec3f, matrix, rotation, box3f, viewvolume, dict       |
| Bundles    | ~3    | bundle_basics                                                  |
| Caches     | ~3    | cache_basics                                                   |
| Details    | ~6    | details_suite, details_deep                                    |
| Draggers   | ~8    | dragger lifecycle, state                                       |
| Elements   | ~3    | element_stack                                                  |
| Engines    | ~15   | engine_basics, deep traversal                                  |
| Errors     | ~3    | error_handling                                                 |
| Events     | ~6    | event_dispatch, event_filter                                   |
| Fields     | ~20   | sf_fields, mf_fields, connections, containers                  |
| IO         | ~12   | read/write, binary IO, SoDB                                    |
| Lists      | ~3    | list_basics                                                    |
| Manips     | ~6    | manip_lifecycle                                                |
| Misc       | ~6    | misc_basics                                                    |
| Nodes      | ~30   | shape nodes, nodekits, selection, animation                    |
| Profiler   | ~3    | profiler_basics                                                |
| Projectors | ~6    | line_projector, plane_projector                               |
| Sensors    | ~8    | sensor_dispatch                                                |
| Threads    | ~3    | thread_safety                                                  |
| Tools      | ~3    | tools_basics                                                   |

### Rendering / Visual Tests (→ obol_render + obol_viewer)

Total: ~100 tests across all rendering scenarios.

**Sub-groups:**

- **Basic geometry** (primitives, face/line/point sets, quad mesh, strip set): 15
- **Materials & lighting** (materials, lighting, vertex colors, material binding): 6
- **Textures** (texture, texture3, bump map, multi texture, cubemap, scene tex): 8
- **Text rendering** (text2, text3, ascii_text, text_demo, text3_parts): 5
- **Transforms & cameras** (transforms, cameras, reset_transform, view_volume): 5
- **Effects** (shadow, shadow_advanced, transparency, alpha_test, depth_buffer): 6
- **Draggers & manipulators** (draggers, simple_draggers, light_manips,
  manip_sequences, arb8_draggers, arb8_edit_cycle): 6
- **Selection & picking** (pick_interaction, pick_filter, selection_interaction,
  ext_selection, ext_selection_events, lod_picking, raypick_shapes): 7
- **Actions & sensors** (bbox_action, search_action, callback_action, path_ops,
  write_read, field_connections, sensors_rendering, sensor_interaction): 10
- **HUD & UI** (hud, hud_overlay, hud_no3d, hud_interaction, hud_demo): 5
- **NanoRT / raytracing** (nanort, nanort_shadow, rt_proxy_shapes): 3
- **Procedural & special** (procedural_shape, shader_program, gl_big_image,
  gl_features, offscreen, render_manager, sorender_manager): 7
- **Viewport** (viewport, quad_viewport, quad_viewport_lod, viewport_scene): 4
- **Interaction demos** (camera_interaction, scene_interaction,
  event_callback_interaction, nodekit_interaction, engine_interaction,
  engine_converter): 6
- **Misc visual** (coordinates, sphere_position, gradient, colored_cube,
  checker_texture, annotation, background_gradient, switch_visibility,
  shape_hints, environment, scene, clip_plane, array_multiple_copy,
  image_node, image_deep, marker_set, lod): 17

---

## Session Log

### Session 1 (2026-03-03)
- Performed full analysis of existing test infrastructure (317 REGISTER_TEST
  calls, 100 visual / 213 unit, 41 testlib-backed render tests, 62 standalone)
- Identified shadow consolidation issue: `render_shadow_advanced.cpp` does not
  use `createShadowAdvanced` testlib factory
- Identified `render_sequence` gap: no mechanism for multi-frame interaction
  testing  
- **Completed:**
  - Created this document
  - Added `render_sequence` field to `TestEntry` in `test_registry.h`
  - Created `tests/obol_unittest.cpp` (non-graphical unit test runner)
  - Created `tests/obol_render.cpp` (rendering test runner)
  - Updated `tests/CMakeLists.txt` to build both new executables
  - Fixed `render_shadow_advanced.cpp` to use testlib factory +
    changed to `add_testlib_rendering_test`
  - Updated `tests/rendering/CMakeLists.txt` `add_rendering_image_test` macro
    to use `obol_render` instead of `obol_test`

### Session 2 (planned)
- Begin Phase 3: convert easy standalone render tests to use testlib factories
- Implement `render_sequence` callbacks for high-priority interaction tests
  (pick_interaction, selection_interaction, draggers)
- Add explicit interactive elements to tests that currently have only static
  scenes (gradient, colored_cube, switch_visibility, etc.)

### Session 3+ (planned)
- Continue Phase 3 conversions (medium/hard difficulty tests)
- Phase 4 completion: all render tests have meaningful interactive elements
- Phase 5: add missing viewport factories
- Final review and cleanup
