/*
 * obol_bridge.cpp  –  C bridge implementation for one Obol backend
 *
 * This file is compiled TWICE (once per backend) as part of the superbuild:
 *
 *   Backend 1 – system OpenGL / GLX:
 *     Coin built with COIN3D_USE_OSMESA=OFF
 *     bridge built without COIN3D_OSMESA_BUILD
 *     output: libobol_bridge_sys.so
 *
 *   Backend 2 – OSMesa (fully headless):
 *     Coin built with COIN3D_USE_OSMESA=ON  (→ USE_MGL_NAMESPACE, mgl* symbols)
 *     bridge built with COIN3D_OSMESA_BUILD
 *     output: libobol_bridge_osmesa.so
 *
 * Both .so files are loaded by obol_viewer with dlopen(RTLD_LOCAL) so their
 * C++ Obol symbols live in private namespaces and never collide.  Only the
 * obol_* C functions below are visible (exported via the linker version
 * script obol_bridge.map).
 *
 * Scene catalogue
 * ───────────────
 * A small set of self-contained scene factories is embedded directly in this
 * file (no dependency on tests/testlib) to keep the bridge self-contained.
 * Adding a new scene: append an entry to s_scenes[] at the bottom.
 */

#include "obol_bridge.h"

/* ---- Obol / Coin headers ---- */
#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <functional>

/* ---- Backend context manager (headless_utils.h pattern) ---- */

#ifdef COIN3D_OSMESA_BUILD
#  include <OSMesa/osmesa.h>
#  include <OSMesa/gl.h>
/* OSMesa context manager -------------------------------------------------- */
struct ObolOSMesaCtx {
    OSMesaContext ctx;
    std::unique_ptr<unsigned char[]> buf;
    int w, h;
    OSMesaContext prev_ctx;
    void*         prev_buf;
    GLsizei       prev_w, prev_h, prev_bpr;
    GLenum        prev_fmt;

    ObolOSMesaCtx(int w_, int h_) : w(w_), h(h_),
        prev_ctx(nullptr), prev_buf(nullptr),
        prev_w(0), prev_h(0), prev_bpr(0), prev_fmt(0)
    {
        ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, nullptr);
        if (ctx) buf = std::make_unique<unsigned char[]>(w * h * 4);
    }
    ~ObolOSMesaCtx() { if (ctx) OSMesaDestroyContext(ctx); }
    bool valid() const { return ctx != nullptr; }
    bool makeCurrent() {
        if (!ctx) return false;
        prev_ctx = OSMesaGetCurrentContext();
        prev_buf = nullptr; prev_w = prev_h = prev_bpr = 0; prev_fmt = 0;
        if (prev_ctx) {
            GLint fmt = 0;
            OSMesaGetColorBuffer(prev_ctx, &prev_w, &prev_h, &fmt, &prev_buf);
            prev_fmt = (GLenum)fmt;
        }
        return OSMesaMakeCurrent(ctx, buf.get(), GL_UNSIGNED_BYTE, w, h) != 0;
    }
};

class ObolOSMesaContextManager : public SoDB::ContextManager {
public:
    void* createOffscreenContext(unsigned int w, unsigned int h) override {
        auto* c = new ObolOSMesaCtx(w, h);
        return c->valid() ? c : (delete c, nullptr);
    }
    SbBool makeContextCurrent(void* c) override {
        return c && static_cast<ObolOSMesaCtx*>(c)->makeCurrent() ? TRUE : FALSE;
    }
    void restorePreviousContext(void* c) override {
        auto* ctx = static_cast<ObolOSMesaCtx*>(c);
        if (!ctx) return;
        if (ctx->prev_ctx && ctx->prev_buf)
            OSMesaMakeCurrent(ctx->prev_ctx, ctx->prev_buf,
                              GL_UNSIGNED_BYTE, ctx->prev_w, ctx->prev_h);
        else
            OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
    }
    void destroyContext(void* c) override { delete static_cast<ObolOSMesaCtx*>(c); }
};

