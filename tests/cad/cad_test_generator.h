/*
 * cad_test_generator.h
 *
 * Deterministic CAD assembly generator for regression tests and benchmarks.
 *
 * Overview
 * --------
 * Produces N unique part geometries and M instances in a 2-level hierarchy:
 *
 *   Root
 *   ├── group_0000  (internal node; sets world-space origin for its leaves)
 *   │   ├── leaf_000000  (icosphere variant, world transform = group + leaf offset)
 *   │   └── leaf_000001
 *   ├── group_0001
 *   │   └── ...
 *   └── ...
 *
 * Instance IDs are built with CadIdBuilder::extendNameOccBool, giving stable
 * hierarchical IDs that can be used to test setHiddenInstances,
 * updateInstanceTransform, updateInstanceStyle, and pick operations.
 *
 * Part geometry
 * -------------
 * Part index modulo 4 controls icosphere complexity:
 *   0 → 20 triangles   (level 0)
 *   1 → 80 triangles   (level 1)
 *   2 → 320 triangles  (level 2)
 *   3 → 1 280 triangles (level 3)
 * Odd-indexed parts additionally include a box wireframe (12 edges) so that
 * WIREFRAME draw mode is exercised for those parts.
 * Multiples of 8 are wire-only (no shaded mesh); this exercises WIREFRAME mode
 * for parts that genuinely have no triangle data.
 *
 * Scene-graph baseline
 * --------------------
 * buildFlatSG() constructs an equivalent flat SoSeparator tree: one
 * SoSeparator{SoTransform + shared geometry} per instance.  Geometry is shared
 * per unique part (Inventor's multiple-parent mechanism), matching the
 * SoCADAssembly model where each unique part is uploaded once and referenced
 * many times.
 *
 * Thread safety: all functions are stateless; objects are caller-owned.
 */

#pragma once

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/CadIds.h>

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoBaseColor.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace CadTestGen {

// ============================================================================
// Seeded LCG (portable, fully deterministic across platforms)
// ============================================================================

/**
 * Minimal linear-congruential generator seeded at construction.
 * Suitable for procedural geometry offsets; NOT intended for cryptography.
 */
struct LCG {
    uint32_t state;
    explicit LCG(uint32_t seed) : state(seed | 1u) {} // force odd seed (prevents cycle length collapse in Park-Miller LCG)
    uint32_t next() { state = 1664525u * state + 1013904223u; return state; }
    float nextFloat()              { return float(next() >> 8) * (1.0f / 16777216.0f); }
    float nextRange(float lo, float hi) { return lo + nextFloat() * (hi - lo); }
};

// ============================================================================
// Icosphere mesh generation
// ============================================================================

static void normalizeVec(SbVec3f& v)
{
    float len = v.length();
    if (len > 1e-7f) v /= len;
}

/**
 * Build a unit icosphere at the given subdivision level.
 *   level 0 →   20 triangles (icosahedron)
 *   level 1 →   80 triangles
 *   level 2 →  320 triangles
 *   level 3 → 1280 triangles
 *
 * Normals equal positions (unit sphere property).
 */
static obol::TriMesh buildIcosphereMesh(int level)
{
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<SbVec3f> pts = {
        SbVec3f(-1, t, 0), SbVec3f( 1, t, 0), SbVec3f(-1,-t, 0), SbVec3f( 1,-t, 0),
        SbVec3f( 0,-1, t), SbVec3f( 0, 1, t), SbVec3f( 0,-1,-t), SbVec3f( 0, 1,-t),
        SbVec3f( t, 0,-1), SbVec3f( t, 0, 1), SbVec3f(-t, 0,-1), SbVec3f(-t, 0, 1)
    };
    for (auto& v : pts) normalizeVec(v);

    std::vector<uint32_t> idx = {
         0,11,5,  0,5,1,  0,1,7,  0,7,10, 0,10,11,
         1,5,9,   5,11,4, 11,10,2, 10,7,6, 7,1,8,
         3,9,4,   3,4,2,  3,2,6,   3,6,8,  3,8,9,
         4,9,5,   2,4,11, 6,2,10,  8,6,7,  9,8,1
    };

    for (int s = 0; s < level; ++s) {
        std::unordered_map<uint64_t, uint32_t> edgeCache;
        auto getMid = [&](uint32_t a, uint32_t b) -> uint32_t {
            uint64_t key = (uint64_t(std::min(a, b)) << 32) |
                            uint64_t(std::max(a, b));
            auto it = edgeCache.find(key);
            if (it != edgeCache.end()) return it->second;
            SbVec3f m = (pts[a] + pts[b]) * 0.5f;
            normalizeVec(m);
            uint32_t mid = uint32_t(pts.size());
            pts.push_back(m);
            return edgeCache[key] = mid;
        };
        std::vector<uint32_t> newIdx;
        newIdx.reserve(idx.size() * 4);
        for (size_t i = 0; i < idx.size(); i += 3) {
            uint32_t v0 = idx[i], v1 = idx[i+1], v2 = idx[i+2];
            uint32_t a = getMid(v0,v1), b = getMid(v1,v2), c = getMid(v2,v0);
            newIdx.insert(newIdx.end(), {v0,a,c, v1,b,a, v2,c,b, a,b,c});
        }
        idx = std::move(newIdx);
    }

    obol::TriMesh mesh;
    mesh.positions = pts;
    mesh.normals   = pts; // unit sphere: normal == normalised position
    mesh.indices   = idx;
    mesh.bounds.setBounds(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
    return mesh;
}

// ============================================================================
// Box wireframe  (12 edges of the unit cube [0,1]^3)
// ============================================================================

static obol::WireRep buildBoxWireRep()
{
    static const float E[12][2][3] = {
        {{0,0,0},{1,0,0}},{{1,0,0},{1,1,0}},{{1,1,0},{0,1,0}},{{0,1,0},{0,0,0}},
        {{0,0,1},{1,0,1}},{{1,0,1},{1,1,1}},{{1,1,1},{0,1,1}},{{0,1,1},{0,0,1}},
        {{0,0,0},{0,0,1}},{{1,0,0},{1,0,1}},{{1,1,0},{1,1,1}},{{0,1,0},{0,1,1}}
    };
    obol::WireRep wire;
    for (int e = 0; e < 12; ++e) {
        obol::WirePolyline poly;
        poly.points = {
            SbVec3f(E[e][0][0], E[e][0][1], E[e][0][2]),
            SbVec3f(E[e][1][0], E[e][1][1], E[e][1][2])
        };
        poly.edgeId = uint32_t(e + 1);
        wire.polylines.push_back(std::move(poly));
    }
    wire.bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,1));
    return wire;
}

