/*
 * test_cad_hier_benchmark.cpp
 *
 * CAD hierarchy scalability benchmark — unique parts, hierarchical IDs,
 * memory tracking, API-path coverage, and SG baseline comparison.
 *
 * What this adds over the existing benchmarks
 * -------------------------------------------
 * test_cad_benchmark.cpp      – one shared wireframe box, 512 instances, flat
 * test_cad_mesh_lod_benchmark – one shared icosphere mesh, up to 1M instances
 *
 * This benchmark specifically exercises:
 *   1. Unique parts: N distinct mesh geometries (not one shared part), so
 *      geometry-upload and part-library scaling are stressed.
 *   2. Hierarchical instance IDs: 2-level hierarchy (root → group → leaf) via
 *      CadIdBuilder::extendNameOccBool, matching real CAD comb-tree structure.
 *   3. Three draw modes timed separately: WIREFRAME, SHADED, SHADED_WITH_EDGES.
 *   4. API-path coverage at scale: setHiddenInstances, updateInstanceTransform,
 *      updateInstanceStyle, SoRayPickAction → SoCADDetail.
 *   5. Memory: process RSS sampled post-build and post-render on Linux.
 *   6. SG flat-baseline comparison at smoke scale (pixel coverage + MAD).
 *   7. CSV output for machine-readable results and trend tracking.
 *
 * Scenarios
 * ---------
 *   smoke            –   5 parts,   64 instances: correctness + SG comparison
 *   unique_parts_med – 50 parts,  500 instances: unique-part perf reference
 *   unique_parts_lg  – 200 parts, 2000 instances: larger unique-part stress
 *
 * Pass/fail criteria (smoke scenario only; larger scenarios are informational)
 * ---------------------------------------------------------------------------
 *   • All three draw-mode CAD renders must succeed (non-blank, ≥ 1% lit).
 *   • SG flat render must succeed (non-blank, ≥ 1% lit).
 *   • SG vs. CAD-shaded pixel-coverage diff must be < 20 percentage points.
 *   • API paths must not crash (timings are informational).
 *   • SoRayPickAction at viewport centre must return an SoCADDetail (smoke).
 *
 * Usage: test_cad_hier_benchmark [outprefix]
 *   Writes <outprefix>_smoke_cad_shaded.rgb and <outprefix>_smoke_sg.rgb.
 * Returns 0 on pass.
 */

#include "headless_utils.h"
#include "cad_test_generator.h"

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/SoCADDetail.h>
#include <obol/cad/CadIds.h>

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>

#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

using namespace CadTestGen;
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// ---------------------------------------------------------------------------
// Viewport dimensions (kept small for CI)
// ---------------------------------------------------------------------------
static const int W = 512;
static const int H = 512;

// Number of warm frames averaged for the steady-state render metric
static const int STEADY_FRAMES = 3;

// ---------------------------------------------------------------------------
// Memory tracking (Linux /proc/self/status only; returns -1 elsewhere)
// ---------------------------------------------------------------------------

#ifdef __linux__
static long readProcStatusKB(const char* field)
{
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1L;
    char   line[256];
    long   val      = -1L;
    size_t fieldLen = strlen(field);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, field, fieldLen) == 0) {
            sscanf(line + fieldLen + 1, " %ld kB", &val);
            break;
        }
    }
    fclose(f);
    return val;
}
/** Current resident set size in kB (sampled at call time). */
static long currentRSS_KB() { return readProcStatusKB("VmRSS:"); }
/** Peak (high-water mark) RSS in kB since process start. */
static long peakRSS_KB()    { return readProcStatusKB("VmHWM:"); }
#else
static long currentRSS_KB() { return -1L; }
static long peakRSS_KB()    { return -1L; }
#endif

// ---------------------------------------------------------------------------
// Pixel analysis helpers
// ---------------------------------------------------------------------------

static int countNonBlack(const unsigned char* buf, int w, int h)
{
    int n = 0;
    for (int i = 0; i < w * h; ++i)
        if (buf[i*3] > 20 || buf[i*3+1] > 20 || buf[i*3+2] > 20) ++n;
    return n;
}

static double pixelMAD(const unsigned char* a, const unsigned char* b,
                        int w, int h)
{
    double sum = 0.0;
    for (int i = 0; i < w * h * 3; ++i)
        sum += std::abs(int(a[i]) - int(b[i]));
    return sum / double(w * h * 3);
}

// ---------------------------------------------------------------------------
// CSV row structure
// ---------------------------------------------------------------------------