static void bridge_init_coin() {
    static ObolOSMesaContextManager mgr;
    SoDB::init(&mgr);
    SoNodeKit::init();
    SoInteraction::init();
}

#else /* System OpenGL / GLX ------------------------------------------------ */
#  ifdef __unix__
#    include <X11/Xlib.h>
#    include <GL/glx.h>
#  endif

#ifdef __unix__
struct ObolGLXCtx {
    Display*    dpy;
    int         w, h;
    GLXContext  ctx;
    GLXPbuffer  pbuffer;
    GLXFBConfig fbconfig;
    bool        use_pbuffer;
    Pixmap      xpixmap;
    GLXPixmap   glxpixmap;
    XVisualInfo*vi;
    GLXContext  prev_ctx;
    GLXDrawable prev_draw, prev_read;
};

class ObolGLXContextManager : public SoDB::ContextManager {
public:
    ObolGLXContextManager() : m_dpy(nullptr) {}
    ~ObolGLXContextManager() { if (m_dpy) { XCloseDisplay(m_dpy); m_dpy = nullptr; } }

    void* createOffscreenContext(unsigned int w, unsigned int h) override {
        Display* dpy = getDisplay();
        if (!dpy) return nullptr;
        int scr = DefaultScreen(dpy);

        auto* ctx = new ObolGLXCtx;
        ctx->dpy        = dpy;
        ctx->w = (int)w; ctx->h = (int)h;
        ctx->ctx        = nullptr;
        ctx->pbuffer    = 0;
        ctx->use_pbuffer= false;
        ctx->xpixmap    = 0;
        ctx->glxpixmap  = 0;
        ctx->vi         = nullptr;
        ctx->prev_ctx   = nullptr;
        ctx->prev_draw  = ctx->prev_read = 0;

        /* Try pbuffer first */
        const char* env = getenv("COIN_GLXGLUE_NO_PBUFFERS");
        if (!env || env[0] == '0') {
            int fbatt[] = {
                GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,8, GLX_GREEN_SIZE,8, GLX_BLUE_SIZE,8,
                GLX_DEPTH_SIZE,16, GLX_DOUBLEBUFFER,False, None
            };
            int nfb = 0;
            GLXFBConfig* fbs = glXChooseFBConfig(dpy, scr, fbatt, &nfb);
            if (fbs && nfb > 0) {
                int pbatt[] = { GLX_PBUFFER_WIDTH,(int)w,
                                GLX_PBUFFER_HEIGHT,(int)h,
                                GLX_PRESERVED_CONTENTS,False, None };
                ctx->fbconfig = fbs[0];
                ctx->pbuffer  = glXCreatePbuffer(dpy, fbs[0], pbatt);
                if (ctx->pbuffer) {
                    ctx->ctx = glXCreateNewContext(dpy,fbs[0],GLX_RGBA_TYPE,nullptr,True);
                    if (ctx->ctx) { ctx->use_pbuffer=true; XFree(fbs); return ctx; }
                    glXDestroyPbuffer(dpy, ctx->pbuffer); ctx->pbuffer=0;
                }
                XFree(fbs);
            }
        }
        /* Pixmap fallback */
        int vatt[] = { GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,
                       GLX_DEPTH_SIZE,16,None };
        ctx->vi = glXChooseVisual(dpy, scr, vatt);
        if (!ctx->vi) { delete ctx; return nullptr; }
        ctx->xpixmap  = XCreatePixmap(dpy, RootWindow(dpy,scr), w, h, ctx->vi->depth);
        if (!ctx->xpixmap) { XFree(ctx->vi); delete ctx; return nullptr; }
        ctx->glxpixmap = glXCreateGLXPixmap(dpy, ctx->vi, ctx->xpixmap);
        ctx->ctx       = glXCreateContext(dpy, ctx->vi, nullptr, False);
        if (!ctx->ctx && !False)
            ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, True);
        if (!ctx->ctx || !ctx->glxpixmap) {
            if (ctx->glxpixmap) glXDestroyGLXPixmap(dpy, ctx->glxpixmap);
            if (ctx->xpixmap)   XFreePixmap(dpy, ctx->xpixmap);
            XFree(ctx->vi); delete ctx; return nullptr;
        }
        return ctx;
    }

    SbBool makeContextCurrent(void* c) override {
        auto* ctx = static_cast<ObolGLXCtx*>(c);
        if (!ctx || !ctx->ctx) return FALSE;
        ctx->prev_ctx  = glXGetCurrentContext();
        ctx->prev_draw = glXGetCurrentDrawable();
        ctx->prev_read = glXGetCurrentReadDrawable();
        Bool ok = ctx->use_pbuffer
            ? glXMakeCurrent(ctx->dpy, ctx->pbuffer,    ctx->ctx)
            : glXMakeCurrent(ctx->dpy, ctx->glxpixmap,  ctx->ctx);
        return ok ? TRUE : FALSE;
    }

    void restorePreviousContext(void* c) override {
        auto* ctx = static_cast<ObolGLXCtx*>(c);
        if (!ctx) return;
        if (ctx->prev_ctx) glXMakeCurrent(ctx->dpy, ctx->prev_draw, ctx->prev_ctx);
        else                glXMakeCurrent(ctx->dpy, None, nullptr);
    }

    void destroyContext(void* c) override {
        auto* ctx = static_cast<ObolGLXCtx*>(c);
        if (!ctx) return;
        glXMakeCurrent(ctx->dpy, None, nullptr);
        if (ctx->ctx) glXDestroyContext(ctx->dpy, ctx->ctx);
        if (ctx->use_pbuffer) {
            if (ctx->pbuffer) glXDestroyPbuffer(ctx->dpy, ctx->pbuffer);
        } else {
            if (ctx->glxpixmap) glXDestroyGLXPixmap(ctx->dpy, ctx->glxpixmap);
            if (ctx->xpixmap)   XFreePixmap(ctx->dpy, ctx->xpixmap);
            if (ctx->vi)        XFree(ctx->vi);
        }
        delete ctx;
    }

