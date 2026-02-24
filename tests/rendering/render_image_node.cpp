/*
 * render_image_node.cpp - Integration test: SoImage node rendering
 *
 * Creates an SoImage node with a checkerboard of red/blue 8×8 pixels.
 * Renders and verifies that both red and blue pixel values are present
 * in the output buffer.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateImage(const unsigned char * buf)
{
    int redFound = 0, blueFound = 0;

    // Scan the full buffer for recognisably red or blue pixels
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x += 2) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 50 && p[2] < 50) ++redFound;
            if (p[2] > 150 && p[0] < 50 && p[1] < 50) ++blueFound;
        }
    }

    printf("render_image_node: redFound=%d blueFound=%d\n", redFound, blueFound);

    if (redFound < 2 || blueFound < 2) {
        fprintf(stderr,
                "render_image_node: FAIL - expected both red and blue pixels "
                "(red=%d, blue=%d)\n", redFound, blueFound);
        return false;
    }
    printf("render_image_node: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    // Build checkerboard pixel data (8×8, RGB, red/blue alternating)
    static const int IMG_W = 8, IMG_H = 8;
    unsigned char pixels[IMG_W * IMG_H * 3];
    for (int row = 0; row < IMG_H; ++row) {
        for (int col = 0; col < IMG_W; ++col) {
            unsigned char * p = pixels + (row * IMG_W + col) * 3;
            if ((row + col) % 2 == 0) {
                p[0] = 255; p[1] = 0; p[2] = 0;   // red
            } else {
                p[0] = 0;   p[1] = 0; p[2] = 255; // blue
            }
        }
    }

    SoSeparator * root = new SoSeparator;
    root->ref();

    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoImage * img = new SoImage;
    img->image.setValue(SbVec2s(IMG_W, IMG_H), 3, pixels);
    root->addChild(img);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.2f, 0.2f, 0.2f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_image_node.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateImage(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_image_node: render() failed\n");
        renderer.writeToRGB(outpath);
    }

    root->unref();
    return ok ? 0 : 1;
}