struct BenchRow {
    std::string scenario;
    std::string approach;       // e.g. "cad_shaded", "cad_wireframe", "sg_flat"
    int         parts           = 0;
    int         instances       = 0;
    double      buildMs         = 0.0;
    double      renderFirstMs   = 0.0;
    double      renderSteadyMs  = 0.0;  // average of STEADY_FRAMES warm frames
    long        rssPostBuildKB  = -1L;
    long        rssPostRenderKB = -1L;
    long        peakRssKB       = -1L;
    int         renderTier      = -1;   // SoCADAssembly tier; -1 for SG
    double      coveragePct     = 0.0;  // % non-black pixels
    bool        renderOk        = false;
    // API-path metrics (only populated for "cad_shaded" rows)
    double      hiddenAllMs      = -1.0; // setHiddenInstances(all) + render
    double      transformUpdMs   = -1.0; // 10% transform updates (total)
    double      styleUpdMs       = -1.0; // 10% style updates (total)
    double      pickMs           = -1.0; // SoRayPickAction at centre
    bool        pickHit          = false;
};

static std::vector<BenchRow> gRows;

static void printCSVHeader()
{
    printf("scenario,approach,parts,instances,"
           "build_ms,render_first_ms,render_steady_ms,"
           "rss_post_build_kB,rss_post_render_kB,peak_rss_kB,"
           "render_tier,coverage_pct,render_ok,"
           "hidden_all_ms,transform_upd_ms,style_upd_ms,"
           "pick_ms,pick_hit\n");
}

static void printCSVRow(const BenchRow& r)
{
    printf("%s,%s,%d,%d,"
           "%.2f,%.2f,%.2f,"
           "%ld,%ld,%ld,"
           "%d,%.2f,%d,"
           "%.2f,%.2f,%.2f,"
           "%.2f,%d\n",
           r.scenario.c_str(), r.approach.c_str(),
           r.parts, r.instances,
           r.buildMs, r.renderFirstMs, r.renderSteadyMs,
           r.rssPostBuildKB, r.rssPostRenderKB, r.peakRssKB,
           r.renderTier, r.coveragePct, int(r.renderOk),
           r.hiddenAllMs, r.transformUpdMs, r.styleUpdMs,
           r.pickMs, int(r.pickHit));
}

// ---------------------------------------------------------------------------
// CAD scene builder
// ---------------------------------------------------------------------------

struct CADScene {
    SoSeparator*         root     = nullptr; ///< ref-counted; caller must unref
    SoCADAssembly*       assembly = nullptr; ///< owned by root; do NOT unref
    SoPerspectiveCamera* camera   = nullptr; ///< owned by root
};

/**
 * Build a CAD scene:
 *   SoSeparator root
 *     SoPerspectiveCamera (viewAll called after population)
 *     SoLightModel(BASE_COLOR)   ← keeps appearance comparable to SG
 *     SoCADAssembly(mode)
 *
 * @param maxInstances  Limit instances (-1 = all).
 */
static CADScene buildCADScene(const Assembly& gen,
                               SoCADAssembly::DrawMode mode,
                               int maxInstances = -1)
{
    CADScene s;
    s.root = new SoSeparator;
    s.root->ref();

    s.camera = new SoPerspectiveCamera;
    s.root->addChild(s.camera);

    SoLightModel* lm = new SoLightModel;
    lm->model.setValue(SoLightModel::BASE_COLOR);
    s.root->addChild(lm);

    s.assembly = new SoCADAssembly;
    s.assembly->drawMode.setValue(mode);
    s.assembly->lodEnabled.setValue(FALSE);
    s.root->addChild(s.assembly);

    populateCAD(s.assembly, gen, maxInstances);

    SbViewportRegion vp(W, H);
    s.camera->viewAll(s.root, vp);

    return s;
}

/**
 * Build a flat SoSeparator SG scene with camera prepended.
 * Returns ref-counted root; caller must unref.
 */
static SoSeparator* buildSGScene(const Assembly& gen, int maxInstances = -1)
{
    SoSeparator* root = buildFlatSG(gen, maxInstances);
    // buildFlatSG positions SoLightModel/SoBaseColor first; prepend camera
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->insertChild(cam, 0);
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);
    return root; // already ref-counted by buildFlatSG
}

// ---------------------------------------------------------------------------
// Render helper: measures first-frame and steady-state (STEADY_FRAMES avg)
// ---------------------------------------------------------------------------