private:
    Display* m_dpy;
    Display* getDisplay() {
        if (!m_dpy) {
            m_dpy = XOpenDisplay(nullptr);
            if (!m_dpy)
                fprintf(stderr, "obol_bridge(sys): cannot open X display\n");
        }
        return m_dpy;
    }
};
#endif /* __unix__ */

static void bridge_init_coin() {
#ifdef __unix__
    XSetErrorHandler([](Display*, XErrorEvent* e)->int{
        fprintf(stderr,"obol_bridge(sys): X error %d (ignored)\n",e->error_code);
        return 0; });
    static ObolGLXContextManager mgr;
    SoDB::init(&mgr);
#else
    class StubMgr : public SoDB::ContextManager {
    public:
        void* createOffscreenContext(unsigned int,unsigned int) override{return nullptr;}
        SbBool makeContextCurrent(void*) override{return FALSE;}
        void restorePreviousContext(void*) override{}
        void destroyContext(void*) override{}
    };
    static StubMgr stub;
    SoDB::init(&stub);
#endif
    SoNodeKit::init();
    SoInteraction::init();
}
#endif /* COIN3D_OSMESA_BUILD */

/* =========================================================================
 * Scene factories
 * ======================================================================= */

typedef SoSeparator* (*SceneFactory)(int w, int h);

static SoSeparator* scene_primitives(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-0.5f,-0.8f,-0.6f); root->addChild(li);
    const float s = 2.5f;
    struct { float r,g,b,tx,ty; SoNode* shape; } prims[] = {
        {0.85f,0.15f,0.15f,-s*.5f, s*.5f,new SoSphere},
        {0.15f,0.75f,0.15f, s*.5f, s*.5f,new SoCube},
        {0.15f,0.35f,0.90f,-s*.5f,-s*.5f,new SoCone},
        {0.90f,0.75f,0.15f, s*.5f,-s*.5f,new SoCylinder},
    };
    for (auto& p : prims) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(p.tx,p.ty,0);
        sep->addChild(t);
        SoMaterial* m = new SoMaterial;
        m->diffuseColor.setValue(p.r,p.g,p.b);
        m->specularColor.setValue(0.6f,0.6f,0.6f);
        m->shininess.setValue(0.5f);
        sep->addChild(m); sep->addChild(p.shape); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    cam->position.setValue(cam->position.getValue()*1.1f);
    return root;
}

