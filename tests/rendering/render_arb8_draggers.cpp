/*
 * render_arb8_draggers.cpp
 *
 * Visual regression / smoke test for SoProceduralShape with
 * buildHandleDraggers().  Verifies that all three handle types
 * (vertex DRAG_POINT, edge DRAG_ON_PLANE, face DRAG_ALONG_AXIS) can be
 * created and rendered without crashing.
 *
 * Output: argv[1]+".rgb"  (800×600 SGI RGB)
 */

#include "headless_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

// ---- ARB8 geometry constants ----

static const int kFaces[6][4] = {
  {0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}
};
static const int kEdges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};
static const float kDefault[24] = {
  -1,-1,-1, 1,-1,-1, 1,-1,1,-1,-1,1,
  -1, 1,-1, 1, 1,-1, 1, 1,1,-1, 1,1
};

static inline SbVec3f vtx(const float* p,int i){return SbVec3f(p[3*i],p[3*i+1],p[3*i+2]);}

static SbVec3f faceNrm(const float* p,const int fv[4])
{
  SbVec3f n=(vtx(p,fv[1])-vtx(p,fv[0])).cross(vtx(p,fv[2])-vtx(p,fv[0]));
  n.normalize(); return n;
}
static SbVec3f faceCtr(const float* p,const int fv[4])
{
  return (vtx(p,fv[0])+vtx(p,fv[1])+vtx(p,fv[2])+vtx(p,fv[3]))*0.25f;
}

static void arb8_bbox_r(const float* p,int n,SbVec3f& mn,SbVec3f& mx,void*)
{
  if(n<24){mn.setValue(-1,-1,-1);mx.setValue(1,1,1);return;}
  float ax=p[0],ay=p[1],az=p[2],bx=p[0],by=p[1],bz=p[2];
  for(int i=1;i<8;++i){
    if(p[3*i  ]<ax)ax=p[3*i  ];if(p[3*i+1]<ay)ay=p[3*i+1];if(p[3*i+2]<az)az=p[3*i+2];
    if(p[3*i  ]>bx)bx=p[3*i  ];if(p[3*i+1]>by)by=p[3*i+1];if(p[3*i+2]>bz)bz=p[3*i+2];
  }
  mn.setValue(ax,ay,az); mx.setValue(bx,by,bz);
}

static void arb8_geom_r(const float* pp,int n,
                         SoProceduralTriangles* tris,
                         SoProceduralWireframe* wire,void*)
{
  const float* p=(n>=24)?pp:kDefault;
  if(tris){
    tris->vertices.clear();tris->normals.clear();tris->indices.clear();
    for(int f=0;f<6;++f){
      const int* fv=kFaces[f];
      SbVec3f v0=vtx(p,fv[0]),v1=vtx(p,fv[1]),v2=vtx(p,fv[2]),v3=vtx(p,fv[3]);
      SbVec3f nm=(v1-v0).cross(v2-v0);nm.normalize();
      int b=(int)tris->vertices.size();
      tris->vertices.insert(tris->vertices.end(),{v0,v1,v2,v3});
      tris->normals .insert(tris->normals .end(),{nm,nm,nm,nm});
      tris->indices .insert(tris->indices.end(),{b,b+1,b+2,b,b+2,b+3});
    }
  }
  if(wire){
    wire->vertices.clear();wire->segments.clear();
    for(int i=0;i<8;++i)wire->vertices.push_back(vtx(p,i));
    for(int e=0;e<12;++e){wire->segments.push_back(kEdges[e][0]);wire->segments.push_back(kEdges[e][1]);}
  }
}

