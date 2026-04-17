# CAD Benchmark Tests

This directory contains tests and benchmarks for the `SoCADAssembly` subsystem.
Each test answers a different set of questions; consult the table below to pick
the right test for your task.

---

## Quick reference

| Executable                   | GL required | What it measures | When to run |
|------------------------------|-------------|------------------|-------------|
| `test_cad_ids`               | No          | ID generation, hash stability | every build |
| `test_segment_lod`           | No          | SegmentPopLod POP-quantisation | every build |
| `test_triangle_lod`          | No          | TrianglePopLod POP-quantisation | every build |
| `test_cad_picking`           | No          | CPU BVH ray-picking | every build |
| `test_cad_benchmark`         | Yes         | SG vs. CAD wireframe (1 part, 512 inst) | every build |
| `test_cad_mesh_lod_benchmark`| Yes         | SoCAD icosphere LoD scaling to 1M inst | every build |
| `test_cad_hier_benchmark`    | Yes         | Unique parts, hierarchy IDs, memory, API paths | every build |

---

## Detailed descriptions

### test_cad_benchmark

*Baseline correctness and performance comparison.*

- One shared wireframe box part; `INSTANCES_PER_AXIS^3` (512) instances on a 3-D grid.
- Renders with both a flat `SoSeparator` scene graph and `SoCADAssembly::WIREFRAME`.
- Verifies: pixel coverage within 20 pp, pixel MAD < 30/255.
- Also tests orthographic camera for both approaches.
- Fast (< 5 s in CI).

### test_cad_mesh_lod_benchmark

*Triangle-mesh LoD scaling limits.*

- One shared icosphere mesh (1 280 triangles / instance).
- Scales from 64 instances (correctness check) up to 1 000 000 instances (build-only).
- Compares `SHADED` (no LoD), `SHADED+LoD`, and `SG` approaches.
- Verifies: LoD actually reduces triangle count for distant instances.
- Medium scale (< 30 s in CI).

### test_cad_hier_benchmark  ← new

*Unique parts, hierarchical IDs, memory, and API-path coverage.*

**What the existing benchmarks do *not* cover:**

| Gap | How this test fills it |
|-----|------------------------|
| All instances share one geometry | N distinct icosphere meshes (complexity varies per part index) |
| Flat instances (parent = root) | 2-level hierarchy: root → groups → leaves via `CadIdBuilder::extendNameOccBool` |
| Only one draw mode tested | WIREFRAME, SHADED, and SHADED_WITH_EDGES timed separately |
| No API-path coverage at scale | `setHiddenInstances`, `updateInstanceTransform`, `updateInstanceStyle`, pick |
| No memory tracking | RSS sampled post-build and post-render (Linux `/proc/self/status`) |
| No machine-readable output | CSV block printed at end (between `# BEGIN_CSV` / `# END_CSV` markers) |

**Scenarios:**

| Name | Parts | Instances | Description |
|------|-------|-----------|-------------|
| `smoke` | 5 | 64 | Correctness + SG flat baseline comparison |
| `unique_parts_med` | 50 | 500 | Unique-part performance reference |
| `unique_parts_lg` | 200 | 2 000 | Larger unique-part stress |

**Pass/fail criteria (smoke scenario):**

- All three draw-mode CAD renders succeed (non-blank, ≥ 1% coverage).
- SG flat render succeeds.
- SG vs. CAD-shaded coverage diff < 20 percentage points.
- Pixel MAD between SG and CAD-shaded < 30/255.
- `SoRayPickAction` at viewport centre returns a valid `SoCADDetail`.

Larger scenarios are purely informational — their timings and memory figures
go into the CSV but do not gate the test result.

---

## CSV output format

`test_cad_hier_benchmark` prints a CSV block surrounded by markers:

```
# BEGIN_CSV
scenario,approach,parts,instances,build_ms,render_first_ms,render_steady_ms,
  rss_post_build_kB,rss_post_render_kB,peak_rss_kB,
  render_tier,coverage_pct,render_ok,
  hidden_all_ms,transform_upd_ms,style_upd_ms,pick_ms,pick_hit
smoke,cad_shaded,5,64,12.3,45.1,30.2,45000,46000,47000,1,18.5,1,...
...
# END_CSV
```

Extract the CSV block with:

```bash
./test_cad_hier_benchmark | sed -n '/^# BEGIN_CSV/,/^# END_CSV/p' \
  | grep -v '^#' > results.csv
```

`render_tier` values:
- `0` = Tier 0 (immediate-mode GL 1.1 fallback)
- `1` = Tier 1 (VBO-loop, GL 2.0 / GLSL 1.10)
- `2` = Tier 2 (instanced, GL 3.1+)
- `-1` = SG baseline (no tier)

Memory columns are populated on Linux only; `-1` on other platforms.

---

## Generator utility (`cad_test_generator.h`)

`cad_test_generator.h` is a standalone header-only library shared by
`test_cad_hier_benchmark` (and future tests).  It provides:

- `CadTestGen::generate(HierGenConfig)` — produces a deterministic `Assembly`
  (parts + 2-level hierarchy of instances, grid-positioned).
- `CadTestGen::populateCAD(asm, gen)` — fills a `SoCADAssembly` from an `Assembly`.
- `CadTestGen::buildFlatSG(gen)` — builds a flat `SoSeparator` tree with one
  node per instance (geometry shared per unique part).

Part complexity is varied by part index:

| `partIdx % 4` | Icosphere level | Triangles |
|---------------|-----------------|-----------|
| 0 | 0 | 20 |
| 1 | 1 | 80 |
| 2 | 2 | 320 |
| 3 | 3 | 1 280 |

Multiples of 8 are wire-only (no shaded mesh) to exercise WIREFRAME mode for
parts that genuinely have no triangle geometry.

---

## Roadmap / planned follow-on work

The items below were identified as important but deferred for later phases:

**Phase 3 — Expanded API-path correctness checks**
- Verify that `setHiddenInstances(all)` produces a blank render.
- Verify that `updateInstanceTransform` moves parts to the correct new position
  (image-difference check against a reference render).
- Verify pick stability across transform updates.

**Phase 3 — Regression gating**
- Capture per-scenario timing baselines.
- Assert SoCAD-vs-SG speedup ratio ≥ defined threshold at medium scale.
- Alert on sustained regressions (configurable tolerance).

**Phase 4 — Heavy-scale / nightly limit probing**
- Add a `--heavy` flag that runs 10 000-instance and 50 000-instance scenarios
  (omitted from default `ctest` to stay within CI time budget).
- Record trend data across CI runs for anomaly detection.

**Phase 4 — True deep-hierarchy SG comparison**
- Build a proper nested `SoSeparator` tree (depth × branch factor = leaf count)
  as a third comparison point alongside the flat SG baseline, to demonstrate
  the O(depth × nodes) traversal cost penalty for deep conventional scene graphs.
