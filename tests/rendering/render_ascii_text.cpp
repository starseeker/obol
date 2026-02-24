/*
 * render_ascii_text.cpp - Integration test: SoAsciiText rendering
 *
 * Renders SoAsciiText with string "HELLO" and verifies that non-black pixels
 * appear in the centre of the image (text is rendered against a dark background).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateAsciiText(const unsigned char * buf)
{
    int litFound = 0;

    // Centre region of the image should have lit pixels (text)
    for (int y = H / 4; y < 3 * H / 4; y += 2) {
        for (int x = 0; x < W; x += 2) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 100 || p[1] > 100 || p[2] > 100)
                ++litFound;
        }
    }

    printf("render_ascii_text: litFound=%d\n", litFound);

    if (litFound < 5) {
        fprintf(stderr, "render_ascii_text: FAIL - no lit pixels found for text\n");
        return false;
    }
    printf("render_ascii_text: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = new SoSeparator;
    root->ref();

    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 10.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 4.0f;
    root->addChild(cam);

    // White text
    SoBaseColor * color = new SoBaseColor;
    color->rgb.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(color);

    // Centre the text by translating slightly left
    SoTransform * xf = new SoTransform;
    xf->translation.setValue(-1.5f, -0.3f, 0.0f);
    root->addChild(xf);

    SoAsciiText * text = new SoAsciiText;
    text->string.setValue("HELLO");
    text->justification.setValue(SoAsciiText::LEFT);
    root->addChild(text);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_ascii_text.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateAsciiText(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_ascii_text: render() failed\n");
        // Write output file even on failure to ensure file exists for debugging
        renderer.writeToRGB(outpath);
    }

    root->unref();
    return ok ? 0 : 1;
}
