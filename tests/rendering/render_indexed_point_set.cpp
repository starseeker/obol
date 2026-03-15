/*
 * render_indexed_point_set.cpp - Integration test: SoIndexedPointSet
 *
 * Renders four points using SoIndexedPointSet with explicit coord indices.
 * The scene is built by the shared testlib factory (createIndexedPointSet).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateIndexedPointSet(const unsigned char *buf)
{
    // The scene has four emissive red points at (±0.5, ±0.5).
    // Camera: ortho, height=2, from z=1.
    // World (0.5, 0.5) → pixel (3W/4, 3H/4) (top-right quadrant).
    // World (-0.5, -0.5) → pixel (W/4, H/4) (bottom-left quadrant).
    // Check that non-background pixels exist in each quadrant.
    int topRight = 0, bottomLeft = 0;
    for (int y = H/2; y < H; ++y) {
        for (int x = W/2; x < W; ++x) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 120 && p[1] < 60 && p[2] < 60) ++topRight;
        }
    }
    for (int y = 0; y < H/2; ++y) {
        for (int x = 0; x < W/2; ++x) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 120 && p[1] < 60 && p[2] < 60) ++bottomLeft;
        }
    }
    printf("render_indexed_point_set: topRight=%d bottomLeft=%d\n",
           topRight, bottomLeft);
    if (topRight == 0) {
        fprintf(stderr, "render_indexed_point_set: FAIL – no red pixels top-right\n");
        return false;
    }
    if (bottomLeft == 0) {
        fprintf(stderr, "render_indexed_point_set: FAIL – no red pixels bottom-left\n");
        return false;
    }
    printf("render_indexed_point_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createIndexedPointSet(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_indexed_point_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateIndexedPointSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_indexed_point_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