/**
 * Build varied geometry for part index `partIdx`.
 *
 *  partIdx % 4  → icosphere subdivision level (complexity)
 *  partIdx % 8 == 0  → wire-only (no shaded mesh; exercises WIREFRAME mode
 *                       for parts that genuinely lack triangle data)
 *  odd partIdx  → shaded icosphere + box wireframe overlay
 *  even partIdx → shaded icosphere only
 */
static obol::PartGeometry buildVariedPartGeometry(int partIdx)
{
    obol::PartGeometry geom;
    if (partIdx % 8 == 0) {
        geom.wire   = buildBoxWireRep();
    } else {
        int subLevel = partIdx % 4;
        geom.shaded  = buildIcosphereMesh(subLevel);
        if (partIdx % 2 == 1)
            geom.wire = buildBoxWireRep();
    }
    return geom;
}

// ============================================================================
// Configuration and data types
// ============================================================================

/** Configuration for the deterministic assembly generator. */
struct HierGenConfig {
    uint32_t seed           = 42;
    int      uniqueParts    = 50;   ///< number of distinct part geometries
    int      totalInstances = 500;  ///< total leaf instances
    int      branchFactor   = 10;   ///< max leaves per group (level-1 node)
    float    spacing        = 3.0f; ///< world-space grid spacing (scene units)
};

/** A generated part (geometry + stable ID). */
struct PartSpec {
    obol::PartId       id;
    std::string        name;
    obol::PartGeometry geometry;
    int                subdivLevel; ///< icosphere level used (0–3, or -1 for wire-only)
};

/** A generated group (level-1 internal node; no geometry, just an origin). */
struct GroupSpec {
    obol::InstanceId  id;
    std::string       name;
    SbVec3f           worldOrigin; ///< coarse grid position for this group
};

/** A generated leaf instance (part + world transform + hierarchy identity). */
struct InstSpec {
    obol::InstanceId  id;
    obol::InstanceId  parentGroupId;
    obol::PartId      partId;
    int               partIndex;      ///< index into Assembly::parts
    SbVec3f           worldPos;       ///< world-space position (pure translation)
    std::string       name;
    int               occurrenceIdx;  ///< sibling index within group (for ID)
};

/** Complete generated assembly. */
struct Assembly {
    std::vector<PartSpec>   parts;
    std::vector<GroupSpec>  groups;    ///< level-1 internal nodes
    std::vector<InstSpec>   instances; ///< leaf nodes (the actual parts)
    SbBox3f                 bounds;
    HierGenConfig           cfg;
};

// ============================================================================
// Generator
// ============================================================================

