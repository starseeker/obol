/*
 * render_texture3.cpp - Integration test: SoTexture3 (3-D texture) on a cube
 *
 * Uses ObolTest::Scenes::createTexture3 — the same factory used by the
 * interactive obol_viewer — so CLI and viewer render identical scenes.
 *
 * Pixel validation: the rendered cube must be non-blank.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static int countNonBackground(const unsigned char *buf)
{
    int count = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++count;
    }
    return count;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_texture3.rgb");

    SoSeparator *root = ObolTest::Scenes::createTexture3(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool ok = renderer.render(root);
    int nb  = ok ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_texture3: ok=%d nonbg=%d\n", ok, nb);
    renderer.writeToRGB(outpath);

    root->unref();

    if (!ok || nb < 100) {
        fprintf(stderr, "render_texture3: FAIL – scene blank (nb=%d)\n", nb);
        return 1;
    }
    printf("render_texture3: PASS\n");
    return 0;
}
