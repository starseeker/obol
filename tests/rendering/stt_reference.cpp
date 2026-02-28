/*
 * stt_reference.cpp -- stb_truetype direct-render reference tool
 *
 * Renders text directly from the embedded ProFont TTF via SbFont/stb_truetype
 * WITHOUT going through any OpenGL layer.  The output PNG represents what the
 * text should look like at the highest possible quality from stb_truetype, and
 * can be compared against the OpenGL render_text2 output to evaluate fidelity.
 *
 * Scene matches render_text2.cpp:
 *   - "Hello World"  at 14px, white
 *   - "Coin3D Text"  at 18px, cyan
 *   - "ABC 123"      at 12px, yellow
 *   - "Line Two"     at 12px, yellow
 *   - "3D Test"      at 20px, green  (from render_text_demo)
 *
 * Usage: stt_reference <output.png>
 *
 * The tool exits with 0 on success and 1 on any error.  It does NOT require
 * an OpenGL context or display server.
 */

#include <Inventor/SbFont.h>
#include <Inventor/SbVec2s.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const int IMG_W    = 400;
static const int IMG_H    = 200;
static const int CHANNELS = 4;   // RGBA

// Composite a glyph bitmap over an RGBA canvas using the same formula as
// SoText2::GLRender's pixel-buffer path.
static void blit_glyph(unsigned char *canvas, int cw, int ch,
                        const unsigned char *bitmap, int bw, int bh,
                        int dst_x, int dst_y,
                        unsigned char cr, unsigned char cg, unsigned char cb)
{
    for (int gy = 0; gy < bh; gy++) {
        int dy = dst_y + gy;
        if (dy < 0 || dy >= ch) continue;
        for (int gx = 0; gx < bw; gx++) {
            int dx = dst_x + gx;
            if (dx < 0 || dx >= cw) continue;
            const int srcval = bitmap[gy * bw + gx];
            unsigned char *dst = canvas + (dy * cw + dx) * CHANNELS;
            // Same blending formula as SoText2::GLRender (alpha_material = 256):
            //   blended = ((256 - srcval) * old_alpha + srcval * 255) >> 8
            const int blended = ((256 - srcval) * (int)dst[3] +
                                  srcval * 255) >> 8;
            const unsigned char newval = (unsigned char)(blended < 255 ? blended : 255);
            if (newval > dst[3]) {
                dst[0] = cr; dst[1] = cg; dst[2] = cb; dst[3] = newval;
            }
        }
    }
}

struct TextSpec {
    float       size;
    const char *text;
    int         x_start;
    int         y_top;
    unsigned char r, g, b;
};

// approximate pixel positions for an 800×600 viewport
static const TextSpec k_rows[] = {
    { 14.0f, "Hello World",  5,   5, 255, 255, 255 },
    { 18.0f, "Coin3D Text",  5,  35,   0, 255, 255 },
    { 12.0f, "ABC 123",      5,  75, 255, 255,   0 },
    { 12.0f, "Line Two",     5,  95, 255, 255,   0 },
    { 20.0f, "3D Test",      5, 130,  51, 204,  76 },
    {  0.0f, NULL,           0,   0,   0,   0,   0 }
};

int main(int argc, char **argv)
{
    const char *outpath = (argc > 1) ? argv[1] : "/tmp/stt_reference.png";

    unsigned char *canvas =
        (unsigned char *)calloc((size_t)IMG_W * IMG_H * CHANNELS, 1);
    if (!canvas) {
        fprintf(stderr, "stt_reference: out of memory\n");
        return 1;
    }

    for (int ri = 0; k_rows[ri].text != NULL; ri++) {
        SbFont font;
        font.setSize(k_rows[ri].size);

        int x = k_rows[ri].x_start;
        const char *p = k_rows[ri].text;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            const unsigned char *bitmap = font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);

            if (bitmap && sz[0] > 0 && sz[1] > 0) {
                // bearing[1] = number of pixels above baseline (positive = up)
                const int dst_x = x + bearing[0];
                const int dst_y = k_rows[ri].y_top
                                  + static_cast<int>(k_rows[ri].size)
                                  - bearing[1];
                blit_glyph(canvas, IMG_W, IMG_H,
                           bitmap, sz[0], sz[1],
                           dst_x, dst_y,
                           k_rows[ri].r, k_rows[ri].g, k_rows[ri].b);
            }
            x += static_cast<int>(adv[0]);
        }
    }

    const int written = stbi_write_png(outpath,
                                       IMG_W, IMG_H, CHANNELS,
                                       canvas, IMG_W * CHANNELS);
    free(canvas);

    if (written) {
        printf("stt_reference: saved %s (%dx%d)\n", outpath, IMG_W, IMG_H);
        return 0;
    }
    fprintf(stderr, "stt_reference: failed to write %s\n", outpath);
    return 1;
}
