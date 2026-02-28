// Test just the multi-casters scenario in isolation
#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>
#include <Inventor/annex/FXViz/nodes/SoShadowCulling.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

int main() {
    initCoinHeadless();
    
    SoSeparator *root = new SoSeparator;
    root->ref();
    
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 5.0f, 10.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.45f);
    cam->nearDistance = 0.1f;
    cam->farDistance = 60.0f;
    root->addChild(cam);
    
    SoShadowGroup *sg = new SoShadowGroup;
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.7f);
    sg->precision.setValue(0.3f);
    sg->quality.setValue(1.0f);
    sg->smoothBorder.setValue(0.0f);
    sg->shadowCachingEnabled.setValue(TRUE);
    root->addChild(sg);
    
    SoShadowDirectionalLight *dlight = new SoShadowDirectionalLight;
    dlight->direction.setValue(-0.4f, -1.0f, -0.3f);
    dlight->intensity.setValue(0.9f);
    sg->addChild(dlight);
    
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.9f);
    sg->addChild(mat);
    sg->addChild(new SoCube);
    
    // First render
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(SbViewportRegion(W, H));
    bool ok1 = ren->render(root);
    
    const unsigned char *buf1 = ren->getBuffer();
    int nonbg1 = 0;
    for (int i = 0; i < W*H; i++) {
        const unsigned char *p = buf1 + i*3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg1;
    }
    printf("First render: ok=%d nonbg=%d\n", (int)ok1, nonbg1);
    
    // Second render (same scene)
    bool ok2 = ren->render(root);
    const unsigned char *buf2 = ren->getBuffer();
    int nonbg2 = 0;
    for (int i = 0; i < W*H; i++) {
        const unsigned char *p = buf2 + i*3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg2;
    }
    printf("Second render: ok=%d nonbg=%d\n", (int)ok2, nonbg2);
    
    root->unref();
    return (nonbg1 > 50 && nonbg2 > 50) ? 0 : 1;
}