/**
 * Generate a deterministic assembly from `cfg`.
 *
 * Layout
 * ------
 * Groups are placed on a 2-D coarse grid.
 * Within each group, up to branchFactor leaves are placed on a 3-D fine grid
 * (3 × 3 × ceil(branchFactor/9) cells).
 * Parts are assigned by cycling: instance i → part (i % uniqueParts).
 */
static Assembly generate(const HierGenConfig& cfg)
{
    Assembly gen;
    gen.cfg = cfg;

    // ---- Parts ----
    gen.parts.reserve(size_t(cfg.uniqueParts));
    for (int p = 0; p < cfg.uniqueParts; ++p) {
        char buf[64];
        snprintf(buf, sizeof(buf), "part_%04d", p);
        PartSpec ps;
        ps.name       = buf;
        ps.id         = obol::CadIdBuilder::hash128(std::string(buf));
        ps.geometry   = buildVariedPartGeometry(p);
        ps.subdivLevel= (p % 8 == 0) ? -1 : (p % 4);
        gen.parts.push_back(std::move(ps));
    }

    // ---- Groups ----
    int nGroups = (cfg.totalInstances + cfg.branchFactor - 1) / cfg.branchFactor;
    int groupGridW = std::max(1, (int)std::ceil(std::sqrt(double(nGroups))));
    // Space groups far enough apart that leaves don't overlap between groups
    float groupSpacing = cfg.spacing * float(std::max(3, cfg.branchFactor / 3 + 2));

    obol::InstanceId rootId = obol::CadIdBuilder::Root();
    gen.groups.reserve(size_t(nGroups));
    for (int g = 0; g < nGroups; ++g) {
        GroupSpec gs;
        char buf[64];
        snprintf(buf, sizeof(buf), "group_%04d", g);
        gs.name        = buf;
        gs.id          = obol::CadIdBuilder::extendNameOccBool(
                             rootId, std::string(buf), uint32_t(g), 0);
        gs.worldOrigin = SbVec3f(float(g % groupGridW) * groupSpacing,
                                  float(g / groupGridW) * groupSpacing, 0.0f);
        gen.groups.push_back(std::move(gs));
    }

    // ---- Instances ----
    gen.instances.reserve(size_t(cfg.totalInstances));
    for (int i = 0; i < cfg.totalInstances; ++i) {
        int gIdx = i / cfg.branchFactor;
        int lIdx = i % cfg.branchFactor;
        const GroupSpec& grp = gen.groups[size_t(gIdx)];

        // Fine-grid leaf position within the group
        SbVec3f leafOff(float(lIdx % 3)        * cfg.spacing,
                         float((lIdx / 3) % 3)  * cfg.spacing,
                         float(lIdx / 9)         * cfg.spacing);

        InstSpec is;
        char buf[64];
        snprintf(buf, sizeof(buf), "leaf_%06d", i);
        is.name          = buf;
        is.partIndex     = i % cfg.uniqueParts;
        is.partId        = gen.parts[size_t(is.partIndex)].id;
        is.parentGroupId = grp.id;
        is.worldPos      = grp.worldOrigin + leafOff;
        is.occurrenceIdx = lIdx;
        is.id = obol::CadIdBuilder::extendNameOccBool(
                    grp.id, std::string(buf), uint32_t(lIdx), 0);
        gen.instances.push_back(std::move(is));
    }

    // ---- Bounds ----
    gen.bounds.makeEmpty();
    for (const auto& is : gen.instances)
        gen.bounds.extendBy(is.worldPos);

    return gen;
}

// ============================================================================
// SoCADAssembly population helper
// ============================================================================

/**
 * Upload all parts and up to `maxInstances` instances from `gen` into `asm_`.
 * Uses beginUpdate()/endUpdate() so geometry is compiled in one pass.
 *
 * @param maxInstances  Number of instances to upload; -1 means all.
 */
static void populateCAD(SoCADAssembly* asm_, const Assembly& gen,
                         int maxInstances = -1)
{
    asm_->beginUpdate();

    for (const auto& ps : gen.parts)
        asm_->upsertPart(ps.id, ps.geometry);

    int limit = (maxInstances >= 0)
                ? std::min(maxInstances, int(gen.instances.size()))
                : int(gen.instances.size());

    for (int i = 0; i < limit; ++i) {
        const InstSpec& is = gen.instances[size_t(i)];
        obol::InstanceRecord rec;
        rec.part            = is.partId;
        rec.parent          = is.parentGroupId;
        rec.childName       = is.name;
        rec.occurrenceIndex = uint32_t(is.occurrenceIdx);
        rec.boolOp          = 0;
        rec.localToRoot.makeIdentity();
        rec.localToRoot.setTranslate(is.worldPos);
        rec.style.hasColorOverride = true;
        rec.style.color = SbColor4f(0.7f, 0.75f, 0.8f, 1.0f);
        asm_->upsertInstance(is.id, rec);
    }

    asm_->endUpdate();
}