static void arb8_handles_r(const float* pp,int n,
                             std::vector<SoProceduralHandle>& handles,void*)
{
  const float* p=(n>=24)?pp:kDefault;
  handles.clear();
  for(int i=0;i<8;++i){
    SoProceduralHandle h; h.position=vtx(p,i);
    h.dragType=SoProceduralHandle::DRAG_POINT; handles.push_back(h);
  }
  for(int e=0;e<12;++e){
    SbVec3f a=vtx(p,kEdges[e][0]),b=vtx(p,kEdges[e][1]);
    SoProceduralHandle h; h.position=(a+b)*0.5f;
    h.dragType=SoProceduralHandle::DRAG_ON_PLANE;
    h.normal=SbVec3f(0,1,0);
    for(int f=0;f<6;++f){
      const int* fv=kFaces[f];
      for(int k=0;k<4;++k)
        if(fv[k]==kEdges[e][0]&&fv[(k+1)%4]==kEdges[e][1]){h.normal=faceNrm(p,fv);goto done_e;}
    }
    done_e: handles.push_back(h);
  }
  for(int f=0;f<6;++f){
    SoProceduralHandle h; h.position=faceCtr(p,kFaces[f]);
    h.dragType=SoProceduralHandle::DRAG_ALONG_AXIS; h.axis=faceNrm(p,kFaces[f]);
    handles.push_back(h);
  }
}

static void arb8_handleDrag_r(float* params,int n,int idx,
                                const SbVec3f& oldP,const SbVec3f& newP,void*)
{
  if(n<24)return;
  SbVec3f d=newP-oldP;
  if(idx<8){
    params[3*idx  ]+=d[0];params[3*idx+1]+=d[1];params[3*idx+2]+=d[2];
  }else if(idx<20){
    int e=idx-8;
    for(int k=0;k<2;++k){int v=kEdges[e][k];params[3*v]+=d[0];params[3*v+1]+=d[1];params[3*v+2]+=d[2];}
  }else if(idx<26){
    int f=idx-20;
    for(int k=0;k<4;++k){int v=kFaces[f][k];params[3*v]+=d[0];params[3*v+1]+=d[1];params[3*v+2]+=d[2];}
  }
}

int main(int argc, char** argv)
{
  initCoinHeadless();

  SoProceduralShape::registerShapeType(
      "ARB8_render", "",
      arb8_bbox_r, arb8_geom_r,
      arb8_handles_r, arb8_handleDrag_r);

  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f,-0.8f,-0.6f);
  root->addChild(light);

  // Slightly sheared box
  float sheared[24];
  memcpy(sheared, kDefault, sizeof(sheared));
  for(int i=4;i<8;++i) sheared[3*i] += 0.3f;

  // Left: solid box
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(-2.5f,0,0); sep->addChild(t);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.3f,0.5f,0.9f);
    m->specularColor.setValue(0.4f,0.4f,0.4f); m->shininess.setValue(0.5f);
    sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,kDefault);
    sep->addChild(s);
    root->addChild(sep);
  }

  // Centre: sheared box + vertex/edge/face dragger handles
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(0.f,0,0); sep->addChild(t);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.9f,0.5f,0.2f);
    m->specularColor.setValue(0.4f,0.4f,0.4f); m->shininess.setValue(0.5f);
    sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,sheared);
    sep->addChild(s);

    // Add handle draggers — the core feature under test
    SoSeparator* handles = s->buildHandleDraggers();
    if(handles) sep->addChild(handles);

    root->addChild(sep);
  }

  // Right: wireframe
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(2.5f,0,0); sep->addChild(t);
    SoDrawStyle* ds=new SoDrawStyle; ds->style.setValue(SoDrawStyle::LINES); sep->addChild(ds);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.1f,0.9f,0.3f); sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,sheared);
    sep->addChild(s);
    root->addChild(sep);
  }

  SbViewportRegion vp(DEFAULT_WIDTH,DEFAULT_HEIGHT);
  cam->viewAll(root,vp);
  cam->position.setValue(cam->position.getValue()*1.3f);

  char outpath[1024];
  if(argc>1) snprintf(outpath,sizeof(outpath),"%s.rgb",argv[1]);
  else       snprintf(outpath,sizeof(outpath),"render_arb8_draggers.rgb");

  bool ok=renderToFile(root,outpath);
  root->unref();
  return ok?0:1;
}