static SoSeparator* scene_materials(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    struct MatDesc { float dr,dg,db,sr,sg,sb,sh,tx; const char* label; };
    MatDesc mats[] = {
        {0.8f,0.1f,0.1f,0.9f,0.9f,0.9f,0.9f,-3.0f,"shiny red"},
        {0.1f,0.7f,0.1f,0.0f,0.0f,0.0f,0.0f,-1.0f,"matte green"},
        {0.1f,0.1f,0.8f,0.5f,0.5f,0.8f,0.5f, 1.0f,"blue"},
        {0.8f,0.7f,0.0f,1.0f,1.0f,0.8f,0.8f, 3.0f,"gold"},
    };
    for (auto& m : mats) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(m.tx,0,0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(m.dr,m.dg,m.db);
        mat->specularColor.setValue(m.sr,m.sg,m.sb);
        mat->shininess.setValue(m.sh);
        sep->addChild(mat); sep->addChild(new SoSphere); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_lighting(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(0,-1,-1); li->color.setValue(1,0.9f,0.8f);
    root->addChild(li);
    SoPointLight* pl = new SoPointLight;
    pl->location.setValue(-3,2,2); pl->color.setValue(0.2f,0.4f,1.0f);
    root->addChild(pl);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f,0.7f,0.7f);
    mat->specularColor.setValue(1,1,1); mat->shininess.setValue(0.7f);
    root->addChild(mat);
    root->addChild(new SoSphere);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_transforms(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.7f,0.9f);
    mat->specularColor.setValue(0.8f,0.8f,0.8f);
    mat->shininess.setValue(0.6f);
    root->addChild(mat);
    for (int i = 0; i < 5; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTransform* xf = new SoTransform;
        xf->translation.setValue((i-2)*2.0f, 0, 0);
        xf->rotation.setValue(SbVec3f(0,1,0), (float)i * 0.5f);
        xf->scaleFactor.setValue(0.6f+i*0.1f, 0.6f+i*0.1f, 0.6f+i*0.1f);
        sep->addChild(xf); sep->addChild(new SoCube);
        root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_colored_cube(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.85f,0.10f,0.10f);
    mat->specularColor.setValue(0.50f,0.50f,0.50f);
    mat->shininess.setValue(0.40f);
    root->addChild(mat); root->addChild(new SoCube);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_coordinates(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    /* Draw X, Y, Z axes as colour-coded line sets */
    static const float axverts[6][3] = {
        {0,0,0},{2,0,0}, {0,0,0},{0,2,0}, {0,0,0},{0,0,2}
    };
    static const float axcols[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    SoLightModel* lm = new SoLightModel;
    lm->model = SoLightModel::BASE_COLOR;
    root->addChild(lm);
    for (int a = 0; a < 3; ++a) {
        SoSeparator* sep = new SoSeparator;
        SoBaseColor* bc = new SoBaseColor;
        bc->rgb.setValue(axcols[a][0], axcols[a][1], axcols[a][2]);
        sep->addChild(bc);
        SoCoordinate3* co = new SoCoordinate3;
        co->point.setValues(0, 2, reinterpret_cast<const float(*)[3]>(axverts + a*2));
        sep->addChild(co);
        SoIndexedLineSet* ils = new SoIndexedLineSet;
        static const int32_t idx[] = {0,1,-1};
        ils->coordIndex.setValues(0, 3, idx);
        sep->addChild(ils);
        root->addChild(sep);
    }
    root->addChild(new SoSphere);  // origin marker
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_shadow(int w, int h) {
    SoSeparator* outer = new SoSeparator; outer->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->position.setValue(0, 8, 10);
    cam->pointAt(SbVec3f(0,0,0), SbVec3f(0,1,0));
    outer->addChild(cam);
    SoShadowGroup* sg = new SoShadowGroup;
    sg->isActive = TRUE;
    sg->intensity = 0.7f;
    SoShadowDirectionalLight* sdl = new SoShadowDirectionalLight;
    sdl->direction.setValue(-1,-2,-1);
    sg->addChild(sdl);
    SoMaterial* floor_mat = new SoMaterial;
    floor_mat->diffuseColor.setValue(0.7f,0.6f,0.5f);
    sg->addChild(floor_mat);
    SoTransform* floor_xf = new SoTransform;
    floor_xf->scaleFactor.setValue(8,0.1f,8);
    floor_xf->translation.setValue(0,-0.5f,0);
    sg->addChild(floor_xf);
    sg->addChild(new SoCube);
    SoMaterial* sphere_mat = new SoMaterial;
    sphere_mat->diffuseColor.setValue(0.2f,0.4f,0.9f);
    sg->addChild(sphere_mat);
    SoTranslation* sph_t = new SoTranslation;
    sph_t->translation.setValue(0,1,0);
    sg->addChild(sph_t);
    sg->addChild(new SoSphere);
    outer->addChild(sg);
    return outer;
}

static SoSeparator* scene_transparency(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    struct { float r,g,b,a,x; } spheres[] = {
        {0.8f,0.1f,0.1f,0.3f,-2.0f},
        {0.1f,0.8f,0.1f,0.5f, 0.0f},
        {0.1f,0.1f,0.8f,0.7f, 2.0f},
        {0.8f,0.8f,0.1f,0.9f, 4.0f},
    };
    for (auto& s : spheres) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(s.x,0,0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(s.r,s.g,s.b);
        mat->transparency.setValue(1.0f-s.a);
        sep->addChild(mat); sep->addChild(new SoSphere); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_lod(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f,0.6f,1.0f);
    mat->specularColor.setValue(0.9f,0.9f,0.9f);
    mat->shininess.setValue(0.8f);
    root->addChild(mat);
    /* Three spheres at different complexities showing LOD effect */
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue((i-1)*3.0f,0,0);
        sep->addChild(t);
        SoComplexity* cx = new SoComplexity;
        cx->value = 0.1f + 0.4f * i;
        sep->addChild(cx);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_drawstyle(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    const struct { SoDrawStyle::Style s; float tx; } styles[] = {
        {SoDrawStyle::FILLED, -3.0f},
        {SoDrawStyle::LINES,   0.0f},
        {SoDrawStyle::POINTS,  3.0f},
    };
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.7f,0.3f);
    root->addChild(mat);
    for (auto& st : styles) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(st.tx,0,0);
        sep->addChild(t);
        SoDrawStyle* ds = new SoDrawStyle; ds->style = st.s; sep->addChild(ds);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_indexed_face_set(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f,0.5f,0.1f);
    root->addChild(mat);
    /* A simple tetrahedron */
    static const float pts[4][3] = {
        {0,1,0},{-1,-1,1},{1,-1,1},{0,-1,-1}
    };
    static const int32_t idx[] = {
        0,1,2,-1,  0,2,3,-1,  0,3,1,-1,  1,3,2,-1
    };
    SoCoordinate3* co = new SoCoordinate3;
    co->point.setValues(0, 4, pts); root->addChild(co);
    SoShapeHints* sh = new SoShapeHints;
    sh->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    sh->shapeType = SoShapeHints::SOLID;
    root->addChild(sh);
    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 16, idx); root->addChild(ifs);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

/* =========================================================================
 * Scene catalogue table
 * ======================================================================= */
struct SceneEntry {
    const char* name;
    const char* category;
    const char* description;
    SceneFactory factory;
};

static const SceneEntry s_scenes[] = {
    {"primitives",       "Rendering",  "Basic primitives: sphere, cube, cone, cylinder", scene_primitives},
    {"materials",        "Rendering",  "Material property showcase",                      scene_materials},
    {"lighting",         "Rendering",  "Multiple light sources",                          scene_lighting},
    {"transforms",       "Rendering",  "Hierarchical transform chain",                    scene_transforms},
    {"colored_cube",     "Rendering",  "Simple red cube (smoke test)",                    scene_colored_cube},
    {"coordinates",      "Rendering",  "XYZ coordinate axis visualization",               scene_coordinates},
    {"shadow",           "Rendering",  "SoShadowGroup shadow casting",                    scene_shadow},
    {"transparency",     "Rendering",  "Alpha-blended transparent spheres",               scene_transparency},
    {"lod",              "Rendering",  "Level-of-detail SoComplexity comparison",         scene_lod},
    {"drawstyle",        "Rendering",  "Filled / wireframe / points draw styles",         scene_drawstyle},
    {"indexed_face_set", "Rendering",  "SoIndexedFaceSet tetrahedron",                    scene_indexed_face_set},
};
static const int s_scene_count = (int)(sizeof(s_scenes)/sizeof(s_scenes[0]));

/* =========================================================================
 * ObolBridgeScene – per-instance state
 * ======================================================================= */
struct ObolBridgeScene {
    const SceneEntry* entry  = nullptr;
    SoSeparator*      root   = nullptr;
    SoPerspectiveCamera* cam = nullptr;
    int width  = 800;
    int height = 600;
    /* drag state for orbit camera */
    bool   dragging   = false;
    int    drag_btn   = 0;
    int    last_x     = 0;
    int    last_y     = 0;
    bool   initialized = false;
};

/* Shared offscreen renderer (created on first use) */
static SoOffscreenRenderer* s_renderer = nullptr;
static SoOffscreenRenderer* getRenderer(int w, int h) {
    if (!s_renderer) {
        SbViewportRegion vp(w,h);
        s_renderer = new SoOffscreenRenderer(vp);
    }
    return s_renderer;
}

/* Find or create camera in scene */
static SoPerspectiveCamera* findOrCreateCamera(SoSeparator* root) {
    SoSearchAction sa;
    sa.setType(SoPerspectiveCamera::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    if (sa.getPath())
        return static_cast<SoPerspectiveCamera*>(sa.getPath()->getTail());
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->insertChild(cam, 0);
    SbViewportRegion vp(800,600); cam->viewAll(root, vp);
    return cam;
}

/* =========================================================================
 * Exported C bridge functions
 *
 * OBOL_EXPORT marks each function visible despite -fvisibility=hidden so
 * that dlsym() can locate them.  The GNU version script (obol_bridge.map)
 * then restricts the *dynamic* symbol table to only these symbols.
 * ======================================================================= */

#if defined(__GNUC__) || defined(__clang__)
#  define OBOL_EXPORT __attribute__((visibility("default")))
#else
#  define OBOL_EXPORT
#endif

extern "C" {

OBOL_EXPORT int obol_init(void) {
    static bool done = false;
    if (done) return 0;
    done = true;
    bridge_init_coin();
    return 0;
}

OBOL_EXPORT const char* obol_backend_name(void) {
#ifdef COIN3D_OSMESA_BUILD
    return "OSMesa (headless)";
#else
    return "OpenGL/GLX (system)";
#endif
}

OBOL_EXPORT int obol_scene_count(void) { return s_scene_count; }

OBOL_EXPORT const char* obol_scene_name(int i) {
    if (i < 0 || i >= s_scene_count) return "";
    return s_scenes[i].name;
}

OBOL_EXPORT const char* obol_scene_category(int i) {
    if (i < 0 || i >= s_scene_count) return "";
    return s_scenes[i].category;
}

OBOL_EXPORT const char* obol_scene_description(int i) {
    if (i < 0 || i >= s_scene_count) return "";
    return s_scenes[i].description;
}

OBOL_EXPORT int obol_scene_find(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < s_scene_count; ++i)
        if (strcmp(s_scenes[i].name, name) == 0) return i;
    return -1;
}

OBOL_EXPORT ObolScene obol_scene_create(const char* name, int width, int height) {
    int idx = obol_scene_find(name);
    if (idx < 0) {
        fprintf(stderr, "obol_bridge: unknown scene '%s'\n", name ? name : "(null)");
        return nullptr;
    }
    SoSeparator* root = s_scenes[idx].factory(width, height);
    if (!root) return nullptr;
    ObolBridgeScene* s = new ObolBridgeScene;
    s->entry  = &s_scenes[idx];
    s->root   = root;
    s->cam    = findOrCreateCamera(root);
    s->width  = width;
    s->height = height;
    s->initialized = true;
    return s;
}

OBOL_EXPORT void obol_scene_destroy(ObolScene scene) {
    if (!scene) return;
    if (scene->root) scene->root->unref();
    delete scene;
}

OBOL_EXPORT int obol_scene_render(ObolScene scene, uint8_t* rgba_buf, int width, int height) {
    if (!scene || !rgba_buf) return 1;
    SoOffscreenRenderer* r = getRenderer(width, height);
    SbViewportRegion vp(width, height);
    r->setViewportRegion(vp);
    r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
    r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
    if (!r->render(scene->root)) return 2;
    /* Copy pixels: SoOffscreenRenderer uses RGBA bottom-up */
    const unsigned char* src = r->getBuffer();
    if (!src) return 3;
    memcpy(rgba_buf, src, (size_t)width * height * 4);
    return 0;
}

OBOL_EXPORT void obol_scene_resize(ObolScene scene, int width, int height) {
    if (!scene) return;
    scene->width  = width;
    scene->height = height;
}

OBOL_EXPORT void obol_scene_mouse_press(ObolScene scene, int x, int y, int button) {
    if (!scene || !scene->root) return;
    scene->dragging = true;
    scene->drag_btn = button;
    scene->last_x   = x;
    scene->last_y   = y;
    /* Also dispatch to scene graph for dragger interaction */
    SoMouseButtonEvent ev;
    ev.setButton(button == 1 ? SoMouseButtonEvent::BUTTON1 :
                 button == 2 ? SoMouseButtonEvent::BUTTON2 :
                               SoMouseButtonEvent::BUTTON3);
    ev.setState(SoButtonEvent::DOWN);
    ev.setPosition(SbVec2s((short)x, (short)(scene->height - y)));
    ev.setTime(SbTime::getTimeOfDay());
    SbViewportRegion vp(scene->width, scene->height);
    SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(scene->root);
}

OBOL_EXPORT void obol_scene_mouse_release(ObolScene scene, int x, int y, int button) {
    if (!scene) return;
    scene->dragging = false;
    if (!scene->root) return;
    SoMouseButtonEvent ev;
    ev.setButton(button == 1 ? SoMouseButtonEvent::BUTTON1 :
                 button == 2 ? SoMouseButtonEvent::BUTTON2 :
                               SoMouseButtonEvent::BUTTON3);
    ev.setState(SoButtonEvent::UP);
    ev.setPosition(SbVec2s((short)x, (short)(scene->height - y)));
    ev.setTime(SbTime::getTimeOfDay());
    SbViewportRegion vp(scene->width, scene->height);
    SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(scene->root);
}

OBOL_EXPORT void obol_scene_mouse_move(ObolScene scene, int x, int y) {
    if (!scene || !scene->root) return;
    int dx = x - scene->last_x;
    int dy = y - scene->last_y;
    scene->last_x = x; scene->last_y = y;
    if (scene->dragging && scene->cam) {
        if (scene->drag_btn == 1) {
            /* Left drag → orbit */
            float az = -(float)dx * 0.01f;
            float el =  (float)dy * 0.01f;
            SbVec3f pos = scene->cam->position.getValue();
            SbVec3f center(0,0,0);
            SbVec3f offset = pos - center;
            SbRotation azRot(SbVec3f(0,1,0), az);
            azRot.multVec(offset, offset);
            SbVec3f viewDir = -offset; viewDir.normalize();
            SbVec3f up(0,1,0);
            SbVec3f right = up.cross(viewDir);
            float rLen = right.length();
            if (rLen > 1e-4f) right *= 1.0f/rLen;
            else               right = SbVec3f(1,0,0);
            SbRotation elRot(right, el);
            elRot.multVec(offset, offset);
            scene->cam->position.setValue(center + offset);
            scene->cam->pointAt(center, SbVec3f(0,1,0));
        } else if (scene->drag_btn == 3) {
            /* Right drag → dolly */
            float dist = scene->cam->focalDistance.getValue();
            dist *= (1.0f + dy * 0.01f);
            if (dist < 0.1f) dist = 0.1f;
            SbVec3f dir = scene->cam->position.getValue();
            dir.normalize();
            scene->cam->position.setValue(dir * dist);
            scene->cam->focalDistance.setValue(dist);
        }
    }
    /* Forward motion event to scene graph */
    SoLocation2Event ev;
    ev.setPosition(SbVec2s((short)x, (short)(scene->height - y)));
    ev.setTime(SbTime::getTimeOfDay());
    SbViewportRegion vp(scene->width, scene->height);
    SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(scene->root);
}

OBOL_EXPORT void obol_scene_scroll(ObolScene scene, float delta) {
    if (!scene || !scene->cam) return;
    float dist = scene->cam->focalDistance.getValue();
    dist *= (1.0f - delta * 0.1f);
    if (dist < 0.1f) dist = 0.1f;
    SbVec3f dir = scene->cam->position.getValue();
    dir.normalize();
    scene->cam->position.setValue(dir * dist);
    scene->cam->focalDistance.setValue(dist);
}

OBOL_EXPORT void obol_scene_key_press(ObolScene scene, int key) {
    if (!scene || !scene->root) return;
    SoKeyboardEvent ev;
    ev.setKey((SoKeyboardEvent::Key)key);
    ev.setState(SoButtonEvent::DOWN);
    ev.setTime(SbTime::getTimeOfDay());
    SbViewportRegion vp(scene->width, scene->height);
    SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(scene->root);
}

OBOL_EXPORT void obol_scene_key_release(ObolScene scene, int key) {
    if (!scene || !scene->root) return;
    SoKeyboardEvent ev;
    ev.setKey((SoKeyboardEvent::Key)key);
    ev.setState(SoButtonEvent::UP);
    ev.setTime(SbTime::getTimeOfDay());
    SbViewportRegion vp(scene->width, scene->height);
    SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(scene->root);
}

OBOL_EXPORT void obol_scene_get_camera(ObolScene scene, float* pos3, float* orient4, float* dist) {
    if (!scene || !scene->cam) {
        if (pos3)    { pos3[0]=0; pos3[1]=0; pos3[2]=10; }
        if (orient4) { orient4[0]=0; orient4[1]=0; orient4[2]=0; orient4[3]=1; }
        if (dist)    { *dist = 10.0f; }
        return;
    }
    SbVec3f p = scene->cam->position.getValue();
    if (pos3) { pos3[0]=p[0]; pos3[1]=p[1]; pos3[2]=p[2]; }
    SbRotation rot = scene->cam->orientation.getValue();
    SbVec3f axis; float angle;
    rot.getValue(axis, angle);
    /* Store as quaternion */
    SbVec4f q;
    float sinA = sinf(angle*0.5f);
    q[0] = axis[0]*sinA; q[1] = axis[1]*sinA; q[2] = axis[2]*sinA; q[3] = cosf(angle*0.5f);
    if (orient4) { orient4[0]=q[0]; orient4[1]=q[1]; orient4[2]=q[2]; orient4[3]=q[3]; }
    if (dist) *dist = scene->cam->focalDistance.getValue();
}

OBOL_EXPORT void obol_scene_set_camera(ObolScene scene, const float* pos3, const float* orient4, float dist) {
    if (!scene || !scene->cam) return;
    if (pos3)
        scene->cam->position.setValue(pos3[0], pos3[1], pos3[2]);
    if (orient4) {
        float qx=orient4[0], qy=orient4[1], qz=orient4[2], qw=orient4[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f * acosf(qw);
        SbVec3f axis(qx,qy,qz);
        float axLen = axis.length();
        if (axLen < 1e-6f) { axis = SbVec3f(0,1,0); angle = 0; }
        else axis.normalize();
        scene->cam->orientation.setValue(SbRotation(axis, angle));
    }
    if (dist > 0.0f) scene->cam->focalDistance.setValue(dist);
}

} /* extern "C" */
