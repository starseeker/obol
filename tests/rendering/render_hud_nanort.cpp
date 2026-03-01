/*
 * render_hud_nanort.cpp – NanoRT HUD text overlay validation test.
 *
 * Validates that SoHUDLabel text is correctly rasterized via SbFont and
 * alpha-composited onto the NanoRT framebuffer by nrtHUDLabelPreCB in
 * nanort_context_manager.h.
 *
 * Scene: createHUD(W, H) — a blue cube in a 3-D perspective scene with a
 * yellow "HUD Overlay" label composited on top via SoHUDKit/SoHUDLabel.
 *
 * Test passes when:
 *   - At least one "yellow" pixel (high R ≈ G, low B) is found in the top
 *     half of the image (where the HUD label is placed by createHUD),
 *     confirming that SoHUDLabel text was rasterized and composited.
 *   - The overall image is non-blank (at least some non-background pixels
 *     from the 3-D cube geometry).
 *
 * Viewport : 800 × 600
 * Output   : argv[1]+".rgb" (SGI RGB, used by the image-comparison infra)
 *
 * Building / linking: see CMakeLists.txt add_nanort_testlib_rendering_test()
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

#ifdef OBOL_NANORT_BUILD
// Thresholds for yellow-pixel detection (matches the createHUD label color:
// SbColor(1.0, 1.0, 0.2) → R≈255, G≈255, B≈51 before shading).
static const int YELLOW_MIN_RED     = 120;  // minimum R channel value
static const int YELLOW_MIN_GREEN   = 100;  // minimum G channel value
static const int YELLOW_MAX_BLUE    = 80;   // maximum B channel value
static const int YELLOW_RG_TOLERANCE = 60;  // max |R-G| for yellow hue

// Minimum pixel counts for the test to pass.
static const int MIN_NON_BACKGROUND_PIXELS = 500;  // scene must have some geometry
static const int MIN_YELLOW_PIXELS         = 10;   // at least a few HUD text pixels

static bool validateHUDText(const unsigned char * buf, int w, int h)
{
    // "HUD Overlay" label: yellow (R≈G, B low) positioned near the top.
    // createHUD places the label at y = height - 30, so within the top
    // ~10% of the image (top in GL = high y row = high index in GL buffer).
    int yellowInTopHalf = 0;
    int nonBg = 0;

    // The GL framebuffer stores row 0 at the bottom.
    // "Top half" means rows h/2 .. h-1 in GL convention.
    for (int row = h / 2; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const unsigned char * p = buf + (row * w + col) * 3;
            const int r = p[0], g = p[1], b = p[2];
            // Yellow: R and G both elevated, B clearly lower, R≈G within tolerance
            if (r > YELLOW_MIN_RED && g > YELLOW_MIN_GREEN &&
                b < YELLOW_MAX_BLUE && std::abs(r - g) < YELLOW_RG_TOLERANCE)
                ++yellowInTopHalf;
        }
    }
    for (int i = 0; i < w * h; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 10 || p[1] > 10 || p[2] > 10) ++nonBg;
    }

    printf("render_hud_nanort: yellowInTopHalf=%d nonBg=%d\n",
           yellowInTopHalf, nonBg);

    if (nonBg < MIN_NON_BACKGROUND_PIXELS) {
        fprintf(stderr,
                "render_hud_nanort: FAIL – image appears blank (%d non-bg pixels)\n",
                nonBg);
        return false;
    }
    if (yellowInTopHalf < MIN_YELLOW_PIXELS) {
        fprintf(stderr,
                "render_hud_nanort: FAIL – no yellow HUD text found in top half "
                "(%d yellow pixels); SoHUDLabel compositing may not be working\n",
                yellowInTopHalf);
        return false;
    }
    printf("render_hud_nanort: PASS\n");
    return true;
}
#endif // OBOL_NANORT_BUILD

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = ObolTest::Scenes::createHUD(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_nanort.rgb");

    bool ok = false;

#ifdef OBOL_NANORT_BUILD
    SoOffscreenRenderer renderer(SbViewportRegion(W, H));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateHUDText(buf, W, H);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_hud_nanort: render() failed\n");
    }
#else
    ok = renderToFile(root, outpath, W, H);
#endif

    root->unref();
    return ok ? 0 : 1;
}