// ============================================================================
// SoSeparator flat-scene builder
// ============================================================================

/**
 * Build a geometry-only SoSeparator for one part.
 *  - Shaded mesh → SoCoordinate3 + SoIndexedFaceSet
 *  - Wire-only   → SoCoordinate3 + SoLineSet
 *  - Both        → SoCoordinate3 + SoIndexedFaceSet  (wire overlay omitted
 *                  in the SG baseline to keep the comparison fair: the SG
 *                  does not duplicate the edge pass that SoCADAssembly does
 *                  internally in SHADED_WITH_EDGES mode)
 */
static SoSeparator* buildGeomNodeSG(const obol::PartGeometry& geom)
{
    SoSeparator* sep = new SoSeparator;

    if (geom.shaded.has_value()) {
        const obol::TriMesh& mesh = *geom.shaded;
        SoCoordinate3* coords = new SoCoordinate3;
        for (int i = 0; i < int(mesh.positions.size()); ++i)
            coords->point.set1Value(i, mesh.positions[size_t(i)]);
        sep->addChild(coords);

        SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
        int fi = 0;
        for (size_t t = 0; t < mesh.indices.size(); t += 3) {
            ifs->coordIndex.set1Value(fi++, int(mesh.indices[t]));
            ifs->coordIndex.set1Value(fi++, int(mesh.indices[t+1]));
            ifs->coordIndex.set1Value(fi++, int(mesh.indices[t+2]));
            ifs->coordIndex.set1Value(fi++, SO_END_FACE_INDEX);
        }
        sep->addChild(ifs);

    } else if (geom.wire.has_value()) {
        const obol::WireRep& wire = *geom.wire;
        std::vector<SbVec3f> pts;
        std::vector<int>     nv;
        for (const auto& poly : wire.polylines) {
            for (const auto& p : poly.points) pts.push_back(p);
            nv.push_back(int(poly.points.size()));
        }
        SoCoordinate3* coords = new SoCoordinate3;
        for (int i = 0; i < int(pts.size()); ++i)
            coords->point.set1Value(i, pts[size_t(i)]);
        sep->addChild(coords);

        SoLineSet* ls = new SoLineSet;
        for (int j = 0; j < int(nv.size()); ++j)
            ls->numVertices.set1Value(j, nv[size_t(j)]);
        sep->addChild(ls);
    }

    return sep;
}

/**
 * Build a flat SoSeparator scene graph from `gen` (camera NOT included).
 *
 * Structure:
 *   SoSeparator root                     ← returned (ref-count = 1)
 *     SoLightModel(BASE_COLOR)
 *     SoBaseColor(0.7, 0.75, 0.8)
 *     SoSeparator inst_0                 ← one per instance
 *       SoTransform (world translation)
 *       SoSeparator geom_for_part_k      ← shared across instances of the same part
 *         SoCoordinate3 + SoIndexedFaceSet / SoLineSet
 *     SoSeparator inst_1
 *       ...
 *
 * Geometry is shared per unique part (Inventor's multiple-parent mechanism),
 * matching SoCADAssembly's model of uploading each geometry once.
 *
 * @param maxInstances  Number of instances to add; -1 means all.
 * @return Ref-counted SoSeparator; caller must call unref().
 */
static SoSeparator* buildFlatSG(const Assembly& gen, int maxInstances = -1)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoLightModel* lm = new SoLightModel;
    lm->model.setValue(SoLightModel::BASE_COLOR);
    root->addChild(lm);

    SoBaseColor* col = new SoBaseColor;
    col->rgb.setValue(0.7f, 0.75f, 0.8f);
    root->addChild(col);

    // Pre-build one geometry SoSeparator per unique part
    std::vector<SoSeparator*> partGeomNodes(gen.parts.size(), nullptr);
    for (size_t p = 0; p < gen.parts.size(); ++p)
        partGeomNodes[p] = buildGeomNodeSG(gen.parts[p].geometry);

    int limit = (maxInstances >= 0)
                ? std::min(maxInstances, int(gen.instances.size()))
                : int(gen.instances.size());

    for (int i = 0; i < limit; ++i) {
        const InstSpec& is = gen.instances[size_t(i)];
        SoSeparator* inst = new SoSeparator;
        SoTransform* xf   = new SoTransform;
        xf->translation.setValue(is.worldPos);
        inst->addChild(xf);
        inst->addChild(partGeomNodes[size_t(is.partIndex)]);
        root->addChild(inst);
    }

    return root;
}

} // namespace CadTestGen