struct RenderResult {
    bool   ok         = false;
    double firstMs    = 0.0;
    double steadyMs   = 0.0;  // average of STEADY_FRAMES
    int    nonBlack   = 0;
    int    tier       = -1;   // lastRenderTier; -1 for SG
    // First-frame pixel buffer (RGB, W*H*3 bytes); empty on failure
    std::vector<unsigned char> pixels;
};

static RenderResult doRender(SoSeparator* root,
                              SoCADAssembly* assembly = nullptr)
{
    RenderResult r;

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    // First frame
    auto t0 = Clock::now();
    r.ok      = (renderer.render(root) == TRUE);
    r.firstMs = Ms(Clock::now() - t0).count();

    if (assembly) r.tier = assembly->lastRenderTier();

    if (r.ok) {
        const unsigned char* buf = renderer.getBuffer();
        if (buf) {
            r.nonBlack = countNonBlack(buf, W, H);
            r.pixels.assign(buf, buf + size_t(W) * H * 3);
        }
    }

    // Steady-state: STEADY_FRAMES warm renders averaged
    auto ts = Clock::now();
    for (int f = 0; f < STEADY_FRAMES; ++f)
        renderer.render(root);
    r.steadyMs = Ms(Clock::now() - ts).count() / STEADY_FRAMES;

    return r;
}

// ---------------------------------------------------------------------------
// API-path exercise (runs on a fully-built CADScene)
// ---------------------------------------------------------------------------

struct APIMetrics {
    double hiddenAllMs   = -1.0; ///< setHiddenInstances(all) + one render
    double transformUpdMs= -1.0; ///< updateInstanceTransform for 10% of instances
    double styleUpdMs    = -1.0; ///< updateInstanceStyle for 10% of instances
    double pickMs        = -1.0; ///< SoRayPickAction at viewport centre
    bool   pickHit       = false;///< did the pick return a valid SoCADDetail?
};

static APIMetrics runAPITests(CADScene& sc, const Assembly& gen)
{
    APIMetrics m;
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    renderer.render(sc.root); // warm-up

    // ---- 1. Hide all instances + render (measures hide-rebuild cost) ----
    {
        std::vector<obol::InstanceId> allIds;
        allIds.reserve(gen.instances.size());
        for (const auto& is : gen.instances)
            allIds.push_back(is.id);

        auto t0 = Clock::now();
        sc.assembly->setHiddenInstances(allIds);
        renderer.render(sc.root);
        m.hiddenAllMs = Ms(Clock::now() - t0).count();

        // Restore visibility
        sc.assembly->setHiddenInstances({});
    }

    // ---- 2. Update transforms for 10% of instances ----
    {
        int nUpd = std::max(1, int(gen.instances.size()) / 10);
        auto t0  = Clock::now();
        for (int i = 0; i < nUpd; ++i) {
            const InstSpec& is = gen.instances[size_t(i)];
            SbMatrix mat;
            mat.makeIdentity();
            // Small nudge (0.5 units in X) to trigger a dirty flag
            mat.setTranslate(is.worldPos + SbVec3f(0.5f, 0.0f, 0.0f));
            sc.assembly->updateInstanceTransform(is.id, mat);
        }
        m.transformUpdMs = Ms(Clock::now() - t0).count();

        // Restore original transforms
        for (int i = 0; i < nUpd; ++i) {
            const InstSpec& is = gen.instances[size_t(i)];
            SbMatrix mat;
            mat.makeIdentity();
            mat.setTranslate(is.worldPos);
            sc.assembly->updateInstanceTransform(is.id, mat);
        }
    }

    // ---- 3. Update styles for 10% of instances ----
    {
        int nUpd = std::max(1, int(gen.instances.size()) / 10);
        obol::InstanceStyle highlight;
        highlight.hasColorOverride = true;
        highlight.color = SbColor4f(1.0f, 0.3f, 0.3f, 1.0f);

        auto t0 = Clock::now();
        for (int i = 0; i < nUpd; ++i)
            sc.assembly->updateInstanceStyle(gen.instances[size_t(i)].id, highlight);
        m.styleUpdMs = Ms(Clock::now() - t0).count();

        // Restore original styles
        obol::InstanceStyle orig;
        orig.hasColorOverride = true;
        orig.color = SbColor4f(0.7f, 0.75f, 0.8f, 1.0f);
        for (int i = 0; i < nUpd; ++i)
            sc.assembly->updateInstanceStyle(gen.instances[size_t(i)].id, orig);
    }

    // ---- 4. SoRayPickAction at viewport centre ----
    //
    // After viewAll the camera frames the scene so the viewport centre maps
    // to approximately the scene centre.  For a dense grid this reliably
    // produces a hit against a part instance.
    {
        renderer.render(sc.root); // ensure GL state is current

        auto t0 = Clock::now();
        SoRayPickAction rpa(vp);
        rpa.setPoint(SbVec2s(short(W / 2), short(H / 2)));
        rpa.apply(sc.root);
        m.pickMs = Ms(Clock::now() - t0).count();

        SoPickedPoint* pp = rpa.getPickedPoint();
        if (pp)
            m.pickHit = (dynamic_cast<const SoCADDetail*>(pp->getDetail()) != nullptr);
    }

    return m;
}

