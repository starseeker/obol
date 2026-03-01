/*
 * render_scene_texture_state.cpp
 *
 * Regression test for the bug: "Rendering/scene_texture is leaving OSMesa in
 * a bad state - after selecting it in obol_viewer, selecting any other test
 * results in a blank window with no visuals rendered."
 *
 * The obol_viewer OSMesaPanel reuses a single SoOffscreenRenderer across
 * scene changes.  This test simulates that usage pattern:
 *
 *   1. Render a scene that contains a SoSceneTexture2 node ("scene_texture").
 *   2. Release that scene root.
 *   3. Render a simple unrelated scene (a red cone) with THE SAME renderer.
 *   4. Verify the second render is NOT blank.
 *
 * Before the fix, step 3 would produce a blank image because OSMesa was
 * left in a bad state after step 1.
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoSceneTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// ---- helpers ----------------------------------------------------------------

static SoSeparator *buildConeSubScene()
{
    SoSeparator *scene = new SoSeparator;
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    scene->addChild(cam);
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    scene->addChild(light);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    scene->addChild(mat);
    SoCone *cone = new SoCone;
    scene->addChild(cone);
    SbViewportRegion vp(128, 128);
    cam->viewAll(scene, vp);
    return scene;
}

/* Scene 1: a flat quad with a SoSceneTexture2 render-to-texture applied. */
static SoSeparator *buildSceneTextureScene()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2 *stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(128, 128));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(buildConeSubScene());
    root->addChild(stex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    return root;
}

/* Scene 2: a simple red cone with no special texturing. */
static SoSeparator *buildSimpleScene()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 0.1f, 0.1f);  // bright red
    root->addChild(mat);

    SoCone *cone = new SoCone;
    root->addChild(cone);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    return root;
}

static int countNonBackground(const unsigned char *buf, int w, int h)
{
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15)
            ++count;
    }
    return count;
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv)
{
    initCoinHeadless();

    /* Simulate the viewer's OSMesaPanel: a single renderer reused across
     * scene changes, backed by an explicit OSMesa context manager. */
    SoDB::ContextManager *mgr = getCoinHeadlessContextManager();
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(mgr, vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    bool all_ok = true;

    /* ---- Render 1: scene_texture ----------------------------------------- */
    {
        SoSeparator *root1 = buildSceneTextureScene();
        bool ok1 = renderer.render(root1);
        const unsigned char *buf1 = renderer.getBuffer();
        int nonbg1 = (buf1 && ok1) ? countNonBackground(buf1, W, H) : 0;
        printf("render_scene_texture_state: render1 (scene_texture) ok=%d nonbg=%d\n",
               ok1, nonbg1);

        if (argc > 1) {
            snprintf(outpath, sizeof(outpath), "%s_1.rgb", argv[1]);
            renderer.writeToRGB(outpath);
        }

        if (!ok1 || nonbg1 < 100) {
            fprintf(stderr, "render_scene_texture_state: FAIL – scene_texture render blank\n");
            all_ok = false;
        }
        /* Release the scene_texture scene root (mirrors viewer's scene switch). */
        root1->unref();
    }

    /* ---- Render 2: simple cone (same renderer, different scene) ----------- */
    {
        SoSeparator *root2 = buildSimpleScene();
        bool ok2 = renderer.render(root2);
        const unsigned char *buf2 = renderer.getBuffer();
        int nonbg2 = (buf2 && ok2) ? countNonBackground(buf2, W, H) : 0;
        printf("render_scene_texture_state: render2 (simple cone) ok=%d nonbg=%d\n",
               ok2, nonbg2);

        if (argc > 1) {
            snprintf(outpath, sizeof(outpath), "%s_2.rgb", argv[1]);
            renderer.writeToRGB(outpath);
        }

        if (!ok2 || nonbg2 < 100) {
            fprintf(stderr,
                    "render_scene_texture_state: FAIL – simple-cone render blank "
                    "after scene_texture (OSMesa left in bad state)\n");
            all_ok = false;
        }
        root2->unref();
    }

    if (all_ok)
        printf("render_scene_texture_state: PASS\n");
    else
        printf("render_scene_texture_state: FAIL\n");

    return all_ok ? 0 : 1;
}
