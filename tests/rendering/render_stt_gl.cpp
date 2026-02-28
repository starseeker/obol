/*
 * render_stt_gl.cpp - Visual regression test: createSTTGL scene
 *
 * Scene built by ObolTest::Scenes::createSTTGL.
 * Viewport: 400 x 200
 * Output: argv[1]+".rgb" (SGI RGB format)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 400;
static const int H = 200;

int main(int argc, char** argv)
{
    initCoinHeadless();

    SoSeparator* root = ObolTest::Scenes::createSTTGL(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_stt_gl.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