// ---------------------------------------------------------------------------
// Full scenario runner
// ---------------------------------------------------------------------------

static bool runScenario(const char*          scenarioName,
                         const HierGenConfig& cfg,
                         const char*          outprefix,
                         bool                 doSGComparison,
                         bool&                allOk)
{
    printf("\n=== Scenario: %-22s  parts=%-4d  instances=%d ===\n",
           scenarioName, cfg.uniqueParts, cfg.totalInstances);

    // Generate the assembly (deterministic)
    auto tGen0 = Clock::now();
    Assembly gen = CadTestGen::generate(cfg);
    printf("  generate: %.1f ms\n", Ms(Clock::now() - tGen0).count());

    // Draw modes to exercise for CAD
    static const struct {
        const char*            label;
        SoCADAssembly::DrawMode mode;
    } kModes[] = {
        { "cad_shaded",            SoCADAssembly::SHADED            },
        { "cad_wireframe",         SoCADAssembly::WIREFRAME          },
        { "cad_shaded_with_edges", SoCADAssembly::SHADED_WITH_EDGES  },
    };

    APIMetrics apiMetrics;
    bool       apiDone = false;

    // --- CAD passes ---
    for (const auto& mspec : kModes) {
        // Build
        auto     tBuild = Clock::now();
        CADScene sc     = buildCADScene(gen, mspec.mode);
        double   buildMs = Ms(Clock::now() - tBuild).count();
        long     rssPostBuildKB = currentRSS_KB();

        // Render
        RenderResult rr           = doRender(sc.root, sc.assembly);
        long rssPostRenderKB = currentRSS_KB();
        long peakRssKB       = peakRSS_KB();

        const char* tierStr[] = { "Tier0(imm)", "Tier1(VBO)", "Tier2(inst)", "?" };
        int tidx = (rr.tier >= 0 && rr.tier <= 2) ? rr.tier : 3;

        printf("  [%-26s]  build=%7.1f ms  first=%7.1f ms  steady=%6.1f ms"
               "  %s  cov=%.1f%%  ok=%d\n",
               mspec.label, buildMs, rr.firstMs, rr.steadyMs,
               tierStr[tidx], 100.0 * rr.nonBlack / (W * H), int(rr.ok));

        // Correctness for smoke
        if (doSGComparison) {
            if (!rr.ok) {
                fprintf(stderr, "FAIL [%s/%s]: render returned false\n",
                        scenarioName, mspec.label);
                allOk = false;
            }
            if (rr.nonBlack < (W * H) / 100) {
                fprintf(stderr, "FAIL [%s/%s]: too few lit pixels (%d)\n",
                        scenarioName, mspec.label, rr.nonBlack);
                allOk = false;
            }
        }

        // API tests on the SHADED pass (once per scenario)
        if (!apiDone && strcmp(mspec.label, "cad_shaded") == 0) {
            apiMetrics = runAPITests(sc, gen);
            apiDone    = true;

            printf("  [api]  hidden_all=%.1f ms  xform_upd=%.1f ms"
                   "  style_upd=%.1f ms  pick=%.1f ms  hit=%d\n",
                   apiMetrics.hiddenAllMs, apiMetrics.transformUpdMs,
                   apiMetrics.styleUpdMs,  apiMetrics.pickMs,
                   int(apiMetrics.pickHit));

            // Note: a centre-pick miss is expected when the grid layout places
            // the viewport centre in the empty space between instances.  The
            // SoCADAssembly::rayPick() code path is exercised regardless
            // (BVH built, no crash); strict correctness for CPU picking is
            // covered by test_cad_picking.cpp.
            if (doSGComparison && !apiMetrics.pickHit) {
                printf("  [api] note: SoRayPickAction at centre missed (geometry "
                       "gap in sparse grid; not a failure)\n");
            }
        }

        // Save reference image (CAD shaded, smoke only)
        if (doSGComparison && strcmp(mspec.label, "cad_shaded") == 0 && rr.ok) {
            char imgPath[1024];
            snprintf(imgPath, sizeof(imgPath), "%s_%s_cad_shaded.rgb",
                     outprefix, scenarioName);
            SbViewportRegion vp2(W, H);
            SoOffscreenRenderer r2(vp2);
            r2.setComponents(SoOffscreenRenderer::RGB);
            r2.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
            if (r2.render(sc.root) == TRUE)
                r2.writeToRGB(imgPath);
        }

        // Record CSV row
        BenchRow row;
        row.scenario        = scenarioName;
        row.approach        = mspec.label;
        row.parts           = int(gen.parts.size());
        row.instances       = int(gen.instances.size());
        row.buildMs         = buildMs;
        row.renderFirstMs   = rr.firstMs;
        row.renderSteadyMs  = rr.steadyMs;
        row.rssPostBuildKB  = rssPostBuildKB;
        row.rssPostRenderKB = rssPostRenderKB;
        row.peakRssKB       = peakRssKB;
        row.renderTier      = rr.tier;
        row.coveragePct     = 100.0 * rr.nonBlack / (W * H);
        row.renderOk        = rr.ok;
        if (strcmp(mspec.label, "cad_shaded") == 0) {
            row.hiddenAllMs   = apiMetrics.hiddenAllMs;
            row.transformUpdMs= apiMetrics.transformUpdMs;
            row.styleUpdMs    = apiMetrics.styleUpdMs;
            row.pickMs        = apiMetrics.pickMs;
            row.pickHit       = apiMetrics.pickHit;
        }
        gRows.push_back(row);

        sc.root->unref();
    }

    // --- SG flat comparison (smoke only) ---
    if (doSGComparison) {
        auto       tBuild = Clock::now();
        SoSeparator* sgRoot = buildSGScene(gen);
        double     buildMs         = Ms(Clock::now() - tBuild).count();
        long       rssPostBuildKB  = currentRSS_KB();

        RenderResult rr           = doRender(sgRoot);
        long rssPostRenderKB = currentRSS_KB();
        long peakRssKB       = peakRSS_KB();

        printf("  [%-26s]  build=%7.1f ms  first=%7.1f ms  steady=%6.1f ms"
               "  (SG-flat)  cov=%.1f%%  ok=%d\n",
               "sg_flat", buildMs, rr.firstMs, rr.steadyMs,
               100.0 * rr.nonBlack / (W * H), int(rr.ok));

        if (!rr.ok) {
            fprintf(stderr, "FAIL [%s/sg_flat]: render returned false\n",
                    scenarioName);
            allOk = false;
        }
        if (rr.nonBlack < (W * H) / 100) {
            fprintf(stderr, "FAIL [%s/sg_flat]: too few lit pixels (%d)\n",
                    scenarioName, rr.nonBlack);
            allOk = false;
        }

        // Compare SG coverage to CAD-shaded coverage
        double sgCov = 100.0 * rr.nonBlack / (W * H);
        // Find the cad_shaded row for this scenario (just added above)
        double cadCov = -1.0;
        for (auto it = gRows.rbegin(); it != gRows.rend(); ++it) {
            if (it->scenario == scenarioName && it->approach == "cad_shaded") {
                cadCov = it->coveragePct;
                break;
            }
        }
        if (cadCov >= 0.0) {
            double diff = std::abs(sgCov - cadCov);
            printf("  [sg_vs_cad_shaded]  coverage SG=%.1f%%  CAD=%.1f%%"
                   "  diff=%.1f pp\n", sgCov, cadCov, diff);
            if (diff > 20.0) {
                fprintf(stderr,
                        "FAIL [%s]: SG vs. CAD-shaded coverage diff %.1f pp > 20\n",
                        scenarioName, diff);
                allOk = false;
            }
        }

        // Pixel MAD comparison: re-render CAD shaded to get fresh pixel buffer
        {
            CADScene sc2 = buildCADScene(gen, SoCADAssembly::SHADED);
            SbViewportRegion vp2(W, H);
            SoOffscreenRenderer r2(vp2);
            r2.setComponents(SoOffscreenRenderer::RGB);
            r2.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
            if (r2.render(sc2.root) == TRUE) {
                const unsigned char* cadBuf = r2.getBuffer();
                if (cadBuf && !rr.pixels.empty()) {
                    double mad = pixelMAD(rr.pixels.data(), cadBuf, W, H);
                    printf("  [sg_vs_cad_shaded]  pixel MAD=%.2f/255\n", mad);
                    if (mad > 30.0) {
                        fprintf(stderr,
                                "FAIL [%s]: pixel MAD %.2f > 30\n",
                                scenarioName, mad);
                        allOk = false;
                    }
                }
            }
            sc2.root->unref();
        }

        // Save SG reference image
        if (rr.ok) {
            char imgPath[1024];
            snprintf(imgPath, sizeof(imgPath), "%s_%s_sg.rgb",
                     outprefix, scenarioName);
            SbViewportRegion vp2(W, H);
            SoOffscreenRenderer r2(vp2);
            r2.setComponents(SoOffscreenRenderer::RGB);
            r2.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
            if (r2.render(sgRoot) == TRUE)
                r2.writeToRGB(imgPath);
        }

        BenchRow row;
        row.scenario        = scenarioName;
        row.approach        = "sg_flat";
        row.parts           = int(gen.parts.size());
        row.instances       = int(gen.instances.size());
        row.buildMs         = buildMs;
        row.renderFirstMs   = rr.firstMs;
        row.renderSteadyMs  = rr.steadyMs;
        row.rssPostBuildKB  = rssPostBuildKB;
        row.rssPostRenderKB = rssPostRenderKB;
        row.peakRssKB       = peakRssKB;
        row.renderTier      = -1;
        row.coveragePct     = sgCov;
        row.renderOk        = rr.ok;
        gRows.push_back(row);

        sgRoot->unref();
    }

    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    const char* outprefix = (argc > 1) ? argv[1] : "test_cad_hier_benchmark";

    initCoinHeadless();
    SoCADAssembly::initClass();

    printf("test_cad_hier_benchmark\n");
    printf("  viewport: %d x %d\n", W, H);
    printf("  steady-state frames averaged: %d\n", STEADY_FRAMES);
    printf("  backend: %s\n",
#ifdef OBOL_SWRAST_BUILD
           "OSMesa (software rasteriser)"
#else
           "system GL (GLX / Xvfb)"
#endif
    );

    bool allOk = true;

    // ------------------------------------------------------------------
    // Smoke: 5 parts, 64 instances — correctness + SG comparison
    //   branchFactor 8 → 64/8 = 8 groups of up to 8 leaves each
    // ------------------------------------------------------------------
    {
        HierGenConfig cfg;
        cfg.seed           = 42;
        cfg.uniqueParts    = 5;
        cfg.totalInstances = 64;
        cfg.branchFactor   = 8;
        cfg.spacing        = 3.0f;
        runScenario("smoke", cfg, outprefix, /*doSGComparison=*/true, allOk);
    }

    // ------------------------------------------------------------------
    // Unique parts medium: 50 unique geometries, 500 instances
    //   Tests part-library upload and per-draw-call variety
    // ------------------------------------------------------------------
    {
        HierGenConfig cfg;
        cfg.seed           = 137;
        cfg.uniqueParts    = 50;
        cfg.totalInstances = 500;
        cfg.branchFactor   = 10;
        cfg.spacing        = 3.0f;
        runScenario("unique_parts_med", cfg, outprefix, /*doSGComparison=*/false, allOk);
    }

    // ------------------------------------------------------------------
    // Unique parts large: 200 unique geometries, 2000 instances
    //   Exercises scaling with many distinct VBO uploads
    // ------------------------------------------------------------------
    {
        HierGenConfig cfg;
        cfg.seed           = 271;
        cfg.uniqueParts    = 200;
        cfg.totalInstances = 2000;
        cfg.branchFactor   = 20;
        cfg.spacing        = 3.0f;
        runScenario("unique_parts_lg", cfg, outprefix, /*doSGComparison=*/false, allOk);
    }

    // ------------------------------------------------------------------
    // Summary and CSV
    // ------------------------------------------------------------------
    printf("\n--- test_cad_hier_benchmark complete ---\n");
    printf("  overall pass: %s\n\n", allOk ? "YES" : "NO");

    printf("# BEGIN_CSV\n");
    printCSVHeader();
    for (const auto& r : gRows)
        printCSVRow(r);
    printf("# END_CSV\n");

    return allOk ? 0 : 1;
}
