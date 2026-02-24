/*
 * render_triangle_strip_set.cpp - Integration test: SoTriangleStripSet rendering
 *
 * Renders a triangle strip forming a quad (2 triangles) using SoTriangleStripSet
 * and SoIndexedTriangleStripSet.  The geometry is emissive blue against a black
 * background.  Pixel validation confirms:
 *   - The lower portion of the buffer contains blue pixels.
 *   - The upper portion contains background (black) pixels.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateTriangleStrip(const unsigned char * buf)
{
    int blueFound = 0, blackFound = 0;

    // Lower portion (y: 0..H/4, x: W/4..3W/4) should contain blue pixels
    for (int y = 0; y < H / 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[2] > 150 && p[0] < 50 && p[1] < 50)
                ++blueFound;
        }
    }

    // Upper portion (y: 3H/4..H) should be black background
    for (int y = 3 * H / 4; y < H; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20)
                ++blackFound;
        }
    }

    printf("render_triangle_strip_set: blueFound=%d blackFound=%d\n",
           blueFound, blackFound);

    if (blueFound < 5) {
        fprintf(stderr, "render_triangle_strip_set: FAIL - not enough blue pixels\n");
        return false;
    }
    if (blackFound < 5) {
        fprintf(stderr, "render_triangle_strip_set: FAIL - not enough black pixels\n");
        return false;
    }
    printf("render_triangle_strip_set: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = new SoSeparator;
    root->ref();

    // Orthographic camera
    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator * grp = new SoSeparator;

    // Emissive blue via SoBaseColor
    SoBaseColor * color = new SoBaseColor;
    color->rgb.setValue(0.0f, 0.0f, 1.0f);
    grp->addChild(color);

    // 4 vertices forming a quad in the lower half: y in [-1, 0]
    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.8f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.8f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.8f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.8f,  0.0f, 0.0f));
    grp->addChild(coords);

    // Triangle strip: one strip of 4 vertices = 2 triangles
    SoTriangleStripSet * strips = new SoTriangleStripSet;
    strips->numVertices.set1Value(0, 4);
    grp->addChild(strips);

    root->addChild(grp);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_triangle_strip_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateTriangleStrip(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_triangle_strip_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
