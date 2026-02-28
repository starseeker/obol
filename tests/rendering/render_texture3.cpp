/*
 * render_texture3.cpp - Visual regression test: createTexture3 scene
 *
 * Scene built by ObolTest::Scenes::createTexture3.
 * Viewport: 800 x 600
 * Output: argv[1]+".rgb" (SGI RGB format)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char** argv)
{
    initCoinHeadless();

    SoSeparator* root = ObolTest::Scenes::createTexture3(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_texture3.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
