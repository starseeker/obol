/* View-local subpixel proxy rendering and hysteresis regression test. */

#include "headless_utils.h"
#include "cad/CadFramePlan.h"
#include "cad/CadGpuResources.h"

#define OBOL_INTERNAL 1
#include "glue/glp.h"

#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoPointLight.h>

#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/SoCADViewState.h>
#include <Obol/cad/CadIds.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoResetTransform.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoTransform.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename Result>
void
requireCadMutation(const Result& result, const char *operation)
{
    if (!result)
        throw std::runtime_error(std::string("CAD mutation failed: ") +
            operation);
}

template <typename Result>
Result
requireCadValue(Result result, const char *operation)
{
    requireCadMutation(result, operation);
    return result;
}

Obol::CadGeometryValidation
admitAndUpsertPart(SoCADAssembly *assembly, Obol::PartId part,
    Obol::PartGeometryBuilder geometry)
{
    const Obol::CadGeometryAdmission admission =
        Obol::cadAdmitPartGeometry(std::move(geometry));
    if (!admission)
        return admission.validation;
    return assembly->upsertParts({{part, admission.geometry}});
}

SoCADViewState *
cadViewState(SoSeparator *root)
{
    if (!root)
        throw std::invalid_argument("CAD test scene root is null");
    for (int index = 0; index < root->getNumChildren(); ++index) {
        SoNode *child = root->getChild(index);
        if (child && child->isOfType(SoCADViewState::getClassTypeId()))
            return static_cast<SoCADViewState *>(child);
    }
    SoCADViewState *viewState = new SoCADViewState;
    viewState->viewIdLow.setValue(1);
    root->addChild(viewState);
    return viewState;
}

void
setCadDrawMode(SoSeparator *root, SoCADViewState::DrawMode mode)
{
    cadViewState(root)->drawMode.setValue(mode);
}

void
setTestEnvironment(const char *name, const char *value, int overwrite)
{
#ifdef _WIN32
    if (!overwrite && std::getenv(name))
        return;
    const int result = _putenv_s(name, value);
#else
    const int result = ::setenv(name, value, overwrite);
#endif
    if (result != 0)
        throw std::runtime_error(
            std::string("cannot set test environment variable ") + name);
}

void
unsetTestEnvironment(const char *name)
{
#ifdef _WIN32
    const int result = _putenv_s(name, "");
#else
    const int result = ::unsetenv(name);
#endif
    if (result != 0)
        throw std::runtime_error(
            std::string("cannot unset test environment variable ") + name);
}

Obol::WireRep unitBox();
bool render(SoOffscreenRenderer& renderer, SoSeparator *root);
size_t nonBlackPixels(const SoOffscreenRenderer& renderer);

bool
sharedProjectedProxyContract()
{
    const SbVec3f corners[8] = {
        SbVec3f(-0.002f, -0.002f, -0.002f),
        SbVec3f( 0.002f, -0.002f, -0.002f),
        SbVec3f(-0.002f,  0.002f, -0.002f),
        SbVec3f( 0.002f,  0.002f, -0.002f),
        SbVec3f(-0.002f, -0.002f,  0.002f),
        SbVec3f( 0.002f, -0.002f,  0.002f),
        SbVec3f(-0.002f,  0.002f,  0.002f),
        SbVec3f( 0.002f,  0.002f,  0.002f)
    };
    SbMatrix identity;
    identity.makeIdentity();
    const SbVec2s viewport(256, 256);

    const Obol::CadProjectedProxy centered =
        Obol::classifyCadProjectedProxy(
            corners, identity, identity, viewport, 1.0f);
    if (!centered.visible || !centered.fullyContained ||
            !centered.pointEligible || centered.pixelWidth <= 0.0f ||
            centered.pixelHeight <= 0.0f)
        return false;

    /* A subpixel proxy straddling a clip plane remains visible but must not
     * collapse to a point.  This is the planner/renderer edge contract which
     * prevents a point request from leaving the renderer's structural box in
     * place indefinitely. */
    SbMatrix edge;
    edge.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
    const Obol::CadProjectedProxy clipped =
        Obol::classifyCadProjectedProxy(
            corners, edge, identity, viewport, 1.0f);
    if (!clipped.visible || clipped.fullyContained || clipped.pointEligible)
        return false;

    SbMatrix outside;
    outside.setTranslate(SbVec3f(1.01f, 0.0f, 0.0f));
    const Obol::CadProjectedProxy rejected =
        Obol::classifyCadProjectedProxy(
            corners, outside, identity, viewport, 1.0f);
    if (rejected.visible || rejected.pointEligible)
        return false;

    const Obol::CadProjectedProxy invalidViewport =
        Obol::classifyCadProjectedProxy(
            corners, identity, identity, SbVec2s(1, 256), 1.0f);
    return !invalidViewport.visible && !invalidViewport.pointEligible;
}

bool
degenerateStructuralProxyContract()
{
    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    for (SbVec3f& point : geometry.wire->segmentPoints)
        point[2] = 0.0f;
    geometry.wire->bounds = SbBox3f(
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f(0.5f, 0.5f, 0.0f));
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;

    SbVec3f corners[8];
    if (!Obol::cadPartGeometryProxyCorners(geometry, corners))
        return false;
    for (const SbVec3f& corner : corners) {
        if (corner[2] != 0.0f)
            return false;
    }
    return true;
}

std::array<SbVec3f, 8>
orientedProxyCorners()
{
    constexpr float cosine = 0.8660254038f;
    constexpr float sine = 0.5f;
    constexpr float halfLength = 2.0f;
    constexpr float halfWidth = 0.25f;
    constexpr float halfHeight = 0.2f;
    std::array<SbVec3f, 8> corners;
    for (size_t corner = 0; corner < corners.size(); ++corner) {
        const float localX = (corner & 1u) ? halfLength : -halfLength;
        const float localY = (corner & 2u) ? halfWidth : -halfWidth;
        corners[corner].setValue(
            cosine * localX - sine * localY,
            sine * localX + cosine * localY,
            (corner & 4u) ? halfHeight : -halfHeight);
    }
    return corners;
}

Obol::WireRep
orientedProxyWire(const std::array<SbVec3f, 8>& corners)
{
    Obol::internal::CadSubpixelProxyPoint proxy;
    proxy.boxCorners = corners;
    proxy.boxCornersValid = true;
    Obol::WireRep wire;
    wire.bounds.makeEmpty();
    Obol::internal::cadForEachAggregateProxyBoxVertex(
        proxy, [&wire](const SbVec3f& point) {
            wire.segmentPoints.push_back(point);
            wire.bounds.extendBy(point);
        });
    return wire;
}

bool
orientedAggregateProxyContract()
{
    const std::array<SbVec3f, 8> corners = orientedProxyCorners();
    Obol::PartGeometryBuilder geometry;
    geometry.wire = orientedProxyWire(corners);
    Obol::TriMesh shaded;
    shaded.positions = {
        corners[0], corners[1], corners[2], corners[4]
    };
    shaded.indices = {
        0u, 2u, 1u,
        0u, 1u, 3u,
        0u, 3u, 2u,
        1u, 2u, 3u
    };
    shaded.bounds.makeEmpty();
    for (const SbVec3f& point : shaded.positions)
        shaded.bounds.extendBy(point);
    geometry.shaded = std::move(shaded);
    geometry.shadedCullBackfaces = false;
    geometry.aggregateProxyCorners = corners;
    geometry.subpixelProxyEligible = true;

    const Obol::CadGeometryAdmission admitted =
        Obol::cadAdmitPartGeometry(geometry);
    if (!admitted || !admitted.geometry ||
            !admitted.geometry.get()->aggregateProxyCorners) {
        std::fprintf(stderr, "oriented aggregate proxy admission failed: %s\n",
            Obol::cadGeometryErrorName(admitted.validation.error));
        return false;
    }
    SbVec3f admittedCorners[8];
    if (!Obol::cadPartGeometryProxyCorners(
            *admitted.geometry.get(), admittedCorners)) {
        std::fprintf(stderr, "oriented aggregate proxy corners unavailable\n");
        return false;
    }
    for (size_t corner = 0; corner < corners.size(); ++corner)
        if (!admittedCorners[corner].equals(corners[corner], 1.0e-6f)) {
            std::fprintf(stderr,
                "oriented aggregate proxy corner changed during admission\n");
            return false;
        }

    Obol::PartGeometryBuilder malformed = geometry;
    (*malformed.aggregateProxyCorners)[7] += SbVec3f(0.25f, 0.0f, 0.0f);
    const Obol::CadGeometryAdmission rejected =
        Obol::cadAdmitPartGeometry(std::move(malformed));
    if (rejected || rejected.validation.error !=
            Obol::CadGeometryError::InvalidAggregateProxy) {
        std::fprintf(stderr,
            "malformed oriented aggregate proxy had result %s\n",
            Obol::cadGeometryErrorName(rejected.validation.error));
        return false;
    }

    /* A degenerate diagonal proxy has an AABB which covers this point, but
     * the point is not on the producer-certified proxy itself.  Admission
     * must test the oriented volume, not only its axis-aligned envelope. */
    Obol::PartGeometryBuilder nonConservative;
    Obol::PointRep outside;
    outside.positions = {SbVec3f(-1.0f, 1.0f, 0.0f)};
    outside.bounds.setBounds(outside.positions[0], outside.positions[0]);
    nonConservative.points = std::move(outside);
    std::array<SbVec3f, 8> diagonal;
    diagonal[0].setValue(-1.0f, -1.0f, 0.0f);
    const SbVec3f diagonalAxis(2.0f, 2.0f, 0.0f);
    for (size_t corner = 0; corner < diagonal.size(); ++corner)
        diagonal[corner] = diagonal[0] +
            ((corner & 1u) ? diagonalAxis : SbVec3f(0.0f, 0.0f, 0.0f));
    nonConservative.aggregateProxyCorners = diagonal;
    Obol::PartGeometryBuilder optionalNonConservative = nonConservative;
    const Obol::CadGeometryAdmission outsideRejected =
        Obol::cadAdmitPartGeometry(std::move(nonConservative));
    if (outsideRejected || outsideRejected.validation.error !=
            Obol::CadGeometryError::InvalidAggregateProxy) {
        std::fprintf(stderr,
            "non-conservative oriented aggregate proxy had result %s\n",
            Obol::cadGeometryErrorName(outsideRejected.validation.error));
        return false;
    }

    const Obol::CadGeometryAdmission proxyDiscarded =
        Obol::cadAdmitPartGeometry(
            std::move(optionalNonConservative),
            Obol::CadAggregateProxyPolicy::DiscardInvalid);
    if (!proxyDiscarded || !proxyDiscarded.geometry ||
            proxyDiscarded.geometry.get()->aggregateProxyCorners ||
            !proxyDiscarded.geometry.get()->points) {
        std::fprintf(stderr,
            "optional invalid aggregate proxy was not discarded: %s\n",
            Obol::cadGeometryErrorName(proxyDiscarded.validation.error));
        return false;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(20.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("oriented-aggregate-proxy");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "oriented aggregate proxy part");
    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "oriented-aggregate-proxy";
    instance.localToRoot.setTranslate(SbVec3f(0.5f, -0.25f, 0.0f));
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "oriented aggregate proxy instance");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const Obol::CadRenderedWork shadedWork =
        assembly->lastRenderedWork();
    const Obol::CadAggregateProxyPresentationWork shadedProxies =
        assembly->lastAggregateProxyPresentationWork();
    passed = passed &&
        assembly->lastSubpixelProxyCount() == 1u &&
        assembly->lastSubpixelProxyDrawPointCount() == 0u &&
        shadedProxies.exact && shadedProxies.pointCount == 0u &&
        shadedProxies.axisAlignedBoxCount == 0u &&
        shadedProxies.orientedBoxCount == 1u &&
        shadedWork.triangleCount ==
            Obol::CadAggregateProxyBoxTriangleCount &&
        shadedWork.lineCount == 0u &&
        nonBlackPixels(renderer) > 0u;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    const bool wireRendered = render(renderer, root);
    const Obol::CadRenderedWork wireWork =
        assembly->lastRenderedWork();
    const Obol::CadAggregateProxyPresentationWork wireProxies =
        assembly->lastAggregateProxyPresentationWork();
    passed = passed && wireRendered &&
        assembly->lastSubpixelProxyCount() == 1u &&
        assembly->lastSubpixelProxyDrawPointCount() == 0u &&
        wireProxies.exact && wireProxies.pointCount == 0u &&
        wireProxies.axisAlignedBoxCount == 0u &&
        wireProxies.orientedBoxCount == 1u &&
        wireWork.triangleCount == 0u &&
        wireWork.lineCount ==
            Obol::CadAggregateProxyBoxLineCount &&
        nonBlackPixels(renderer) > 0u;
    if (!passed) {
        const Obol::CadRenderedWork work = assembly->lastRenderedWork();
        std::fprintf(stderr,
            "oriented aggregate proxy render failed "
            "(logical=%zu points=%zu triangles=%llu lines=%llu pixels=%zu)\n",
            assembly->lastSubpixelProxyCount(),
            assembly->lastSubpixelProxyDrawPointCount(),
            static_cast<unsigned long long>(work.triangleCount),
            static_cast<unsigned long long>(work.lineCount),
            nonBlackPixels(renderer));
    }
    root->unref();
    return passed;
}

void
setProgressiveCuts(Obol::TriMesh& mesh, size_t cutCount,
                   uint32_t indexCount, uint32_t positionCount)
{
    mesh.progressiveCuts.resize(cutCount);
    for (Obol::ProgressiveTriangleCut& cut : mesh.progressiveCuts) {
        cut.indexCount = indexCount;
        cut.positionCount = positionCount;
    }
}

bool
sparseUniformClusterContract()
{
    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 0;
    setProgressiveCuts(mesh, 1, 3, 3);
    mesh.progressiveClusterGridResolution = 8;
    mesh.progressiveClusters.resize(2);
    if (!mesh.hasProgressiveClusters() ||
            mesh.hasAdaptiveProgressiveClusters())
        return false;
    mesh.progressiveClusters.resize(513);
    if (mesh.hasProgressiveClusters())
        return false;

    Obol::WireRep wire;
    wire.segmentPoints = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f)};
    wire.progressiveCuts.resize(1);
    wire.progressiveCuts[0].segmentCount = 1;
    wire.progressiveMinimumCut = 0;
    wire.progressiveResidentCut = 0;
    wire.progressiveClusterGridResolution = 8;
    wire.progressiveClusters.resize(2);
    return wire.hasProgressiveClusters() &&
        !wire.hasAdaptiveProgressiveClusters();
}

Obol::WireRep
unitBox()
{
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    const SbVec3f corners[8] = {
        SbVec3f(-0.5f, -0.5f, -0.5f), SbVec3f(0.5f, -0.5f, -0.5f),
        SbVec3f(0.5f, 0.5f, -0.5f), SbVec3f(-0.5f, 0.5f, -0.5f),
        SbVec3f(-0.5f, -0.5f, 0.5f), SbVec3f(0.5f, -0.5f, 0.5f),
        SbVec3f(0.5f, 0.5f, 0.5f), SbVec3f(-0.5f, 0.5f, 0.5f)
    };
    Obol::WireRep wire;
    for (const auto &edge : edges) {
        wire.segmentPoints.push_back(corners[edge[0]]);
        wire.segmentPoints.push_back(corners[edge[1]]);
    }
    wire.bounds = SbBox3f(corners[0], corners[6]);
    return wire;
}

bool
render(SoOffscreenRenderer &renderer, SoSeparator *root)
{
    return renderer.render(root) == TRUE && renderer.getBuffer() != nullptr;
}

size_t
nonBlackPixels(const SoOffscreenRenderer &renderer)
{
    const unsigned char *buffer = renderer.getBuffer();
    const SbVec2s size = renderer.getViewportRegion().getViewportSizePixels();
    if (!buffer || size[0] <= 0 || size[1] <= 0)
        return 0;

    size_t count = 0;
    const size_t pixelCount = static_cast<size_t>(size[0]) *
        static_cast<size_t>(size[1]);
    for (size_t i = 0; i < pixelCount; ++i) {
        const unsigned char *pixel = buffer + (i * 3u);
        if (pixel[0] || pixel[1] || pixel[2])
            ++count;
    }
    return count;
}

double
nonBlackPixelCentroidX(const SoOffscreenRenderer &renderer)
{
    const unsigned char *buffer = renderer.getBuffer();
    const SbVec2s size = renderer.getViewportRegion().getViewportSizePixels();
    if (!buffer || size[0] <= 0 || size[1] <= 0)
        return -1.0;
    uint64_t count = 0;
    uint64_t total = 0;
    for (int y = 0; y < size[1]; ++y) {
        for (int x = 0; x < size[0]; ++x) {
            const size_t pixel = static_cast<size_t>(y) *
                static_cast<size_t>(size[0]) + static_cast<size_t>(x);
            const unsigned char *rgb = buffer + pixel * 3u;
            if (rgb[0] || rgb[1] || rgb[2]) {
                total += static_cast<uint64_t>(x);
                ++count;
            }
        }
    }
    return count ? static_cast<double>(total) / static_cast<double>(count) :
        -1.0;
}

bool
enclosingModelTransformMovesCadRendering()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);

    SoTransform *transform = new SoTransform;
    root->addChild(transform);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("enclosing-transform-part");
    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "enclosing transform part");
    Obol::InstanceRecord record;
    record.part = part;
    requireCadMutation(assembly->upsertInstance(
        Obol::CadIdBuilder::instanceId("enclosing-transform-instance"),
        record), "enclosing transform instance");

    SoOffscreenRenderer renderer(SbViewportRegion(256, 256));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    const bool firstRendered = render(renderer, root);
    const double firstCentroid = nonBlackPixelCentroidX(renderer);
    transform->translation.setValue(1.0f, 0.0f, 0.0f);
    const bool secondRendered = render(renderer, root);
    const double secondCentroid = nonBlackPixelCentroidX(renderer);
    root->unref();

    /* A one-unit translation spans one quarter of the four-unit viewport.
     * Leave margin for line rasterization and backend rounding. */
    return firstRendered && secondRendered && firstCentroid >= 0.0 &&
        secondCentroid - firstCentroid > 48.0;
}

bool
sparseGeometryTopologyChangesRebuildPlan()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("sparse-topology-part");
    Obol::PartGeometryBuilder initial;
    initial.wire = unitBox();
    initial.points.emplace();
    initial.points->bounds.makeEmpty();
    requireCadMutation(admitAndUpsertPart(assembly, part, initial),
        "initial sparse topology");
    Obol::InstanceRecord record;
    record.part = part;
    requireCadMutation(assembly->upsertInstance(
        Obol::CadIdBuilder::instanceId("sparse-topology-instance"),
        record), "sparse topology instance");

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    if (!render(renderer, root)) {
        root->unref();
        return false;
    }
    const uint64_t initialBuilds = assembly->framePlanBuildCount();

    Obol::PartGeometryBuilder edgeSource;
    edgeSource.shaded.emplace();
    edgeSource.shaded->positions = {
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f(0.5f, -0.5f, 0.0f),
        SbVec3f(0.0f, 0.5f, 0.0f)};
    edgeSource.shaded->indices = {0u, 1u, 2u};
    edgeSource.shaded->bounds = SbBox3f(
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f(0.5f, 0.5f, 0.0f));
    const Obol::CadGeometryAdmission edgeAdmission =
        Obol::cadAdmitPartGeometry(std::move(edgeSource));
    if (!edgeAdmission) {
        root->unref();
        return false;
    }

    Obol::PartGeometryBuilder derived;
    derived.wire.emplace();
    derived.wire->triangleEdgeGeometry = edgeAdmission.geometry.shared();
    derived.wire->triangleEdgeSegmentCount = 3u;
    derived.wire->bounds = edgeAdmission.geometry.get()->shaded->bounds;
    derived.points.emplace();
    derived.points->bounds.makeEmpty();
    requireCadMutation(admitAndUpsertPart(assembly, part, derived),
        "derived-wire sparse topology");
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != initialBuilds + 1u) {
        root->unref();
        return false;
    }

    Obol::PartGeometryBuilder populated;
    populated.wire.emplace();
    populated.wire->triangleEdgeGeometry = edgeAdmission.geometry.shared();
    populated.wire->triangleEdgeSegmentCount = 3u;
    populated.wire->bounds = edgeAdmission.geometry.get()->shaded->bounds;
    populated.points.emplace();
    populated.points->positions = {SbVec3f(0.0f, 0.0f, 0.0f)};
    populated.points->bounds = SbBox3f(
        SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(0.0f, 0.0f, 0.0f));
    requireCadMutation(admitAndUpsertPart(assembly, part, populated),
        "populated-point sparse topology");
    const bool rebuilt = render(renderer, root) &&
        assembly->framePlanBuildCount() == initialBuilds + 2u;
    root->unref();
    return rebuilt;
}

bool
triangleDerivedProgressiveWireGrowsWithRequestedCut()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    Obol::PartGeometryBuilder source;
    source.shaded.emplace();
    source.shaded->positions = {
        SbVec3f(-0.75f, -0.75f, 0.0f),
        SbVec3f(0.75f, -0.75f, 0.0f),
        SbVec3f(-0.75f, 0.75f, 0.0f),
        SbVec3f(0.75f, 0.75f, 0.0f)};
    source.shaded->indices = {0u, 1u, 2u, 1u, 3u, 2u};
    source.shaded->bounds = SbBox3f(
        SbVec3f(-0.75f, -0.75f, 0.0f),
        SbVec3f(0.75f, 0.75f, 0.0f));
    source.shaded->progressiveCuts.resize(2);
    source.shaded->progressiveCuts[0].indexCount = 3u;
    source.shaded->progressiveCuts[0].positionCount = 3u;
    source.shaded->progressiveCuts[1].indexCount = 6u;
    source.shaded->progressiveCuts[1].positionCount = 4u;
    source.shaded->progressiveMinimumCut = 0u;
    source.shaded->progressiveResidentCut = 1u;
    source.shaded->progressiveLineage = 0x445752495245ULL;
    const Obol::CadGeometryAdmission admittedSource =
        Obol::cadAdmitPartGeometry(std::move(source));
    if (!admittedSource) {
        root->unref();
        return false;
    }

    Obol::PartGeometryBuilder wireGeometry;
    wireGeometry.wire.emplace();
    wireGeometry.wire->triangleEdgeGeometry =
        admittedSource.geometry.shared();
    wireGeometry.wire->triangleEdgeSegmentCount = 6u;
    wireGeometry.wire->bounds =
        admittedSource.geometry.get()->shaded->bounds;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("derived-progressive-wire-part");
    requireCadMutation(admitAndUpsertPart(
        assembly, part, std::move(wireGeometry)),
        "derived progressive wire part");
    Obol::InstanceRecord record;
    record.part = part;
    record.lodCut = 0u;
    const Obol::InstanceId instance =
        Obol::CadIdBuilder::instanceId("derived-progressive-wire-instance");
    requireCadMutation(assembly->upsertInstance(instance, record),
        "derived progressive wire instance");

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    if (!render(renderer, root)) {
        root->unref();
        return false;
    }
    const Obol::CadRenderedWork coarse = assembly->lastRenderedWork();
    Obol::InstanceLodUpdate rich;
    rich.instance = instance;
    rich.lodCut = 1u;
    requireCadMutation(assembly->updateInstanceCuts({rich}),
        "derived progressive wire rich cut");
    const bool rendered = render(renderer, root);
    const Obol::CadRenderedWork richer = assembly->lastRenderedWork();
    root->unref();
    return rendered && coarse.exact && richer.exact &&
        coarse.lineCount == 3u && richer.lineCount == 6u;
}

bool
assemblyDestructionReleasesGpuResourcesOnLiveContext()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    if (!render(renderer, root)) {
        root->unref();
        return false;
    }
    const size_t baseline =
        Obol::internal::CadGpuResources::liveInstanceCountForTesting();

    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("destruction-gpu-resource-part");
    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "destruction GPU resource part");
    Obol::InstanceRecord record;
    record.part = part;
    requireCadMutation(assembly->upsertInstance(
        Obol::CadIdBuilder::instanceId("destruction-gpu-resource-instance"),
        record), "destruction GPU resource instance");
    if (!render(renderer, root) ||
            Obol::internal::CadGpuResources::liveInstanceCountForTesting() !=
                baseline + 1u) {
        root->unref();
        return false;
    }

    root->removeChild(assembly);
    const bool deferred =
        Obol::internal::CadGpuResources::liveInstanceCountForTesting() ==
            baseline + 1u;
    const bool drained = render(renderer, root) &&
        Obol::internal::CadGpuResources::liveInstanceCountForTesting() ==
            baseline;
    root->unref();
    return deferred && drained;
}

bool
fixedCutPreparationIsFinite(bool wire)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    setCadDrawMode(root, wire ? SoCADViewState::WIREFRAME :
        SoCADViewState::SHADED);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    Obol::PartGeometryBuilder source;
    source.shaded.emplace();
    auto& mesh = *source.shaded;
    mesh.positions = {SbVec3f(-0.75f, -0.75f, 0.0f),
        SbVec3f(0.75f, -0.75f, 0.0f), SbVec3f(0.0f, 0.75f, 0.0f)};
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = SbBox3f(SbVec3f(-0.75f, -0.75f, 0.0f),
        SbVec3f(0.75f, 0.75f, 0.0f));
    setProgressiveCuts(mesh, 3, 3, 3);
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 2;
    mesh.progressiveLineage = 1;
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();
    for (size_t cut = 0; cut < mesh.progressiveCuts.size(); ++cut)
        mesh.progressiveCuts[cut].quantization = {
            static_cast<uint8_t>(6u + cut),
            static_cast<uint8_t>(6u + cut), 0};
    Obol::CadGeometryAdmission admitted =
        Obol::cadAdmitPartGeometry(std::move(source));
    requireCadMutation(admitted, "fixed preparation mesh");
    if (wire) {
        Obol::PartGeometryBuilder derived;
        derived.wire.emplace();
        derived.wire->triangleEdgeGeometry = admitted.geometry.shared();
        derived.wire->triangleEdgeSegmentCount = 3;
        derived.wire->bounds = admitted.geometry.get()->shaded->bounds;
        admitted = Obol::cadAdmitPartGeometry(std::move(derived));
        requireCadMutation(admitted, "fixed preparation wire");
    }
    const auto part = Obol::CadIdBuilder::partId("fixed-preparation");
    requireCadMutation(assembly->upsertParts({{part, admitted.geometry}}),
        "fixed preparation part");
    Obol::InstanceRecord instance;
    instance.part = part;
    instance.lodCut = 2;
    requireCadMutation(assembly->upsertInstance(
        Obol::CadIdBuilder::instanceId("fixed-preparation"), instance),
        "fixed preparation instance");

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    const auto expectedKind = wire ?
        Obol::CadPresentationPreparationKind::IndexedWireCuts :
        Obol::CadPresentationPreparationKind::FixedFunctionCuts;
    bool passed = true;
    Obol::CadPresentationPreparationTarget previous;
    /* Both finer and coarser new cuts incur preparation.  An exact replay
     * must retain its certificate, not grant another first-frame retry. */
    for (int cut : {0, 2, 1}) {
        cadViewState(root)->progressiveCutCeiling.setValue(cut);
        passed = render(renderer, root) && passed;
        const auto prepared = assembly->presentationPreparationSnapshot();
        passed = passed && prepared.target.kind == expectedKind &&
            prepared.target != previous &&
            prepared.target.progressiveCutCeiling == cut &&
            prepared.state == Obol::CadPresentationPreparationState::Complete &&
            prepared.totalUnits > 0 &&
            prepared.completedUnits == prepared.totalUnits;
        passed = render(renderer, root) && passed;
        const auto replay = assembly->presentationPreparationSnapshot();
        passed = passed && replay.target == prepared.target &&
            replay.totalUnits == prepared.totalUnits &&
            replay.completedUnits == prepared.completedUnits;
        previous = prepared.target;
    }
    root->unref();
    return passed;
}

bool
softwareSubpixelProxyAggregationContract()
{
    /*
     * This intentionally uses many independently retained occurrences of
     * one tiny structural part.  Logical coverage must remain one proxy per
     * occurrence, while the OSMesa executor is allowed only a bounded
     * camera-local point stream.  Keep this outside the broader rendering
     * scenario below so a failure cannot be hidden by its mesh work.
     */
    static constexpr uint32_t occurrenceCount = 8192u;
    static constexpr uint32_t gridWidth = 128u;
    static constexpr float gridSpacing = 4.0f;
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(1000.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("software-subpixel-proxy-part");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "subpixel proxy part");

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "software-subpixel-proxy";
        instance.occurrenceIndex = index;
        instance.lodStructuralProxy = true;
        instance.localToRoot.setTranslate(SbVec3f(
            (static_cast<float>(index % gridWidth) -
                static_cast<float>(gridWidth) * 0.5f) * gridSpacing,
            (static_cast<float>(index / gridWidth) -
                static_cast<float>(occurrenceCount / gridWidth) * 0.5f) *
                gridSpacing,
            0.0f));
        Obol::InstanceUpdate update;
        update.instance = Obol::CadIdBuilder::childInstance(
            instance.parent, instance.childName, instance.occurrenceIndex,
            instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    requireCadMutation(assembly->upsertInstances(updates),
        "subpixel proxy instances");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    const bool rendered = render(renderer, root);
    const size_t logicalCount = assembly->lastSubpixelProxyCount();
    const size_t drawCount = assembly->lastSubpixelProxyDrawPointCount();
    root->unref();
    if (!rendered || logicalCount != occurrenceCount || !drawCount ||
            drawCount > logicalCount) {
        std::fprintf(stderr,
            "software subpixel aggregation did not preserve logical "
            "coverage or bound point submission (%zu logical, %zu draw)\n",
            logicalCount, drawCount);
        return false;
    }
    return true;
}

struct HalfImageStats {
    double leftMean = 0.0;
    double rightMean = 0.0;
    size_t leftPixels = 0;
    size_t rightPixels = 0;
};

HalfImageStats
foregroundHalfStats(const SoOffscreenRenderer &renderer)
{
    HalfImageStats result;
    const unsigned char *buffer = renderer.getBuffer();
    const SbVec2s size = renderer.getViewportRegion().getViewportSizePixels();
    if (!buffer || size[0] <= 0 || size[1] <= 0)
        return result;

    double leftSum = 0.0;
    double rightSum = 0.0;
    for (int y = 0; y < size[1]; ++y) {
        for (int x = 0; x < size[0]; ++x) {
            const unsigned char *pixel =
                buffer + (static_cast<size_t>(y) * size[0] + x) * 3u;
            if (!pixel[0] && !pixel[1] && !pixel[2])
                continue;
            const double luma =
                0.2126 * pixel[0] + 0.7152 * pixel[1] + 0.0722 * pixel[2];
            if (x < size[0] / 2) {
                leftSum += luma;
                ++result.leftPixels;
            } else {
                rightSum += luma;
                ++result.rightPixels;
            }
        }
    }
    if (result.leftPixels)
        result.leftMean = leftSum / result.leftPixels;
    if (result.rightPixels)
        result.rightMean = rightSum / result.rightPixels;
    return result;
}

bool
flatFaceLightingMatchesExplicitNormals(bool batch)
{
    struct LightingCase {
        const char *name;
        bool positional;
        bool localViewer;
        bool mixedNormals;
        bool flatInput;
    };
    const LightingCase cases[] = {
        {"directional", false, false, false, false},
        {"positional", true, false, false, false},
        {"local-viewer", false, true, false, false},
        {"mixed-normals", false, false, true, false},
        {"flat-input", false, false, false, true}
    };
    struct GlState {
        bool localViewer = false;
        GLint expectedShadeModel = GL_SMOOTH;
        bool restored = false;
    };
    const auto configureGl = [](void *data, SoAction *action) {
        if (!action->isOfType(SoGLRenderAction::getClassTypeId()))
            return;
        const auto *state = static_cast<const GlState *>(data);
        const auto *renderAction = static_cast<SoGLRenderAction *>(action);
        const SoGLContext *glue = SoGLContext_instance(
            renderAction->getCacheContext());
        glue->glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,
            state->localViewer ? GL_TRUE : GL_FALSE);
        glue->glShadeModel(state->expectedShadeModel);
    };
    const auto inspectGl = [](void *data, SoAction *action) {
        if (!action->isOfType(SoGLRenderAction::getClassTypeId()))
            return;
        auto *state = static_cast<GlState *>(data);
        const auto *renderAction = static_cast<SoGLRenderAction *>(action);
        const SoGLContext *glue = SoGLContext_instance(
            renderAction->getCacheContext());
        GLint shadeModel = 0;
        glue->glGetIntegerv(GL_SHADE_MODEL, &shadeModel);
        state->restored = shadeModel == state->expectedShadeModel;
    };
    constexpr int width = 192;
    constexpr int height = 160;
    constexpr size_t components = 3;
    const auto renderCase = [&](const LightingCase& test,
                                bool explicitNormals) {
        std::vector<unsigned char> pixels;
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->height = 2.4f;
        camera->nearDistance = 0.1f;
        camera->farDistance = 10.0f;
        root->addChild(camera);
        if (test.positional) {
            SoPointLight *light = new SoPointLight;
            light->location.setValue(-0.7f, 0.8f, 1.0f);
            root->addChild(light);
        } else {
            SoDirectionalLight *key = new SoDirectionalLight;
            key->direction.setValue(-0.3f, -0.4f, -1.0f);
            root->addChild(key);
            SoDirectionalLight *fill = new SoDirectionalLight;
            fill->direction.setValue(0.6f, 0.2f, -1.0f);
            fill->intensity = 0.3f;
            root->addChild(fill);
        }
        GlState state;
        state.localViewer = test.localViewer;
        state.expectedShadeModel = test.flatInput ? GL_FLAT : GL_SMOOTH;
        SoCallback *before = new SoCallback;
        before->setCallback(configureGl, &state);
        root->addChild(before);
        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);
        SoCallback *after = new SoCallback;
        after->setCallback(inspectGl, &state);
        root->addChild(after);

        /* The batch route requires 128 distinct parts.  Alternating windings
         * exercise both sides; explicit constant normals provide the same
         * lighting through the batch's smooth-normal path. */
        constexpr unsigned columns = 16;
        constexpr unsigned rows = 8;
        for (unsigned partIndex = 0; partIndex < columns * rows; ++partIndex) {
            const bool backFacing = partIndex % 2 != 0;
            Obol::TriMesh mesh;
            mesh.positions = {SbVec3f(-0.065f, -0.10f, 0.0f),
                SbVec3f(0.065f, -0.10f, 0.0f), SbVec3f(0.0f, 0.10f, 0.0f)};
            mesh.indices = backFacing ? std::vector<uint32_t>{0, 2, 1} :
                std::vector<uint32_t>{0, 1, 2};
            const float normalZ = backFacing ? -1.0f : 1.0f;
            if (explicitNormals || (test.mixedNormals && backFacing))
                mesh.normals.assign(mesh.positions.size(),
                    SbVec3f(0.0f, 0.0f, normalZ));
            if (test.mixedNormals && backFacing) {
                mesh.normals[0] = SbVec3f(0.6f, 0.0f, -0.8f);
                mesh.normals[1] = SbVec3f(-0.6f, 0.0f, -0.8f);
            }
            mesh.bounds.makeEmpty();
            for (const auto& point : mesh.positions)
                mesh.bounds.extendBy(point);
            if (!batch) {
                mesh.progressiveMinimumCut = 0;
                mesh.progressiveResidentCut = 0;
                setProgressiveCuts(mesh, 1, 3, 3);
                /* Both references must prepare a cut, including meshes
                 * whose explicit normals would allow an exact-cut bypass. */
                mesh.progressiveCuts[0].quantization = {12, 12, 0};
                mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
                mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();
            }
            Obol::PartGeometryBuilder geometry;
            geometry.shaded = std::move(mesh);
            geometry.shadedCullBackfaces = false;
            const std::string name = "flat-face-" + std::to_string(partIndex);
            const Obol::PartId part = Obol::CadIdBuilder::partId(name);
            requireCadMutation(admitAndUpsertPart(assembly, part,
                std::move(geometry)), name.c_str());
            Obol::InstanceRecord instance;
            instance.part = part;
            instance.parent = Obol::CadIdBuilder::rootInstance();
            instance.childName = name;
            instance.localToRoot.setTranslate(
                SbVec3f(-1.2f + 0.16f * (partIndex % columns),
                    -0.875f + 0.25f * (partIndex / columns), 0.0f));
            instance.style.hasColorOverride = true;
            instance.style.color = SbColor4f(0.6f, 0.35f, 0.2f, 1.0f);
            requireCadMutation(assembly->upsertInstanceAuto(instance),
                name.c_str());
        }
        SoOffscreenRenderer renderer(SbViewportRegion(width, height));
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (render(renderer, root) && state.restored &&
                assembly->lastRenderTier() == (batch ? 4 : 1) &&
                assembly->presentationPreparationSnapshot().target.kind ==
                    (batch ? Obol::CadPresentationPreparationKind::FlatShadedAtlas :
                        Obol::CadPresentationPreparationKind::FixedFunctionCuts) &&
                nonBlackPixels(renderer) > 100) {
            const unsigned char *buffer = renderer.getBuffer();
            pixels.assign(buffer, buffer + width * height * components);
        }
        root->unref();
        return pixels;
    };

    struct EnvironmentSnapshot {
        const char *name;
        bool present;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT", false, {}},
        {"OBOL_CAD_FLAT_SHADED", false, {}},
        {"OBOL_CAD_SOFTWARE_GLSL", false, {}}
    };
    for (auto& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        setting.value = value ? value : "";
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", batch ? "1" : "0", 1);
    setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "0", 1);
    bool matched = true;
    for (const auto& test : cases) {
        const auto face = renderCase(test, false);
        const auto explicitNormals = renderCase(test, true);
        if (face.empty() || face != explicitNormals) {
            size_t differences = 0;
            unsigned maximumDifference = 0;
            const size_t sharedSize =
                std::min(face.size(), explicitNormals.size());
            for (size_t i = 0; i < sharedSize; ++i) {
                const unsigned difference = std::abs(
                    int(face[i]) - int(explicitNormals[i]));
                differences += difference != 0;
                maximumDifference = std::max(maximumDifference, difference);
            }
            std::fprintf(stderr,
                "flat face lighting mismatch: %s sizes=%zu/%zu "
                "channels=%zu max=%u\n", test.name, face.size(),
                explicitNormals.size(), differences, maximumDifference);
            matched = false;
        }
    }
    for (const auto& setting : settings) {
        if (setting.present)
            setTestEnvironment(setting.name, setting.value.c_str(), 1);
        else
            unsetTestEnvironment(setting.name);
    }
    return matched;
}

bool
normalFreeTwoSidedGlslMatchesFixed()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                   previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        HalfImageStats image;
    };
    const auto renderRoute = [](bool softwareGlsl,
                                bool coarseOriented) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        /*
         * Renderer configuration is immutable per assembly.  Construct a
         * distinct scene after selecting each route; changing the environment
         * around repeated renders of one node only retests the cached route.
         */
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(2.4f);
        root->addChild(camera);

        SoDirectionalLight *light = new SoDirectionalLight;
        light->direction.setValue(0.0f, 0.0f, -1.0f);
        root->addChild(light);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        /*
         * Two disjoint, coplanar triangles deliberately use opposite winding
         * and have no authored normal stream, matching normal-free PoP
         * payloads such as Lucy.  Both routes must light both faces equally.
         */
        Obol::TriMesh mesh;
        mesh.positions = {
            SbVec3f(-1.05f, -0.8f, 0.0f),
            SbVec3f(-0.05f, -0.8f, 0.0f),
            SbVec3f(-0.55f, 0.8f, 0.0f),
            SbVec3f(0.05f, -0.8f, 0.0f),
            SbVec3f(1.05f, -0.8f, 0.0f),
            SbVec3f(0.55f, 0.8f, 0.0f)
        };
        mesh.indices = {0, 1, 2, 3, 5, 4};
        mesh.bounds.makeEmpty();
        for (const SbVec3f& point : mesh.positions)
            mesh.bounds.extendBy(point);
        mesh.progressiveMinimumCut = coarseOriented ? 8 : 15;
        mesh.progressiveResidentCut = 16;
        setProgressiveCuts(mesh, 17,
            static_cast<uint32_t>(mesh.indices.size()),
            static_cast<uint32_t>(mesh.positions.size()));
        for (size_t cut = mesh.progressiveMinimumCut;
                cut < mesh.progressiveCuts.size(); ++cut) {
            mesh.progressiveCuts[cut].quantization = cut == 16 ?
                Obol::ProgressiveQuantization{16, 16, 0} :
                Obol::ProgressiveQuantization{8, 8, 0};
        }
        mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
        mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

        Obol::PartGeometryBuilder geometry;
        geometry.shaded = std::move(mesh);
        geometry.shadedCullBackfaces = coarseOriented;
        const Obol::PartId part =
            Obol::CadIdBuilder::partId(coarseOriented ?
                "normal-free-coarse-oriented" :
                "normal-free-two-sided");
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "retained proxy part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = coarseOriented ?
            "normal-free-coarse-oriented" :
            "normal-free-two-sided";
        instance.localToRoot.makeIdentity();
        instance.lodCut = coarseOriented ? 8 : 16;
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "retained proxy instance");

        const SbViewportRegion viewport(256, 192);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered)
            result.image = foregroundHalfStats(renderer);
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false, false);
    const RouteResult glsl = renderRoute(true, false);
    const RouteResult coarseFixed = renderRoute(false, true);
    const RouteResult coarseGlsl = renderRoute(true, true);
    restoreGlslEnvironment();

    if (!fixed.rendered || !glsl.rendered ||
            !coarseFixed.rendered || !coarseGlsl.rendered ||
            fixed.image.leftPixels < 100 ||
            fixed.image.rightPixels < 100 ||
            glsl.image.leftPixels < 100 ||
            glsl.image.rightPixels < 100 ||
            coarseFixed.image.leftPixels < 100 ||
            coarseFixed.image.rightPixels < 100 ||
            coarseGlsl.image.leftPixels < 100 ||
            coarseGlsl.image.rightPixels < 100)
        return false;
    const double minimumLit = 0.75 *
        (std::min)(fixed.image.leftMean, fixed.image.rightMean);
    const double glslLow =
        (std::min)(glsl.image.leftMean, glsl.image.rightMean);
    const double glslHigh =
        (std::max)(glsl.image.leftMean, glsl.image.rightMean);
    const bool exactMatched = glslLow >= minimumLit && glslHigh > 0.0 &&
        glslLow / glslHigh >= 0.9;
    const double coarseFixedLow = (std::min)(
        coarseFixed.image.leftMean, coarseFixed.image.rightMean);
    const double coarseFixedHigh = (std::max)(
        coarseFixed.image.leftMean, coarseFixed.image.rightMean);
    const double coarseGlslLow = (std::min)(
        coarseGlsl.image.leftMean, coarseGlsl.image.rightMean);
    const double coarseGlslHigh = (std::max)(
        coarseGlsl.image.leftMean, coarseGlsl.image.rightMean);
    /* A source certified as oriented may use one-sided lighting only at an
     * exact cut.  Non-exact quantization disables culling, so both newly
     * visible sides must remain lit on the fixed and GLSL routes. */
    const bool coarseMatched = coarseFixedHigh > 0.0 &&
        coarseFixedLow / coarseFixedHigh >= 0.9 &&
        coarseGlslHigh > 0.0 && coarseGlslLow / coarseGlslHigh >= 0.9 &&
        coarseGlslLow >= 0.75 * coarseFixedLow;
    const bool matched = exactMatched && coarseMatched;
    if (!matched) {
        std::fprintf(stderr,
            "two-sided stats fixed={left=%.3f/%zu right=%.3f/%zu} "
            "glsl={left=%.3f/%zu right=%.3f/%zu} "
            "coarse-fixed={left=%.3f/%zu right=%.3f/%zu} "
            "coarse-glsl={left=%.3f/%zu right=%.3f/%zu}\n",
            fixed.image.leftMean, fixed.image.leftPixels,
            fixed.image.rightMean, fixed.image.rightPixels,
            glsl.image.leftMean, glsl.image.leftPixels,
            glsl.image.rightMean, glsl.image.rightPixels,
            coarseFixed.image.leftMean, coarseFixed.image.leftPixels,
            coarseFixed.image.rightMean, coarseFixed.image.rightPixels,
            coarseGlsl.image.leftMean, coarseGlsl.image.leftPixels,
            coarseGlsl.image.rightMean, coarseGlsl.image.rightPixels);
    }
    return matched;
}

bool
nonUniformNormalTransformMatchesFixed()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        size_t pixels = 0;
        double mean = 0.0;
    };
    const auto renderRoute = [](bool softwareGlsl) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(2.4f);
        root->addChild(camera);
        SoDirectionalLight *light = new SoDirectionalLight;
        light->direction.setValue(0.0f, -1.0f, 0.0f);
        root->addChild(light);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        Obol::TriMesh mesh;
        mesh.positions = {
            SbVec3f(-0.25f, -0.8f, 0.0f),
            SbVec3f(0.25f, -0.8f, 0.0f),
            SbVec3f(0.0f, 0.8f, 0.0f)};
        mesh.normals.assign(3, SbVec3f(
            0.70710678f, 0.70710678f, 0.0f));
        mesh.indices = {0, 1, 2};
        mesh.bounds = SbBox3f(
            SbVec3f(-0.25f, -0.8f, 0.0f),
            SbVec3f(0.25f, 0.8f, 0.0f));
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = std::move(mesh);
        const Obol::PartId part =
            Obol::CadIdBuilder::partId("non-uniform-normal-transform");
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "non-uniform normal part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "non-uniform-normal-transform";
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        instance.localToRoot.makeIdentity();
        instance.localToRoot[0][0] = 4.0f;
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "non-uniform normal instance");

        const SbViewportRegion viewport(256, 192);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered) {
            const HalfImageStats stats = foregroundHalfStats(renderer);
            result.pixels = stats.leftPixels + stats.rightPixels;
            if (result.pixels)
                result.mean =
                    (stats.leftMean * stats.leftPixels +
                     stats.rightMean * stats.rightPixels) / result.pixels;
        }
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false);
    const RouteResult glsl = renderRoute(true);
    restoreGlslEnvironment();
    const bool matched = fixed.rendered && glsl.rendered &&
        fixed.pixels > 500 && glsl.pixels > 500 && fixed.mean > 1.0 &&
        glsl.mean >= fixed.mean * 0.8 && glsl.mean <= fixed.mean * 1.2;
    if (!matched)
        std::fprintf(stderr,
            "non-uniform normal stats fixed={mean=%.3f pixels=%zu} "
            "glsl={mean=%.3f pixels=%zu}\n",
            fixed.mean, fixed.pixels, glsl.mean, glsl.pixels);
    return matched;
}

bool
progressiveNormalFacingIgnoresQuantizedWinding()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        HalfImageStats image;
    };
    const auto renderRoute = [](bool softwareGlsl) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(1.2f);
        root->addChild(camera);
        SoDirectionalLight *light = new SoDirectionalLight;
        light->direction.setValue(0.0f, 0.0f, -1.0f);
        root->addChild(light);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        Obol::TriMesh reference;
        reference.positions = {
            SbVec3f(0.05f, 0.25f, 0.0f),
            SbVec3f(0.95f, 0.25f, 0.0f),
            SbVec3f(0.50f, 0.40f, 0.0f)};
        reference.normals.assign(3, SbVec3f(0.0f, 0.0f, 1.0f));
        reference.indices = {0, 1, 2};
        reference.bounds = SbBox3f(
            SbVec3f(0.0f, 0.0f, 0.0f),
            SbVec3f(1.0f, 1.0f, 0.0f));

        Obol::TriMesh progressive;
        /* This triangle is counter-clockwise before three-bit PoP snapping
         * and clockwise afterwards.  Authored smooth normals remain the
         * shading authority: lossy displayed winding is neither stable nor a
         * reliable orientation signal for an unoriented source mesh. */
        progressive.positions = {
            SbVec3f(0.054196399f, 0.282882103f, 0.0f),
            SbVec3f(0.831061744f, 0.842560400f, 0.0f),
            SbVec3f(0.135469593f, 0.370431414f, 0.0f)};
        progressive.normals.assign(3, SbVec3f(0.0f, 0.0f, 1.0f));
        progressive.indices = {0, 1, 2};
        progressive.bounds = SbBox3f(
            SbVec3f(0.0f, 0.0f, 0.0f),
            SbVec3f(1.0f, 1.0f, 0.0f));
        progressive.progressiveMinimumCut = 3;
        progressive.progressiveResidentCut = 16;
        setProgressiveCuts(progressive, 17, 3u, 3u);
        for (size_t cut = progressive.progressiveMinimumCut;
                cut < progressive.progressiveCuts.size(); ++cut) {
            progressive.progressiveCuts[cut].quantization = cut == 16 ?
                Obol::ProgressiveQuantization{16, 16, 0} :
                Obol::ProgressiveQuantization{3, 3, 0};
        }
        progressive.progressiveQuantizationMinimum.setValue(
            0.0f, 0.0f, 0.0f);
        progressive.progressiveQuantizationMaximum.setValue(
            1.0f, 1.0f, 0.0f);

        const auto addPart = [assembly](const char *name,
                                        Obol::TriMesh mesh,
                                        float xOffset, uint8_t cut) {
            Obol::PartGeometryBuilder geometry;
            geometry.shaded = std::move(mesh);
            geometry.shadedCullBackfaces = true;
            const Obol::PartId part = Obol::CadIdBuilder::partId(name);
            requireCadMutation(admitAndUpsertPart(
                assembly, part, std::move(geometry)), name);
            Obol::InstanceRecord instance;
            instance.part = part;
            instance.parent = Obol::CadIdBuilder::rootInstance();
            instance.childName = name;
            instance.lodCut = cut;
            instance.localToRoot.setTranslate(
                SbVec3f(xOffset, -0.5f, 0.0f));
            instance.style.hasColorOverride = true;
            instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            requireCadMutation(assembly->upsertInstanceAuto(instance), name);
        };
        addPart("display-normal-reference", std::move(reference), -1.0f,
            Obol::ProgressiveCutUnspecified);
        addPart("display-normal-progressive", std::move(progressive), 0.0f,
            3);

        SoOffscreenRenderer renderer(SbViewportRegion(256, 192));
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered)
            result.image = foregroundHalfStats(renderer);
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false);
    const RouteResult glsl = renderRoute(true);
    restoreGlslEnvironment();
    const auto routeMatched = [](const RouteResult& result) {
        const double low = (std::min)(
            result.image.leftMean, result.image.rightMean);
        const double high = (std::max)(
            result.image.leftMean, result.image.rightMean);
        return result.rendered && result.image.leftPixels > 100 &&
            result.image.rightPixels > 100 && high > 0.0 &&
            low / high >= 0.85;
    };
    const bool matched = routeMatched(fixed) && routeMatched(glsl);
    if (!matched) {
        std::fprintf(stderr,
            "display-normal stats fixed={left=%.3f/%zu right=%.3f/%zu} "
            "glsl={left=%.3f/%zu right=%.3f/%zu}\n",
            fixed.image.leftMean, fixed.image.leftPixels,
            fixed.image.rightMean, fixed.image.rightPixels,
            glsl.image.leftMean, glsl.image.leftPixels,
            glsl.image.rightMean, glsl.image.rightPixels);
    }
    return matched;
}

bool
transformedSpotlightStateAffectsBothPipelines()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        HalfImageStats image;
    };
    const auto renderRoute = [](bool softwareGlsl) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(2.0f);
        root->addChild(camera);

        SoEnvironment *environment = new SoEnvironment;
        environment->ambientIntensity.setValue(0.0f);
        // SoEnvironment stores quadratic, linear, then constant.
        environment->attenuation.setValue(0.5f, 0.0f, 0.5f);
        root->addChild(environment);

        // Capture a translated light matrix, then reset the model transform
        // before the CAD assembly.  Reading the light's raw fields would
        // incorrectly leave this spotlight centered between the two panels.
        SoTransform *lightTransform = new SoTransform;
        lightTransform->translation.setValue(-0.625f, 0.0f, 0.0f);
        root->addChild(lightTransform);
        SoSpotLight *light = new SoSpotLight;
        light->location.setValue(0.0f, 0.0f, 1.0f);
        light->direction.setValue(0.0f, 0.0f, -1.0f);
        light->cutOffAngle.setValue(1.3f);
        light->dropOffRate.setValue(0.01f);
        root->addChild(light);
        root->addChild(new SoResetTransform);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        Obol::TriMesh mesh;
        /* Fixed-function OpenGL evaluates positional and spot lighting per
         * vertex, while the CAD GLSL route evaluates it per fragment.  Use a
         * modest grid so this contract measures transformed light state and
         * attenuation instead of a backend's Gouraud interpolation error. */
        const auto appendPanel = [&mesh](float minimumX, float maximumX) {
            constexpr uint32_t columns = 8;
            constexpr uint32_t rows = 8;
            const uint32_t base = static_cast<uint32_t>(
                mesh.positions.size());
            for (uint32_t row = 0; row <= rows; ++row) {
                const float y = -0.6f + 1.2f *
                    static_cast<float>(row) / static_cast<float>(rows);
                for (uint32_t column = 0; column <= columns; ++column) {
                    const float x = minimumX + (maximumX - minimumX) *
                        static_cast<float>(column) /
                        static_cast<float>(columns);
                    mesh.positions.emplace_back(x, y, 0.0f);
                }
            }
            for (uint32_t row = 0; row < rows; ++row) {
                for (uint32_t column = 0; column < columns; ++column) {
                    const uint32_t lowerLeft = base +
                        row * (columns + 1u) + column;
                    const uint32_t lowerRight = lowerLeft + 1u;
                    const uint32_t upperLeft =
                        lowerLeft + columns + 1u;
                    const uint32_t upperRight = upperLeft + 1u;
                    mesh.indices.insert(mesh.indices.end(), {
                        lowerLeft, lowerRight, upperRight,
                        lowerLeft, upperRight, upperLeft});
                }
            }
        };
        appendPanel(-1.0f, -0.25f);
        appendPanel(0.25f, 1.0f);
        mesh.normals.assign(mesh.positions.size(),
            SbVec3f(0.0f, 0.0f, 1.0f));
        mesh.bounds.makeEmpty();
        for (const SbVec3f& point : mesh.positions)
            mesh.bounds.extendBy(point);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = std::move(mesh);
        const Obol::PartId part =
            Obol::CadIdBuilder::partId("transformed-spotlight-state");
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "transformed spotlight part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "transformed-spotlight-state";
        instance.localToRoot.makeIdentity();
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "transformed spotlight instance");

        SoOffscreenRenderer renderer(SbViewportRegion(256, 192));
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered)
            result.image = foregroundHalfStats(renderer);
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false);
    const RouteResult glsl = renderRoute(true);
    restoreGlslEnvironment();
    const auto hasExpectedFalloff = [](const RouteResult& result) {
        return result.rendered && result.image.leftPixels > 500 &&
            result.image.rightPixels > 500 && result.image.rightMean > 0.0 &&
            result.image.leftMean / result.image.rightMean > 1.5;
    };
    /* The fixed path evaluates spot attenuation at vertices and interpolates
     * the resulting colour, while GLSL evaluates it per fragment.  Their
     * exact gradients are intentionally different; the shared contract is
     * that both consume the transformed spotlight and put the strong falloff
     * on the same side of the image. */
    const bool matched =
        hasExpectedFalloff(fixed) && hasExpectedFalloff(glsl);
    if (!matched)
        std::fprintf(stderr,
            "transformed spotlight stats fixed={left=%.3f/%zu "
            "right=%.3f/%zu} glsl={left=%.3f/%zu right=%.3f/%zu}\n",
            fixed.image.leftMean, fixed.image.leftPixels,
            fixed.image.rightMean, fixed.image.rightPixels,
            glsl.image.leftMean, glsl.image.leftPixels,
            glsl.image.rightMean, glsl.image.rightPixels);
    return matched;
}

bool
indirectProgressiveAtlasGrows(bool ceilingOnly)
{
    constexpr int partCount = 128;
    constexpr int trianglesPerPart = 300;

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(110.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions.reserve(trianglesPerPart * 3u);
    mesh.indices.reserve(trianglesPerPart * 3u);
    mesh.bounds.makeEmpty();
    for (int triangle = 0; triangle < trianglesPerPart; ++triangle) {
        const float x = -0.45f +
            0.06f * static_cast<float>(triangle % 15);
        const float y = -0.45f +
            0.045f * static_cast<float>(triangle / 15);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.05f, y, 0.0f),
            SbVec3f(x + 0.025f, y + 0.04f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(mesh.indices.size());
    mesh.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(mesh.positions.size());
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    std::vector<Obol::InstanceLodUpdate> richCuts;
    richCuts.reserve(partCount);
    for (int i = 0; i < partCount; ++i) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "progressive-atlas-growth-%03d", i);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "progressive atlas part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(i);
        instance.localToRoot.setTranslate(SbVec3f(
            -44.0f + 8.0f * static_cast<float>(i % 12),
            -44.0f + 8.0f * static_cast<float>(i / 12),
            0.0f));
        instance.lodCut = ceilingOnly ? 15 : 0;
        const Obol::InstanceId id =
            requireCadValue(assembly->upsertInstanceAuto(instance),
                "progressive atlas instance").instance;
        richCuts.push_back({id, 15});
    }

    const auto presentCut = [&](bool rich) {
        if (ceilingOnly) {
            cadViewState(root)->progressiveCutCeiling.setValue(
                rich ? mesh.progressiveResidentCut : mesh.progressiveMinimumCut);
        } else {
            std::vector<Obol::InstanceLodUpdate> cuts = richCuts;
            if (!rich) {
                for (Obol::InstanceLodUpdate& update : cuts)
                    update.lodCut = mesh.progressiveMinimumCut;
            }
            requireCadMutation(assembly->updateInstanceCuts(cuts),
                "progressive atlas presentation cuts");
        }
    };
    presentCut(false);

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    const bool coarseRendered = render(renderer, root);
    const int coarseTier = assembly->lastRenderTier();
    const uint64_t coarseTriangles =
        assembly->lastRenderedTriangleCount();

    /*
     * A backend without MDI cannot exercise this GPU-residency contract.
     * Its ordinary progressive path is covered separately.
     */
    bool passed = coarseRendered;
    if (passed && coarseTier == 6) {
        presentCut(true);
        const bool richRendered = render(renderer, root);
        const uint64_t richTriangles =
            assembly->lastRenderedTriangleCount();
        const uint64_t expectedRich =
            static_cast<uint64_t>(partCount) * trianglesPerPart;
        const Obol::CadGpuResourceSnapshot resources =
            assembly->gpuResourceSnapshot();
        passed = richRendered && assembly->lastRenderTier() == 6 &&
            coarseTriangles == static_cast<uint64_t>(partCount) &&
            richTriangles == expectedRich && resources.frameSerial > 0 &&
            resources.triangleAtlasAllocatedBytes > 0 &&
            resources.triangleAtlasLiveBytes > 0 &&
            resources.triangleAtlasLiveBytes <=
                resources.triangleAtlasAllocatedBytes &&
            resources.triangleAtlasPartCount == partCount &&
            resources.triangleAtlasPageCount > 0 &&
            resources.trackedBufferBytes >=
                resources.triangleAtlasAllocatedBytes;
        if (passed) {
            /* A resident prefix can change presentation without changing its
             * immutable generation or uploading geometry.  Prove that the
             * retained commands restore the rich image, not just its counters. */
            const size_t imageBytes = static_cast<size_t>(
                viewport.getViewportSizePixels()[0]) *
                viewport.getViewportSizePixels()[1] * 3u;
            const std::vector<unsigned char> richImage(
                renderer.getBuffer(), renderer.getBuffer() + imageBytes);
            passed = nonBlackPixels(renderer) > 0;
            presentCut(false);
            passed = render(renderer, root) && passed &&
                assembly->lastRenderTier() == 6 &&
                assembly->lastRenderedTriangleCount() == coarseTriangles &&
                std::memcmp(renderer.getBuffer(), richImage.data(),
                    imageBytes) != 0;
            presentCut(true);
            passed = render(renderer, root) && passed &&
                assembly->lastRenderTier() == 6 &&
                assembly->lastRenderedTriangleCount() == expectedRich &&
                std::memcmp(renderer.getBuffer(), richImage.data(),
                    imageBytes) == 0;
            const Obol::CadGpuResourceSnapshot restored =
                assembly->gpuResourceSnapshot();
            passed = passed && restored.frameSerial > resources.frameSerial &&
                restored.triangleAtlasFullUploadBytes ==
                    resources.triangleAtlasFullUploadBytes &&
                restored.triangleAtlasSuffixUploadBytes ==
                    resources.triangleAtlasSuffixUploadBytes &&
                restored.triangleAtlasLineageReuseCount ==
                    resources.triangleAtlasLineageReuseCount;
        }
        if (!passed) {
            std::fprintf(stderr,
                "indirect progressive atlas did not grow "
                "(tier=%d coarse=%llu rich=%llu expected=%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(coarseTriangles),
                static_cast<unsigned long long>(richTriangles),
                static_cast<unsigned long long>(expectedRich));
        }
    }

    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    root->unref();
    return passed;
}

bool
indirectProgressiveGenerationAppendsSuffix()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 128u;
    constexpr uint64_t lineage = UINT64_C(0x7155465847454e31);
    Obol::TriMesh rich;
    rich.positions.reserve(triangleCount * 3u);
    rich.indices.reserve(triangleCount * 3u);
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.6f +
            0.2f * static_cast<float>(triangle % 16u);
        const float y = -1.6f +
            0.2f * static_cast<float>(triangle / 16u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.16f, y, 0.0f),
            SbVec3f(x + 0.08f, y + 0.16f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    rich.progressiveLineage = lineage;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("progressive-generation-suffix");
    std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
        new Obol::PartGeometryBuilder);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
            "progressive suffix admission").geometry, false}}),
        "progressive suffix part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "progressive-generation-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "progressive suffix instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int initialTier = assembly->lastRenderTier();
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed && initialTier == 6) {
        std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
            new Obol::PartGeometryBuilder);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
                "rich suffix admission").geometry, true}}),
            "rich suffix part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot richResources =
            assembly->gpuResourceSnapshot();
        const uint64_t expectedSuffixBytes =
            static_cast<uint64_t>(triangleCount * 3u - 3u) *
                (3u * sizeof(float) + sizeof(uint32_t));
        passed = passed && assembly->lastRenderTier() == 6 &&
            assembly->lastRenderedTriangleCount() == triangleCount &&
            richResources.triangleAtlasFullUploadBytes ==
                coarseResources.triangleAtlasFullUploadBytes &&
            richResources.triangleAtlasSuffixUploadBytes >=
                coarseResources.triangleAtlasSuffixUploadBytes +
                    expectedSuffixBytes &&
            richResources.triangleAtlasLineageReuseCount >
                coarseResources.triangleAtlasLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "progressive immutable generation did not reuse atlas "
                "prefix (tier=%d triangles=%llu full=%llu/%llu "
                "suffix=%llu/%llu reuse=%llu/%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasSuffixUploadBytes),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasSuffixUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasLineageReuseCount),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasLineageReuseCount));
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

bool
ordinaryProgressiveGenerationAppendsSuffix()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 128u;
    constexpr uint64_t lineage = UINT64_C(0x4f5244494e415259);
    Obol::TriMesh rich;
    rich.positions.reserve(triangleCount * 3u);
    rich.indices.reserve(triangleCount * 3u);
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.6f +
            0.2f * static_cast<float>(triangle % 16u);
        const float y = -1.6f +
            0.2f * static_cast<float>(triangle / 16u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.16f, y, 0.0f),
            SbVec3f(x + 0.08f, y + 0.16f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    rich.progressiveLineage = lineage;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;
    Obol::TriMesh contracted = coarse;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("ordinary-progressive-suffix");
    std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
        new Obol::PartGeometryBuilder);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
            "ordinary suffix admission").geometry, false}}),
        "ordinary suffix part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "ordinary-progressive-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "ordinary suffix instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
            new Obol::PartGeometryBuilder);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
                "ordinary rich admission").geometry, true}}),
            "ordinary rich part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot richResources =
            assembly->gpuResourceSnapshot();
        const uint64_t expectedSuffixBytes =
            static_cast<uint64_t>(triangleCount * 3u - 3u) *
                (3u * sizeof(float) + sizeof(uint32_t));
        const uint64_t expectedCopiedBytes =
            3u * (3u * sizeof(float) + sizeof(uint32_t));
        const uint64_t expectedCompleteBytes =
            static_cast<uint64_t>(triangleCount) * 3u *
                (3u * sizeof(float) + sizeof(uint32_t));
        const bool copiedPrefix =
            richResources.ordinaryPartFullUploadBytes ==
                coarseResources.ordinaryPartFullUploadBytes &&
            richResources.ordinaryPartSuffixUploadBytes >=
                coarseResources.ordinaryPartSuffixUploadBytes +
                    expectedSuffixBytes &&
            richResources.ordinaryPartGpuCopyBytes >=
                coarseResources.ordinaryPartGpuCopyBytes +
                    expectedCopiedBytes;
        /* GL 3.1/ARB_copy_buffer preserves the old device prefix and uploads
         * only the suffix.  Legacy software contexts do not expose that
         * operation; their conservative, defined fallback is one complete
         * upload.  Lineage reuse must still be recognized in both cases so a
         * capable later generation/context may take the fast path. */
        const bool completeUploadFallback =
            richResources.ordinaryPartFullUploadBytes >=
                coarseResources.ordinaryPartFullUploadBytes +
                    expectedCompleteBytes &&
            richResources.ordinaryPartSuffixUploadBytes ==
                coarseResources.ordinaryPartSuffixUploadBytes &&
            richResources.ordinaryPartGpuCopyBytes ==
                coarseResources.ordinaryPartGpuCopyBytes;
        passed = passed && assembly->lastRenderTier() != 6 &&
            assembly->lastRenderedTriangleCount() == triangleCount &&
            (copiedPrefix || completeUploadFallback) &&
            richResources.ordinaryPartLineageReuseCount >
                coarseResources.ordinaryPartLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "ordinary progressive generation did not copy/reuse prefix "
                "(tier=%d triangles=%llu full=%llu/%llu suffix=%llu/%llu "
                "copy=%llu/%llu reuse=%llu/%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartSuffixUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartSuffixUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartGpuCopyBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartGpuCopyBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartLineageReuseCount),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartLineageReuseCount));
        }
        if (passed) {
            std::shared_ptr<Obol::PartGeometryBuilder> contractedGeometry(
                new Obol::PartGeometryBuilder);
            contractedGeometry->shaded = std::move(contracted);
            contractedGeometry->conservativeBounds =
                coarseGeometry->conservativeBounds;
            requireCadMutation(assembly->upsertParts({{part,
                requireCadValue(
                    Obol::cadAdmitPartGeometry(*contractedGeometry),
                    "ordinary contraction admission").geometry, true}}),
                "ordinary contraction part");
            passed = render(renderer, root);
            const Obol::CadGpuResourceSnapshot contractedResources =
                assembly->gpuResourceSnapshot();
            passed = passed &&
                assembly->lastRenderedTriangleCount() == 1u &&
                contractedResources.ordinaryPartBufferBytes ==
                    richResources.ordinaryPartBufferBytes &&
                contractedResources.ordinaryPartFullUploadBytes ==
                    richResources.ordinaryPartFullUploadBytes &&
                contractedResources.ordinaryPartSuffixUploadBytes ==
                    richResources.ordinaryPartSuffixUploadBytes &&
                contractedResources.ordinaryPartGpuCopyBytes ==
                    richResources.ordinaryPartGpuCopyBytes &&
                contractedResources.ordinaryPartLineageReuseCount >
                    richResources.ordinaryPartLineageReuseCount;
            if (!passed) {
                std::fprintf(stderr,
                    "ordinary progressive contraction did not retain its "
                    "certified GPU superset (triangles=%llu bytes=%zu/%zu "
                    "full=%llu/%llu suffix=%llu/%llu copy=%llu/%llu "
                    "reuse=%llu/%llu)\n",
                    static_cast<unsigned long long>(
                        assembly->lastRenderedTriangleCount()),
                    richResources.ordinaryPartBufferBytes,
                    contractedResources.ordinaryPartBufferBytes,
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartFullUploadBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartFullUploadBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartSuffixUploadBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartSuffixUploadBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartGpuCopyBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartGpuCopyBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartLineageReuseCount),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartLineageReuseCount));
            }
            if (passed) {
                Obol::TriMesh replacement =
                    *contractedGeometry->shaded;
                replacement.progressiveLineage = lineage + 1u;
                std::shared_ptr<Obol::PartGeometryBuilder> replacementGeometry(
                    new Obol::PartGeometryBuilder);
                replacementGeometry->shaded = std::move(replacement);
                replacementGeometry->conservativeBounds =
                    contractedGeometry->conservativeBounds;
                requireCadMutation(assembly->upsertParts({{part,
                    requireCadValue(
                        Obol::cadAdmitPartGeometry(*replacementGeometry),
                        "ordinary replacement admission").geometry, true}}),
                    "ordinary replacement part");
                passed = render(renderer, root);
                const Obol::CadGpuResourceSnapshot replacementResources =
                    assembly->gpuResourceSnapshot();
                const uint64_t oneTriangleBytes =
                    3u * (3u * sizeof(float) + sizeof(uint32_t));
                passed = passed &&
                    assembly->lastRenderedTriangleCount() == 1u &&
                    replacementResources.ordinaryPartFullUploadBytes ==
                        contractedResources.ordinaryPartFullUploadBytes +
                            oneTriangleBytes &&
                    replacementResources.
                        ordinaryPartLineageReplacementCount >
                    contractedResources.
                        ordinaryPartLineageReplacementCount;
                if (!passed) {
                    std::fprintf(stderr,
                        "ordinary progressive lineage replacement was not "
                        "explicitly accounted (triangles=%llu full=%llu/%llu "
                        "replacement=%llu/%llu)\n",
                        static_cast<unsigned long long>(
                            assembly->lastRenderedTriangleCount()),
                        static_cast<unsigned long long>(
                            contractedResources.
                                ordinaryPartFullUploadBytes),
                        static_cast<unsigned long long>(
                            replacementResources.
                                ordinaryPartFullUploadBytes),
                        static_cast<unsigned long long>(
                            contractedResources.
                                ordinaryPartLineageReplacementCount),
                        static_cast<unsigned long long>(
                            replacementResources.
                                ordinaryPartLineageReplacementCount));
                }
            }
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

bool
ordinaryProgressiveZeroLineageReplacesWithoutOverread()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh rich;
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < 64u; ++triangle) {
        const float x = -1.5f + 0.35f * static_cast<float>(triangle % 8u);
        const float y = -1.5f + 0.35f * static_cast<float>(triangle / 8u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.25f, y, 0.0f),
            SbVec3f(x + 0.12f, y + 0.25f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    /* Zero explicitly means no cross-generation prefix identity. */
    rich.progressiveLineage = 0;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("ordinary-zero-lineage-replacement");
    std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
        new Obol::PartGeometryBuilder);
    richGeometry->shaded = std::move(rich);
    richGeometry->conservativeBounds = richGeometry->shaded->bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
            "zero-lineage rich admission").geometry, false}}),
        "zero-lineage rich part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "ordinary-zero-lineage-replacement";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "zero-lineage instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root) &&
        assembly->lastRenderedTriangleCount() == 64u;
    const Obol::CadGpuResourceSnapshot richResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
            new Obol::PartGeometryBuilder);
        coarseGeometry->shaded = std::move(coarse);
        coarseGeometry->conservativeBounds =
            richGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
                "zero-lineage coarse admission").geometry, true}}),
            "zero-lineage coarse part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot coarseResources =
            assembly->gpuResourceSnapshot();
        const uint64_t oneTriangleBytes =
            3u * (3u * sizeof(float) + sizeof(uint32_t));
        passed = passed && assembly->lastRenderedTriangleCount() == 1u &&
            coarseResources.ordinaryPartFullUploadBytes ==
                richResources.ordinaryPartFullUploadBytes +
                    oneTriangleBytes &&
            coarseResources.ordinaryPartLineageReuseCount ==
                richResources.ordinaryPartLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "zero-lineage progressive replacement retained stale "
                "CPU counts (triangles=%llu full=%llu/%llu reuse=%llu/%llu)\n",
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartLineageReuseCount),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartLineageReuseCount));
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT", previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

struct DeadlineAbortCounter {
    size_t calls = 0;
    size_t abortAt = static_cast<size_t>(-1);
};

SoGLRenderAction::AbortCode
deadlineAbortCounter(void *userData)
{
    DeadlineAbortCounter *counter =
        static_cast<DeadlineAbortCounter *>(userData);
    if (!counter)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return counter->calls >= counter->abortAt ?
        SoGLRenderAction::ABORT : SoGLRenderAction::CONTINUE;
}

struct DeadlineAssemblyAbort {
    SoCADAssembly *assembly = nullptr;
    uint64_t executionSerial = 0;
    size_t calls = 0;
    size_t abortAt = 10;
};

struct DeadlinePreparationAbort {
    SoCADAssembly *assembly = nullptr;
    size_t calls = 0u;
};

SoGLRenderAction::AbortCode
deadlineAbortPreparation(void *userData)
{
    DeadlinePreparationAbort *counter =
        static_cast<DeadlinePreparationAbort *>(userData);
    if (!counter || !counter->assembly ||
            counter->assembly->presentationPreparationSnapshot().state !=
                Obol::CadPresentationPreparationState::Preparing)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return SoGLRenderAction::ABORT;
}

bool
flatShadedPlanningResumesAcrossAborts()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(
                    setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t occurrenceCount = 2048u;
    constexpr uint32_t partCount = 128u;
    constexpr uint32_t instancesPerPart = occurrenceCount / partCount;
    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(-0.01f, -0.01f, 0.0f),
        SbVec3f(0.01f, -0.01f, 0.0f),
        SbVec3f(0.0f, 0.01f, 0.0f)
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = SbBox3f(
        SbVec3f(-0.01f, -0.01f, 0.0f),
        SbVec3f(0.01f, 0.01f, 0.0f));

    for (uint32_t partIndex = 0; partIndex < partCount; ++partIndex) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "flat-planning-resume-%03u", partIndex);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        geometry.shadedCullBackfaces = false;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "flat planning part");
        for (uint32_t offset = 0;
                offset < instancesPerPart; ++offset) {
            const uint32_t occurrence =
                partIndex * instancesPerPart + offset;
            Obol::InstanceRecord instance;
            instance.part = part;
            instance.parent = Obol::CadIdBuilder::rootInstance();
            instance.childName = name;
            instance.occurrenceIndex = occurrence;
            instance.localToRoot.setTranslate(SbVec3f(
                -1.5f + 0.05f * static_cast<float>(occurrence % 64u),
                -0.75f + 0.05f * static_cast<float>(occurrence / 64u),
                0.0f));
            requireCadMutation(assembly->upsertInstanceAuto(instance),
                "flat planning instance");
        }
    }

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    /* Compile and classify the occurrence stream without touching the
     * shaded executor.  Subsequent abort samples then belong to flat-shaded
     * preparation, not first-traversal scene setup. */
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    const bool planWarmed = render(renderer, root);
    setCadDrawMode(root, SoCADViewState::SHADED);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    bool passed = planWarmed && action != nullptr;
    Obol::CadPresentationPreparationTarget target;
    uint64_t priorCompleted = 0;
    constexpr size_t interruptedSlices = 3u;
    constexpr size_t initialPlanningSafePoint = 12u;
    constexpr size_t resumedPlanningSafePoint = 10u;
    for (size_t slice = 0; passed && slice < interruptedSlices; ++slice) {
        DeadlineAbortCounter interrupted;
        /* The first shaded traversal finishes the already-bounded
         * occurrence classifier before entering planning.  Later slices
         * start from its retained result and reach the renderer sooner. */
        interrupted.abortAt = slice == 0 ?
            initialPlanningSafePoint : resumedPlanningSafePoint;
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        const Obol::CadPresentationPreparationSnapshot snapshot =
            assembly->presentationPreparationSnapshot();
        passed = action->hasTerminated() &&
            interrupted.calls == interrupted.abortAt &&
            snapshot.target.kind ==
                Obol::CadPresentationPreparationKind::FlatShadedPlanning &&
            snapshot.state ==
                Obol::CadPresentationPreparationState::Preparing &&
            snapshot.totalUnits == occurrenceCount &&
            snapshot.completedUnits > priorCompleted &&
            snapshot.completedUnits < snapshot.totalUnits;
        if (slice == 0)
            target = snapshot.target;
        else
            passed = passed && snapshot.target == target;
        priorCompleted = snapshot.completedUnits;
    }
    if (action)
        action->setAbortCallback(previousCallback, previousData);
    passed = passed && render(renderer, root) &&
        assembly->lastRenderTier() == 4 &&
        assembly->lastRenderedTriangleCount() == occurrenceCount;
    const Obol::CadPresentationPreparationSnapshot completed =
        assembly->presentationPreparationSnapshot();
    passed = passed && completed.target.kind ==
            Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
        completed.state ==
            Obol::CadPresentationPreparationState::Complete &&
        completed.totalUnits == occurrenceCount &&
        completed.completedUnits == completed.totalUnits;
    if (!passed) {
        std::fprintf(stderr,
            "flat planning did not resume with an immutable certificate "
            "(tier=%d planning=%llu/%llu final-kind=%u final=%llu/%llu)\n",
            assembly->lastRenderTier(),
            static_cast<unsigned long long>(priorCompleted),
            static_cast<unsigned long long>(occurrenceCount),
            static_cast<unsigned>(completed.target.kind),
            static_cast<unsigned long long>(completed.completedUnits),
            static_cast<unsigned long long>(completed.totalUnits));
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
flatShadedAtlasMakesProgressInsideLargeSourceRange()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(
                    setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 32u * 1024u;
    Obol::TriMesh mesh;
    mesh.positions.reserve(triangleCount * 3u);
    mesh.indices.reserve(triangleCount * 3u);
    mesh.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.5f +
            0.012f * static_cast<float>(triangle % 256u);
        const float y = -0.75f +
            0.012f * static_cast<float>((triangle / 256u) % 128u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.008f, y, 0.0f),
            SbVec3f(x + 0.004f, y + 0.007f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    Obol::PartGeometryBuilder geometry;
    geometry.shaded = mesh;
    geometry.shadedCullBackfaces = false;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("flat-large-range");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "flat large-range part");
    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "flat-large-range";
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "flat large-range instance");

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    setCadDrawMode(root, SoCADViewState::HIDDEN_LINE);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);
    else
        passed = false;

    DeadlineAbortCounter interrupted;
    constexpr size_t atlasPartialProgressSafePoint = 64u;
    interrupted.abortAt = atlasPartialProgressSafePoint;
    if (passed) {
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        const Obol::CadPresentationPreparationSnapshot partial =
            assembly->presentationPreparationSnapshot();
        passed = action->hasTerminated() &&
            partial.target.kind ==
                Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
            partial.state ==
                Obol::CadPresentationPreparationState::Preparing &&
            partial.totalUnits > 1u && partial.completedUnits > 0u &&
            partial.completedUnits < partial.totalUnits;
    }
    if (action)
        action->setAbortCallback(previousCallback, previousData);
    passed = passed && render(renderer, root) &&
        assembly->lastRenderTier() == 3 &&
        assembly->lastRenderedTriangleCount() == triangleCount;
    const Obol::CadPresentationPreparationSnapshot completed =
        assembly->presentationPreparationSnapshot();
    passed = passed && completed.target.kind ==
            Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
        completed.state ==
            Obol::CadPresentationPreparationState::Complete &&
        completed.completedUnits == completed.totalUnits;
    if (!passed) {
        std::fprintf(stderr,
            "flat atlas did not commit progress inside a large source "
            "range (tier=%d abort-calls=%zu state=%u units=%llu/%llu)\n",
            assembly->lastRenderTier(), interrupted.calls,
            static_cast<unsigned>(completed.state),
            static_cast<unsigned long long>(completed.completedUnits),
            static_cast<unsigned long long>(completed.totalUnits));
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
progressiveReplacementTombstoneKeepsActiveIndex()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(8.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(-0.4f, -0.3f, 0.0f),
        SbVec3f(0.4f, -0.3f, 0.0f),
        SbVec3f(0.0f, 0.4f, 0.0f)
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = SbBox3f(
        SbVec3f(-0.4f, -0.3f, 0.0f),
        SbVec3f(0.4f, 0.4f, 0.0f));
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    Obol::PartGeometryBuilder geometry;
    geometry.shaded = mesh;
    geometry.shadedCullBackfaces = false;
    const Obol::PartId sharedPart =
        Obol::CadIdBuilder::partId("tombstone-shared-progressive");
    const Obol::PartId replacementPart =
        Obol::CadIdBuilder::partId("tombstone-replacement-progressive");
    requireCadMutation(admitAndUpsertPart(assembly, sharedPart, geometry),
        "shared progressive part");
    requireCadMutation(
        admitAndUpsertPart(assembly, replacementPart, geometry),
        "replacement progressive part");

    constexpr size_t occurrenceCount = 4u;
    std::array<Obol::InstanceId, occurrenceCount> instances;
    std::array<Obol::InstanceRecord, occurrenceCount> records;
    for (size_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord& record = records[index];
        record.part = sharedPart;
        record.parent = Obol::CadIdBuilder::rootInstance();
        record.childName = "tombstone-shared-occurrence";
        record.occurrenceIndex = static_cast<uint32_t>(index);
        record.lodCut = 0;
        record.localToRoot.setTranslate(SbVec3f(
            -2.25f + 1.5f * static_cast<float>(index), 0.0f, 0.0f));
        instances[index] = requireCadValue(
            assembly->upsertInstanceAuto(record),
            "shared progressive instance").instance;
    }

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const uint64_t compiledPlans = assembly->framePlanBuildCount();

    /* Rebinding the final shared occurrence cannot reuse its old part slot,
     * so the sparse path hides that compiled occurrence and appends its
     * active replacement.  Moving the first peer to another cut then swaps
     * it with the hidden final tombstone.  The tombstone and replacement
     * share an InstanceId; this is the exact sequence which previously made
     * the stale slot overwrite the active ID-to-index mapping. */
    records.back().part = replacementPart;
    Obol::InstanceUpdate rebind;
    rebind.instance = instances.back();
    rebind.record = records.back();
    passed = passed && assembly->upsertInstances({rebind});

    Obol::InstanceLodUpdate firstCut;
    firstCut.instance = instances.front();
    firstCut.lodCut = 15;
    passed = passed && assembly->updateInstanceCuts({firstCut});
    Obol::InstanceLodUpdate replacementCut;
    replacementCut.instance = instances.back();
    replacementCut.lodCut = 15;
    passed = passed && assembly->updateInstanceCuts({replacementCut});
    passed = passed && render(renderer, root) &&
        assembly->framePlanBuildCount() == compiledPlans &&
        assembly->lastRenderedTriangleCount() == occurrenceCount;
    if (!passed) {
        std::fprintf(stderr,
            "progressive replacement tombstone stole the active index "
            "(plan-builds=%llu/%llu triangles=%llu)\n",
            static_cast<unsigned long long>(compiledPlans),
            static_cast<unsigned long long>(
                assembly->framePlanBuildCount()),
            static_cast<unsigned long long>(
                assembly->lastRenderedTriangleCount()));
    }

    root->unref();
    return passed;
}

SoGLRenderAction::AbortCode
deadlineAbortAssemblyWork(void *userData)
{
    DeadlineAssemblyAbort *counter =
        static_cast<DeadlineAssemblyAbort *>(userData);
    if (!counter || !counter->assembly ||
            counter->assembly->renderExecutionSerial() ==
                counter->executionSerial)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return counter->calls >= counter->abortAt ?
        SoGLRenderAction::ABORT : SoGLRenderAction::CONTINUE;
}

bool
subpixelPreparationReservationCoversBoundedScratch()
{
    constexpr uint32_t occurrenceCount = 131072u;
    constexpr uint32_t gridWidth = 512u;
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(2048.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("bounded-subpixel-preparation");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "flattened wire part");

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "bounded-subpixel-preparation";
        instance.occurrenceIndex = index;
        instance.lodStructuralProxy = true;
        instance.localToRoot.setTranslate(SbVec3f(
            static_cast<float>(index % gridWidth) -
                static_cast<float>(gridWidth) * 0.5f,
            static_cast<float>(index / gridWidth) -
                static_cast<float>(occurrenceCount / gridWidth) * 0.5f,
            0.0f));
        Obol::InstanceUpdate update;
        update.instance = Obol::CadIdBuilder::childInstance(
            instance.parent, instance.childName,
            instance.occurrenceIndex, instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    requireCadMutation(assembly->upsertInstances(updates),
        "flattened wire instances");

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    DeadlinePreparationAbort interrupted;
    interrupted.assembly = assembly;
    if (action)
        action->setAbortCallback(deadlineAbortPreparation, &interrupted);
    (void)renderer.render(root);
    const Obol::CadPresentationPreparationSnapshot preparing =
        assembly->presentationPreparationSnapshot();
    if (action)
        action->setAbortCallback(previousCallback, previousData);

    /* These are only the fixed occurrence-indexed buffers.  Point records
     * and their reverse index make the real reservation larger.  Requiring
     * this lower bound catches any return to unaccounted node-based scratch
     * while remaining independent of std::vector growth policy. */
    const uint64_t fixedOccurrenceBytes =
        sizeof(uint8_t) + sizeof(uint32_t) + sizeof(int8_t) +
        sizeof(Obol::InstanceId);
    const uint64_t minimumReservation =
        static_cast<uint64_t>(occurrenceCount) * fixedOccurrenceBytes +
        sizeof(uint8_t) + sizeof(size_t);
    const bool preparationInterrupted = action &&
        action->hasTerminated() && interrupted.calls == 1u;
    bool passed = preparationInterrupted &&
        preparing.target.kind == Obol::CadPresentationPreparationKind::
            SubpixelClassification &&
        preparing.state == Obol::CadPresentationPreparationState::Preparing &&
        preparing.completedUnits > 0u &&
        preparing.completedUnits < preparing.totalUnits &&
        preparing.reservedBytes >= minimumReservation;

    SoOffscreenRenderer recoveryRenderer(SbViewportRegion(128, 128));
    recoveryRenderer.setComponents(SoOffscreenRenderer::RGB);
    passed = passed && render(recoveryRenderer, root) &&
        assembly->lastSubpixelProxyCount() == occurrenceCount &&
        assembly->lastUncollapsedStructuralProxyCount() == 0u;
    if (!passed) {
        std::fprintf(stderr,
            "bounded classifier reservation failed "
            "(aborted=%d state=%u completed=%llu/%llu reserved=%llu "
            "minimum=%llu proxies=%zu boxes=%zu)\n",
            preparationInterrupted ? 1 : 0,
            static_cast<unsigned>(preparing.state),
            static_cast<unsigned long long>(preparing.completedUnits),
            static_cast<unsigned long long>(preparing.totalUnits),
            static_cast<unsigned long long>(preparing.reservedBytes),
            static_cast<unsigned long long>(minimumReservation),
            assembly->lastSubpixelProxyCount(),
            assembly->lastUncollapsedStructuralProxyCount());
    }
    root->unref();
    return passed;
}

bool
ordinaryExecutorHonorsAbortSafePoints()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    };
    EnvironmentSnapshot settings[] = {
        {"OBOL_CAD_FLAT_WIRE"},
        {"OBOL_CAD_FLAT_SHADED"},
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_REPLAY"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
        setTestEnvironment(setting.name, "0", 1);
    }
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(72.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = false;
    geometry.structuralProxy = false;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("deadline-shared-wire-part");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "software wire part");
    constexpr uint32_t instanceCount = 4096u;
    for (uint32_t index = 0; index < instanceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "deadline-shared-wire-instance";
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -31.5f + static_cast<float>(index % 64u),
            -31.5f + static_cast<float>(index / 64u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "software wire instance");
    }

    const SbViewportRegion viewport(128, 128);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    /* First build and then replay the retained plan so callback sampling in
     * the measurement below belongs to the renderer executor, not plan
     * construction. */
    bool passed = render(renderer, root) && render(renderer, root);
    const uint64_t planBuilds = assembly->framePlanBuildCount();
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    DeadlineAbortCounter observed;
    if (passed && action) {
        action->setAbortCallback(deadlineAbortCounter, &observed);
        passed = render(renderer, root) && !action->hasTerminated() &&
            assembly->framePlanBuildCount() == planBuilds &&
            observed.calls >= 10u;
    } else {
        passed = false;
    }

    DeadlineAbortCounter interrupted;
    interrupted.abortAt = (std::max)(size_t(4), observed.calls / 2u);
    const Obol::CadGpuResourceSnapshot beforeInterrupted =
        assembly->gpuResourceSnapshot();
    passed = passed && beforeInterrupted.frameSerial > 0 &&
        beforeInterrupted.trackedBufferBytes > 0 &&
        beforeInterrupted.ordinaryPartBufferBytes > 0;
    if (passed) {
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        passed = action->hasTerminated() &&
            interrupted.calls == interrupted.abortAt &&
            assembly->gpuResourceSnapshot().frameSerial ==
                beforeInterrupted.frameSerial;
    }

    if (action)
        action->setAbortCallback(previousCallback, previousData);
    if (passed) {
        passed = render(renderer, root) && nonBlackPixels(renderer) != 0u &&
            assembly->framePlanBuildCount() == planBuilds &&
            assembly->gpuResourceSnapshot().frameSerial >
                beforeInterrupted.frameSerial;
    }
    if (!passed) {
        std::fprintf(stderr,
            "ordinary CAD executor deadline contract failed "
            "(tier=%d observed=%zu abortAt=%zu abortedCalls=%zu)\n",
            assembly->lastRenderTier(), observed.calls,
            interrupted.abortAt, interrupted.calls);
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
indirectAtlasValidationResumesAcrossAborts()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"},
        {"OBOL_CAD_ATLAS_VALIDATION_FRAMES"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "0", 1);
    setTestEnvironment("OBOL_CAD_ATLAS_VALIDATION_FRAMES", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(40.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh triangle;
    triangle.positions = {
        SbVec3f(-0.4f, -0.4f, 0.0f),
        SbVec3f( 0.4f, -0.4f, 0.0f),
        SbVec3f( 0.0f,  0.4f, 0.0f)};
    triangle.indices = {0u, 1u, 2u};
    triangle.bounds = SbBox3f(
        SbVec3f(-0.4f, -0.4f, 0.0f),
        SbVec3f( 0.4f,  0.4f, 0.0f));

    /* More parts than one executor safe-point span ensures validation needs
     * several deadline-bounded traversals when the callback below aborts at
     * its second sample. */
    constexpr uint32_t partCount = 1024u;
    for (uint32_t index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "validation-resume-%04u", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = triangle;
        geometry.subpixelProxyEligible = false;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "atlas pressure part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -15.5f + static_cast<float>(index % 32u),
            -15.5f + static_cast<float>(index / 32u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "atlas pressure instance");
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    if (passed && assembly->lastRenderTier() == 6) {
        SoGLRenderAction *action = renderer.getGLRenderAction();
        SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
        void *previousData = nullptr;
        if (action)
            action->getAbortCallback(previousCallback, previousData);
        else
            passed = false;

        size_t preparationSlices = 0u;
        size_t interruptedAttempts = 0u;
        bool reachedSteadyDraw = false;
        constexpr size_t maximumAttempts = 12u;
        for (size_t attempt = 0;
                passed && attempt < maximumAttempts; ++attempt) {
            DeadlineAssemblyAbort interrupted;
            interrupted.assembly = assembly;
            interrupted.executionSerial =
                assembly->renderExecutionSerial();
            action->setAbortCallback(
                deadlineAbortAssemblyWork, &interrupted);
            const uint64_t preparationBefore =
                assembly->renderPreparationSerial();
            (void)renderer.render(root);
            if (action->hasTerminated())
                ++interruptedAttempts;
            if (assembly->renderPreparationSerial() ==
                    preparationBefore) {
                if (preparationSlices > 0u) {
                    reachedSteadyDraw = true;
                    break;
                }
                /* The one-frame audit countdown is itself steady replay.
                 * Continue once to enter the validation transaction. */
                continue;
            }
            ++preparationSlices;
        }
        if (action)
            action->setAbortCallback(previousCallback, previousData);
        passed = passed && interruptedAttempts >= 1u &&
            preparationSlices >= 2u && reachedSteadyDraw &&
            render(renderer, root) &&
            assembly->lastRenderTier() == 6 &&
            assembly->lastRenderedWork().exact;
        if (!passed)
            std::fprintf(stderr,
                "retained atlas validation did not converge across "
                "deadline slices (tier=%d aborts=%zu slices=%zu "
                "steady=%d)\n",
                assembly->lastRenderTier(), interruptedAttempts,
                preparationSlices, reachedSteadyDraw ? 1 : 0);
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
indirectProgressiveAtlasPreservesCoverageUnderPressure()
{
    constexpr int partCount = 192;
    constexpr int trianglesPerPart = 2000;

    struct EnvironmentSnapshot {
        const char *name;
        bool present;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT", false, std::string()},
        {"OBOL_CAD_ATLAS_MB", false, std::string()}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    /* One ordinary atlas page fits, two do not.  Every minimum prefix fits
     * comfortably, while all requested rich prefixes require the second
     * page. */
    setTestEnvironment("OBOL_CAD_ATLAS_MB", "20", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(150.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions.reserve(trianglesPerPart * 3u);
    mesh.indices.reserve(trianglesPerPart * 3u);
    mesh.bounds.makeEmpty();
    for (int triangle = 0; triangle < trianglesPerPart; ++triangle) {
        const float x = -0.48f +
            0.015f * static_cast<float>(triangle % 64);
        const float y = -0.48f +
            0.015f * static_cast<float>(triangle / 64);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.012f, y, 0.0f),
            SbVec3f(x + 0.006f, y + 0.010f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(mesh.indices.size());
    mesh.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(mesh.positions.size());
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    std::vector<Obol::InstanceLodUpdate> richCuts;
    richCuts.reserve(partCount);
    for (int index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "progressive-pressure-%03d", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        geometry.subpixelProxyEligible = true;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "coverage pressure part");
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(index);
        instance.localToRoot.setTranslate(SbVec3f(
            -60.0f + 7.0f * static_cast<float>(index % 18),
            -35.0f + 7.0f * static_cast<float>(index / 18), 0.0f));
        instance.lodCut = 0;
        const Obol::InstanceId id =
            requireCadValue(assembly->upsertInstanceAuto(instance),
                "coverage pressure instance").instance;
        richCuts.push_back({id, 15});
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int tier = assembly->lastRenderTier();
    if (passed && tier == 6) {
        requireCadMutation(assembly->updateInstanceCuts(richCuts),
            "coverage pressure cuts");
        passed = render(renderer, root);
        const uint64_t pressuredTriangles =
            assembly->lastRenderedTriangleCount();
        const uint64_t requestedTriangles =
            static_cast<uint64_t>(partCount) * trianglesPerPart;
        const Obol::CadGpuResourceSnapshot pressured =
            assembly->gpuResourceSnapshot();
        const uint64_t pressuredSerial = pressured.frameSerial;
        passed = passed && pressured.atlasAdmissionPressure &&
            pressured.pressureProxyCount == 0u &&
            pressured.triangleAtlasPartCount == partCount &&
            pressuredTriangles >= static_cast<uint64_t>(partCount) &&
            pressuredTriangles < requestedTriangles &&
            pressured.triangleAtlasAllocatedBytes <=
                pressured.triangleAtlasBudgetBytes;
        if (passed) {
            passed = render(renderer, root) &&
                assembly->lastRenderedTriangleCount() ==
                    pressuredTriangles;
            const Obol::CadGpuResourceSnapshot replayed =
                assembly->gpuResourceSnapshot();
            passed = passed && replayed.frameSerial > pressuredSerial &&
                replayed.atlasAdmissionPressure &&
                replayed.pressureProxyCount == 0u;
        }
        if (!passed) {
            std::fprintf(stderr,
                "coverage-first atlas pressure contract failed "
                "(tier=%d rendered=%llu requested=%llu parts=%zu "
                "proxies=%zu pressure=%d allocated=%zu budget=%zu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(pressuredTriangles),
                static_cast<unsigned long long>(requestedTriangles),
                pressured.triangleAtlasPartCount,
                pressured.pressureProxyCount,
                pressured.atlasAdmissionPressure ? 1 : 0,
                pressured.triangleAtlasAllocatedBytes,
                pressured.triangleAtlasBudgetBytes);
        }
    }

    root->unref();
    for (const EnvironmentSnapshot& setting : settings) {
        if (setting.present)
            setTestEnvironment(setting.name, setting.value.c_str(), 1);
        else
            unsetTestEnvironment(setting.name);
    }
    return passed;
}

bool
indirectPressureProxyPreservesProjectedExtent()
{
    constexpr uint32_t partCount = 128u;
    constexpr uint64_t boxTriangles =
        partCount * Obol::CadAggregateProxyBoxTriangleCount;
    constexpr uint64_t boxLines =
        partCount * Obol::CadAggregateProxyBoxLineCount;

    struct EnvironmentSnapshot {
        const char *name;
        bool present;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT", false, std::string()},
        {"OBOL_CAD_ATLAS_MB", false, std::string()},
        {"OBOL_CAD_FLAT_SHADED", false, std::string()}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    /* An ordinary atlas page is 16 MiB.  This limit deliberately prevents
     * even the minimum representation from being admitted. */
    setTestEnvironment("OBOL_CAD_ATLAS_MB", "1", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(24.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh tetrahedron;
    tetrahedron.positions = {
        SbVec3f(-0.5f, -0.5f, -0.5f),
        SbVec3f( 0.5f, -0.5f, -0.5f),
        SbVec3f( 0.0f,  0.5f, -0.5f),
        SbVec3f( 0.0f,  0.0f,  0.5f)
    };
    tetrahedron.indices = {
        0u, 2u, 1u,
        0u, 1u, 3u,
        1u, 2u, 3u,
        2u, 0u, 3u
    };
    tetrahedron.bounds = SbBox3f(
        SbVec3f(-0.5f, -0.5f, -0.5f),
        SbVec3f( 0.5f,  0.5f,  0.5f));

    for (uint32_t index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "pressure-proxy-%03u", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = tetrahedron;
        geometry.shadedCullBackfaces = false;
        geometry.subpixelProxyEligible = true;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "pressure proxy part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -15.0f + 2.0f * static_cast<float>(index % 16u),
            -7.0f + 2.0f * static_cast<float>(index / 16u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "pressure proxy instance");
    }

    const SbViewportRegion viewport(384, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    if (passed && assembly->lastRenderTier() == 6) {
        const Obol::CadGpuResourceSnapshot shaded =
            assembly->gpuResourceSnapshot();
        const Obol::CadRenderedWork shadedWork =
            assembly->lastRenderedWork();
        const Obol::CadAggregateProxyPresentationWork shadedProxies =
            assembly->lastAggregateProxyPresentationWork();
        const size_t shadedPixels = nonBlackPixels(renderer);
        const bool shadedPassed = shaded.atlasAdmissionPressure &&
            shaded.pressureProxyCount == partCount &&
            shadedProxies.exact && shadedProxies.pointCount == 0u &&
            shadedProxies.axisAlignedBoxCount == partCount &&
            shadedProxies.orientedBoxCount == 0u &&
            assembly->lastSubpixelProxyDrawPointCount() == 0u &&
            shadedWork.triangleCount == boxTriangles &&
            shadedWork.lineCount == 0u &&
            shadedPixels > 0u;

        setCadDrawMode(root, SoCADViewState::SHADED_WITH_EDGES);
        const bool edgedRendered = render(renderer, root);
        const Obol::CadRenderedWork edgedWork =
            assembly->lastRenderedWork();
        const bool edgedPassed = edgedRendered &&
            assembly->gpuResourceSnapshot().pressureProxyCount == partCount &&
            assembly->lastSubpixelProxyDrawPointCount() == 0u &&
            edgedWork.triangleCount == boxTriangles &&
            edgedWork.lineCount == boxLines;

        camera->height.setValue(1024.0f);
        setCadDrawMode(root, SoCADViewState::SHADED);
        const bool pointRendered = render(renderer, root);
        const Obol::CadRenderedWork pointWork =
            assembly->lastRenderedWork();
        const Obol::CadGpuResourceSnapshot pointSnapshot =
            assembly->gpuResourceSnapshot();
        /* Once every occurrence is genuinely point-sized, ordinary
         * view-local aggregation precedes atlas admission.  The pressure
         * fallback therefore has no remaining occurrences to replace. */
        const bool pointPassed = pointRendered &&
            pointSnapshot.pressureProxyCount == 0u &&
            assembly->lastSubpixelProxyDrawPointCount() == partCount &&
            pointWork.triangleCount == 0u && pointWork.lineCount == 0u;
        passed = shadedPassed && edgedPassed && pointPassed;

        if (!passed) {
            std::fprintf(stderr,
                "atlas-pressure proxy extent contract failed "
                "(pressure=%zu shaded=%llu/%llu edged=%llu/%llu "
                "points=%zu point-work=%llu/%llu "
                "stage=%d/%d/%d point-render=%d point-pressure=%zu "
                "pixels=%zu)\n",
                shaded.pressureProxyCount,
                static_cast<unsigned long long>(shadedWork.triangleCount),
                static_cast<unsigned long long>(shadedWork.lineCount),
                static_cast<unsigned long long>(edgedWork.triangleCount),
                static_cast<unsigned long long>(edgedWork.lineCount),
                assembly->lastSubpixelProxyDrawPointCount(),
                static_cast<unsigned long long>(pointWork.triangleCount),
                static_cast<unsigned long long>(pointWork.lineCount),
                shadedPassed ? 1 : 0, edgedPassed ? 1 : 0,
                pointPassed ? 1 : 0, pointRendered ? 1 : 0,
                pointSnapshot.pressureProxyCount, shadedPixels);
        }
    }

    root->unref();
    for (const EnvironmentSnapshot& setting : settings) {
        if (setting.present)
            setTestEnvironment(setting.name, setting.value.c_str(), 1);
        else
            unsetTestEnvironment(setting.name);
    }
    return passed;
}

} // namespace

int
runCadSubpixelProxyLifecycleContract()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part = Obol::CadIdBuilder::partId("subpixel-proxy");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "proxy lifecycle part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "proxy";
    instance.localToRoot.makeIdentity();
    instance.lodStructuralProxy = true;
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    const Obol::InstanceId proxyInstance =
        requireCadValue(assembly->upsertInstanceAuto(instance),
            "proxy lifecycle instance").instance;

    // An incorrectly bounded payload must not collapse: doing so could hide
    // visible geometry outside the point proxy's projected extent.
    Obol::PartGeometryBuilder malformed = geometry;
    malformed.wire->segmentPoints[0].setValue(-0.75f, -0.5f, -0.5f);
    const Obol::PartId malformedPart =
        Obol::CadIdBuilder::partId("malformed-subpixel-proxy");
    const Obol::CadGeometryValidation malformedResult =
        admitAndUpsertPart(assembly, malformedPart, malformed);
    if (malformedResult)
        throw std::runtime_error("malformed CAD geometry was accepted");
    if (malformedResult.error !=
            Obol::CadGeometryError::NonConservativeBounds)
        throw std::runtime_error("malformed CAD geometry had wrong error");
    instance.part = malformedPart;
    instance.childName = "malformed-proxy";
    instance.occurrenceIndex = 1;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "malformed-part reference instance");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    /* A coalesced occurrence which still covers a recognizable screen area
     * must retain its extent.  It remains one logical aggregate and one
     * batched line stream; it is not converted into twelve scene draw calls. */
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    camera->height.setValue(20.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastSubpixelProxyDrawPointCount() != 0u ||
            assembly->lastRenderedWork().lineCount != 12u ||
            assembly->lastRenderedWork().positionCount != 24u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "screen-significant aggregate did not use one batched box\n");
        root->unref();
        return 1;
    }

    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    camera->height.setValue(1000.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
        nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr, "subpixel proxy did not collapse to a point\n");
        root->unref();
        return 1;
    }
    const Obol::CadRenderedWork collapsedWork =
        assembly->lastRenderedWork();
    if (!collapsedWork.exact ||
            collapsedWork.positionCount <= collapsedWork.lineCount * 2u) {
        std::fprintf(stderr,
            "aggregate proxy point was omitted from exact rendered work\n");
        root->unref();
        return 1;
    }
    /* A point which grows past the hard five-pixel ceiling must become a box
     * immediately.  Anti-flicker hysteresis is permitted only when shrinking
     * a box back to a point. */
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    camera->height.setValue(45.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastSubpixelProxyDrawPointCount() != 0u ||
            assembly->lastRenderedWork().lineCount !=
                Obol::CadAggregateProxyBoxLineCount) {
        std::fprintf(stderr,
            "aggregate point exceeded the hard five-pixel ceiling\n");
        root->unref();
        return 1;
    }
    setCadDrawMode(root, SoCADViewState::SHADED);
    if (!render(renderer, root)) {
        std::fprintf(stderr,
            "shaded aggregate proxy frame did not render\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyPresentationWork shadedProxyWork =
        assembly->lastStructuralProxyPresentationWork();
    if (assembly->lastRenderedWork().lineCount != 0u ||
            assembly->lastRenderedWork().triangleCount !=
                Obol::CadAggregateProxyBoxTriangleCount ||
            !shadedProxyWork.exact ||
            shadedProxyWork.aggregatePointCount != 0u ||
            shadedProxyWork.aggregateBoxCount != 1u ||
            shadedProxyWork.retainedWireBoxCount != 0u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "shaded aggregate proxy was not rendered as a solid box\n");
        root->unref();
        return 1;
    }
    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    camera->height.setValue(1000.0f);
    /* A shaded cold view must aggregate a subpixel replacement directly,
     * rather than loading a shaded mesh merely to reach the same point. */
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "shaded structural fallback did not collapse directly\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyPresentationWork pointProxyWork =
        assembly->lastStructuralProxyPresentationWork();
    if (!pointProxyWork.exact ||
            pointProxyWork.aggregatePointCount != 1u ||
            pointProxyWork.aggregateBoxCount != 0u ||
            pointProxyWork.retainedWireBoxCount != 0u) {
        std::fprintf(stderr,
            "structural aggregate point work report was not exact\n");
        root->unref();
        return 1;
    }
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "wire structural fallback did not survive draw-mode restore\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(320.0f);
    setCadDrawMode(root, SoCADViewState::SHADED);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "pixel-sized shaded structural fallback used mesh hysteresis\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(1000.0f);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "structural fallback did not restore after boundary test\n");
        root->unref();
        return 1;
    }
    const uint64_t initialPresentation =
        assembly->lastSubpixelProxyRevision();
    const uint64_t initialPlanBuilds =
        assembly->framePlanBuildCount();
    if (!initialPresentation || !render(renderer, root) ||
        assembly->lastSubpixelProxyRevision() != initialPresentation) {
        std::fprintf(stderr,
            "unchanged subpixel proxy view rebuilt presentation state\n");
        root->unref();
        return 1;
    }

    /*
     * Visibility is a retained per-instance presentation delta.  Hiding and
     * restoring one occurrence must update the aggregate point channel
     * without recompiling unrelated part/instance topology.
     */
    assembly->setHiddenInstances({proxyInstance});
    if (!render(renderer, root) || !assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 0u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse hide rebuilt the frame plan or remained visible\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({});
    if (!render(renderer, root) || assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse visibility restore rebuilt the frame plan or stayed hidden\n");
        root->unref();
        return 1;
    }
    /* A camera change while hidden legitimately reclassifies the occurrence
     * as offscreen.  The subsequent sparse visibility restore must replace
     * that hidden classifier state at the current camera; merely clearing the
     * instance flag leaves both its geometry and aggregate proxy suppressed. */
    assembly->setHiddenInstances({proxyInstance});
    camera->height.setValue(1100.0f);
    if (!render(renderer, root) ||
            !assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "hidden occurrence did not adopt the changed-camera state\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({});
    if (!render(renderer, root) || assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse restore retained a hidden changed-camera classification\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(1000.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "restored occurrence did not follow the next camera update\n");
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({proxyInstance});
    if (!render(renderer, root) || assembly->selectedInstanceCount() != 1u ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse selection did not retain the styled point proxy in place "
            "(selected=%zu proxies=%zu plans=%llu/%llu)\n",
            assembly->selectedInstanceCount(),
            assembly->lastSubpixelProxyCount(),
            static_cast<unsigned long long>(assembly->framePlanBuildCount()),
            static_cast<unsigned long long>(initialPlanBuilds));
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({});
    if (!render(renderer, root) || assembly->selectedInstanceCount() != 0u ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse selection clear did not restore the point proxy in place\n");
        root->unref();
        return 1;
    }

    // View importance is presentation policy, not semantic selection.  It
    // must promote/demote the same retained occurrence through the sparse
    // proxy channel without rebuilding the assembly-wide frame plan.
    assembly->setPointProxyProtectedInstances({proxyInstance});
    const uint64_t protectedRevision =
        assembly->pointProxyProtectionRevision();
    const std::vector<Obol::InstanceId> protectedSnapshot =
        assembly->pointProxyProtectedInstances();
    if (protectedSnapshot.size() != 1u ||
            protectedSnapshot[0] != proxyInstance ||
            protectedRevision == 0 ||
            assembly->lastClassifiedPointProxyProtectionRevision() !=
                protectedRevision ||
            !render(renderer, root) ||
            assembly->selectedInstanceCount() != 0u ||
            assembly->lastSubpixelProxyCount() != 0u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "point-proxy protection did not promote the retained instance\n");
        root->unref();
        return 1;
    }
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> adoptedProtection;
    assembly->adoptPointProxyProtectedInstances(std::move(adoptedProtection));
    const uint64_t adoptedRevision =
        assembly->pointProxyProtectionRevision();
    if (adoptedRevision == protectedRevision ||
            assembly->lastClassifiedPointProxyProtectionRevision() ==
                adoptedRevision ||
            !assembly->pointProxyProtectedInstances().empty() ||
            !render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastClassifiedPointProxyProtectionRevision() !=
                adoptedRevision ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "adopting point-proxy protection did not restore aggregation\n");
        root->unref();
        return 1;
    }

    // The box remains subpixel from the opposite side, but its nearest
    // depth-preserving corner changes.  This must advance the presentation
    // revision even though the collapsed-instance mask does not change.
    camera->position.setValue(0.0f, 0.0f, -5.0f);
    camera->orientation.setValue(SbRotation(SbVec3f(0.0f, 1.0f, 0.0f),
        3.14159265f));
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
        assembly->lastSubpixelProxyRevision() == initialPresentation) {
        std::fprintf(stderr,
            "camera movement did not update subpixel proxy presentation\n");
        root->unref();
        return 1;
    }

    // Structural fallbacks use the exact declared pixel boundary rather than
    // mesh hysteresis.  The proxy now projects just above one pixel and must
    // remain a box until its mesh is available.
    camera->height.setValue(250.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "structural proxy remained collapsed above the pixel boundary\n");
        root->unref();
        return 1;
    }

    // Above the leave threshold, the same persistent AABB returns to its
    // normal wire representation without rebuilding scene geometry.
    camera->height.setValue(200.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr, "subpixel proxy did not expand back to wire\n");
        root->unref();
        return 1;
    }

    // Under measured interaction pressure the same retained occurrence may
    // enter the aggregate batch at a larger screen-error threshold.  Returning
    // to the pixel-exact threshold must restore its ordinary representation
    // without a geometry update.
    cadViewState(root)->pointProxyPixelThreshold.setValue(2.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "interactive point threshold did not aggregate retained proxy\n");
        root->unref();
        return 1;
    }
    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "pixel-exact point threshold did not restore retained proxy\n");
        root->unref();
        return 1;
    }

    /*
     * Retained shaded LoD uses the same camera-local replacement.  The
     * triangles and instance identity remain in the assembly; only the draw
     * channel changes, so a near view promotes the mesh without an update or
     * reload.
     */
    Obol::PartGeometryBuilder shadedSubpixel;
    Obol::TriMesh shadedMesh;
    shadedMesh.positions = {
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f( 0.5f, -0.5f, 0.0f),
        SbVec3f( 0.5f,  0.5f, 0.0f),
        SbVec3f(-0.5f,  0.5f, 0.0f)
    };
    shadedMesh.indices = {0, 1, 2, 0, 2, 3};
    shadedMesh.bounds.makeEmpty();
    for (const SbVec3f& point : shadedMesh.positions)
        shadedMesh.bounds.extendBy(point);
    shadedSubpixel.shaded = std::move(shadedMesh);
    shadedSubpixel.subpixelProxyEligible = true;
    const Obol::PartId shadedSubpixelPart =
        Obol::CadIdBuilder::partId("shaded-subpixel");
    requireCadMutation(
        admitAndUpsertPart(assembly, shadedSubpixelPart, shadedSubpixel),
        "shaded subpixel part");
    Obol::InstanceRecord shadedInstance;
    shadedInstance.part = shadedSubpixelPart;
    shadedInstance.parent = Obol::CadIdBuilder::rootInstance();
    shadedInstance.childName = "shaded-subpixel";
    shadedInstance.localToRoot.makeIdentity();
    const Obol::InstanceId shadedSubpixelInstance =
        requireCadValue(assembly->upsertInstanceAuto(shadedInstance),
            "shaded subpixel instance").instance;
    setCadDrawMode(root, SoCADViewState::SHADED);
    camera->height.setValue(1000.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 2u) {
        std::fprintf(stderr,
            "subpixel shaded LoD did not enter the aggregate point batch\n");
        root->unref();
        return 1;
    }
    /* Ordinary retained meshes keep the 0.75/1.25 Schmitt band.  At this
     * view the structural fallback expands at the exact one-pixel boundary,
     * while the shaded occurrence remains collapsed until it crosses the
     * wider leave threshold. */
    camera->height.setValue(250.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "retained mesh did not preserve subpixel hysteresis\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(200.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "retained mesh did not leave subpixel hysteresis band\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(2.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "near shaded LoD did not promote its retained mesh in place\n");
        root->unref();
        return 1;
    }
    assembly->removeInstance(shadedSubpixelInstance);
    assembly->removePart(shadedSubpixelPart);

    /*
     * More than 128 independently authored wire parts select the flat batch.
     * Keep one retained progressive shaded part in the same assembly: this is
     * the mixed cold-start state that must not disable box batching.  Compare
     * it with the ordinary per-part renderer so batching cannot silently
     * change authored coordinates or occurrence placement.
     */
    setCadDrawMode(root, SoCADViewState::SHADED_WITH_EDGES);
    for (int i = 0; i < 144; ++i) {
        Obol::PartGeometryBuilder box;
        box.wire = unitBox();
        box.subpixelProxyEligible = true;
        box.structuralProxy = true;
        const float sx = 0.65f + 0.07f * static_cast<float>(i % 5);
        const float sy = 0.55f + 0.05f * static_cast<float>((i / 5) % 7);
        const float sz = 0.45f + 0.03f * static_cast<float>((i / 11) % 9);
        box.wire->bounds.makeEmpty();
        for (SbVec3f& point : box.wire->segmentPoints) {
            point.setValue(point[0] * sx + 0.11f * static_cast<float>(i % 3),
                           point[1] * sy - 0.09f * static_cast<float>(i % 4),
                           point[2] * sz);
            box.wire->bounds.extendBy(point);
        }
        char partName[64] = {};
        std::snprintf(partName, sizeof(partName), "distinct-proxy-%03d", i);
        const Obol::PartId boxPart = Obol::CadIdBuilder::partId(partName);
        requireCadMutation(admitAndUpsertPart(assembly, boxPart, box),
            "streamed box part");

        Obol::InstanceRecord boxInstance;
        boxInstance.part = boxPart;
        boxInstance.parent = Obol::CadIdBuilder::rootInstance();
        boxInstance.childName = partName;
        boxInstance.occurrenceIndex = static_cast<uint32_t>(i + 2);
        boxInstance.lodStructuralProxy = true;
        boxInstance.localToRoot.setTranslate(SbVec3f(
            -27.5f + 5.0f * static_cast<float>(i % 12),
            -27.5f + 5.0f * static_cast<float>(i / 12), 0.0f));
        boxInstance.style.hasColorOverride = true;
        boxInstance.style.color = SbColor4f(
            (i & 1) ? 1.0f : 0.2f,
            (i & 2) ? 0.8f : 0.25f,
            (i & 4) ? 0.9f : 0.3f, 1.0f);
        requireCadMutation(assembly->upsertInstanceAuto(boxInstance),
            "streamed box instance");
    }

    Obol::PartGeometryBuilder progressiveGeometry;
    Obol::TriMesh triangle;
    triangle.positions = {
        SbVec3f(-1.5f, -1.0f, -0.25f),
        SbVec3f(1.5f, -1.0f, -0.25f),
        SbVec3f(0.0f, 1.5f, -0.25f)
    };
    triangle.indices = {0, 1, 2};
    triangle.bounds = SbBox3f(
        SbVec3f(-1.5f, -1.0f, -0.25f),
        SbVec3f(1.5f, 1.5f, -0.25f));
    triangle.progressiveMinimumCut = 15;
    triangle.progressiveResidentCut = 15;
    setProgressiveCuts(triangle, 16, 0, 0);
    triangle.progressiveCuts[15].indexCount = 3;
    triangle.progressiveCuts[15].positionCount = 3;
    triangle.progressiveQuantizationMinimum = triangle.bounds.getMin();
    triangle.progressiveQuantizationMaximum = triangle.bounds.getMax();
    progressiveGeometry.shaded = std::move(triangle);
    const Obol::PartId progressivePart =
        Obol::CadIdBuilder::partId("mixed-progressive-triangle");
    requireCadMutation(
        admitAndUpsertPart(assembly, progressivePart, progressiveGeometry),
        "streamed progressive part");
    Obol::InstanceRecord progressiveInstance;
    progressiveInstance.part = progressivePart;
    progressiveInstance.parent = Obol::CadIdBuilder::rootInstance();
    progressiveInstance.childName = "mixed-progressive-triangle";
    progressiveInstance.localToRoot.makeIdentity();
    requireCadMutation(assembly->upsertInstanceAuto(progressiveInstance),
        "streamed progressive instance");

    camera->height.setValue(70.0f);
    setTestEnvironment("OBOL_CAD_FLAT_WIRE", "0", 1);
    if (!render(renderer, root)) {
        std::fprintf(stderr, "per-part mixed proxy reference did not render\n");
        root->unref();
        return 1;
    }
    const SbVec2s imageSize = renderer.getViewportRegion().getViewportSizePixels();
    const size_t imageBytes = static_cast<size_t>(imageSize[0]) *
        static_cast<size_t>(imageSize[1]) * 3u;
    std::vector<unsigned char> reference(
        renderer.getBuffer(), renderer.getBuffer() + imageBytes);

    setTestEnvironment("OBOL_CAD_FLAT_WIRE", "1", 1);
    if (!render(renderer, root) || assembly->lastRenderTier() != 5) {
        std::fprintf(stderr,
            "mixed progressive scene did not retain the flat wire batch\n");
        root->unref();
        return 1;
    }
    const unsigned char *batched = renderer.getBuffer();
    size_t changedPixels = 0;
    size_t coveredPixels = 0;
    for (size_t p = 0; p < imageBytes; p += 3) {
        const bool referenceCovered =
            reference[p] || reference[p + 1] || reference[p + 2];
        const bool batchedCovered =
            batched[p] || batched[p + 1] || batched[p + 2];
        if (referenceCovered || batchedCovered)
            ++coveredPixels;
        if (std::memcmp(reference.data() + p, batched + p, 3) != 0)
            ++changedPixels;
    }
    if (!coveredPixels || changedPixels > coveredPixels / 50u) {
        std::fprintf(stderr,
            "flat wire batch changed mixed-scene placement (%zu/%zu pixels)\n",
            changedPixels, coveredPixels);
        root->unref();
        return 1;
    }

    /*
     * qged selection changes both the retained style color and the selected
     * flag.  In a mixed shaded-with-edges assembly, a color-only style change
     * must patch the instance stream just like the flag change; recompiling
     * the complete frame plan makes selection latency scale with scene size.
     */
    const uint64_t mixedPlanBuilds = assembly->framePlanBuildCount();
    Obol::InstanceStyle selectedStyle;
    selectedStyle.hasColorOverride = true;
    selectedStyle.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    Obol::InstanceStyleUpdate selectedStyleUpdate;
    selectedStyleUpdate.instance = proxyInstance;
    selectedStyleUpdate.style = selectedStyle;
    requireCadMutation(assembly->updateInstanceStyles({selectedStyleUpdate}),
        "streamed selected style");
    assembly->setSelectedInstances({proxyInstance});
    if (!render(renderer, root) ||
            assembly->selectedInstanceCount() != 1u ||
            assembly->framePlanBuildCount() != mixedPlanBuilds) {
        std::fprintf(stderr,
            "mixed-mode sparse style/selection rebuilt the frame plan\n");
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({});
    if (!render(renderer, root) ||
            assembly->selectedInstanceCount() != 0u ||
            assembly->framePlanBuildCount() != mixedPlanBuilds) {
        std::fprintf(stderr,
            "mixed-mode sparse selection clear rebuilt the frame plan\n");
        root->unref();
        return 1;
    }

    /*
     * A streamed leaf normally changes from a unique structural box part to a
     * newly available progressive mesh part.  That is a presentation-slot
     * rebind, not arbitrary assembly topology: adding the unreferenced mesh
     * and rebinding the unique occurrence must retain the compiled plan.
     */
    Obol::PartGeometryBuilder rebindBox;
    rebindBox.wire = unitBox();
    rebindBox.subpixelProxyEligible = true;
    rebindBox.structuralProxy = true;
    const Obol::PartId rebindBoxPart =
        Obol::CadIdBuilder::partId("stream-rebind-box");
    requireCadMutation(admitAndUpsertPart(assembly, rebindBoxPart, rebindBox),
        "rebind box part");
    Obol::InstanceRecord rebindRecord;
    rebindRecord.part = rebindBoxPart;
    rebindRecord.parent = Obol::CadIdBuilder::rootInstance();
    rebindRecord.childName = "stream-rebind";
    rebindRecord.occurrenceIndex = 9001;
    rebindRecord.lodStructuralProxy = true;
    rebindRecord.localToRoot.setTranslate(SbVec3f(0.0f, 20.0f, 0.0f));
    const Obol::InstanceId rebindInstance =
        requireCadValue(assembly->upsertInstanceAuto(rebindRecord),
            "rebind instance").instance;
    if (!render(renderer, root)) {
        std::fprintf(stderr, "stream rebind setup did not render\n");
        root->unref();
        return 1;
    }
    const uint64_t rebindPlanBuilds = assembly->framePlanBuildCount();
    const size_t rebindPlanInstances =
        assembly->framePlanInstanceRecordCount();
    const Obol::PartId rebindMeshPart =
        Obol::CadIdBuilder::partId("stream-rebind-mesh");
    requireCadMutation(
        admitAndUpsertPart(assembly, rebindMeshPart, progressiveGeometry),
        "rebind mesh part");
    rebindRecord.part = rebindMeshPart;
    rebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate rebindUpdate;
    rebindUpdate.instance = rebindInstance;
    rebindUpdate.record = rebindRecord;
    requireCadMutation(assembly->upsertInstances({rebindUpdate}),
        "part rebind");
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != rebindPlanBuilds ||
            assembly->framePlanInstanceRecordCount() !=
                rebindPlanInstances) {
        std::fprintf(stderr,
            "unique box-to-mesh rebind rebuilt or duplicated the frame plan\n");
        root->unref();
        return 1;
    }

    /*
     * A retained LoD publication is not category-homogeneous: a wave can
     * promote structural boxes while advancing cuts on meshes published by
     * an earlier wave.  Each operation is sparsely patchable and their
     * combination must remain sparsely patchable as well.  This mirrors the
     * large-scene batch which used to trigger one complete plan rebuild per
     * 256-512 occurrence publication.
     */
    const Obol::PartId mixedRebindBoxPart =
        Obol::CadIdBuilder::partId("mixed-wave-rebind-box");
    requireCadMutation(admitAndUpsertPart(assembly, mixedRebindBoxPart, rebindBox),
        "mixed rebind box part");
    Obol::InstanceRecord mixedRebindRecord = rebindRecord;
    mixedRebindRecord.part = mixedRebindBoxPart;
    mixedRebindRecord.childName = "mixed-wave-rebind";
    mixedRebindRecord.occurrenceIndex = 9006;
    mixedRebindRecord.lodStructuralProxy = true;
    mixedRebindRecord.localToRoot.setTranslate(
        SbVec3f(-8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedRebindInstance =
        requireCadValue(assembly->upsertInstanceAuto(mixedRebindRecord),
            "mixed rebind instance").instance;

    Obol::InstanceRecord mixedCutRecord = progressiveInstance;
    mixedCutRecord.childName = "mixed-wave-cut";
    mixedCutRecord.occurrenceIndex = 9007;
    mixedCutRecord.lodCut = 15;
    mixedCutRecord.localToRoot.setTranslate(
        SbVec3f(8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedCutInstance =
        requireCadValue(assembly->upsertInstanceAuto(mixedCutRecord),
            "mixed cut instance").instance;
    if (!render(renderer, root)) {
        std::fprintf(stderr, "mixed rebind/cut setup did not render\n");
        root->unref();
        return 1;
    }
    const uint64_t mixedRebindPlanBuilds =
        assembly->framePlanBuildCount();
    const size_t mixedRebindPlanInstances =
        assembly->framePlanInstanceRecordCount();

    const Obol::PartId mixedRebindMeshPart =
        Obol::CadIdBuilder::partId("mixed-wave-rebind-mesh");
    requireCadMutation(
        admitAndUpsertPart(assembly, mixedRebindMeshPart, progressiveGeometry),
        "mixed rebind mesh part");
    mixedRebindRecord.part = mixedRebindMeshPart;
    mixedRebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate mixedRebindUpdate;
    mixedRebindUpdate.instance = mixedRebindInstance;
    mixedRebindUpdate.record = mixedRebindRecord;
    mixedCutRecord.lodCut = 14;
    Obol::InstanceUpdate mixedCutUpdate;
    mixedCutUpdate.instance = mixedCutInstance;
    mixedCutUpdate.record = mixedCutRecord;
    requireCadMutation(
        assembly->upsertInstances({mixedRebindUpdate, mixedCutUpdate}),
        "mixed rebind instances");
    const std::optional<Obol::InstanceRecord> retainedMixedRebind =
        assembly->getInstanceRecord(mixedRebindInstance);
    const std::optional<Obol::InstanceRecord> retainedMixedCut =
        assembly->getInstanceRecord(mixedCutInstance);
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != mixedRebindPlanBuilds ||
            assembly->framePlanInstanceRecordCount() !=
                mixedRebindPlanInstances ||
            !retainedMixedRebind ||
            !(retainedMixedRebind->part == mixedRebindMeshPart) ||
            retainedMixedRebind->lodStructuralProxy ||
            !retainedMixedCut || retainedMixedCut->lodCut != 14) {
        std::fprintf(stderr,
            "mixed sparse rebind/cut rebuilt or corrupted the frame plan\n");
        root->unref();
        return 1;
    }
    /*
     * The retired box library entry is now referenced only by the hidden
     * compiled tombstone.  Its plan binding owns the immutable payload until
     * compaction, so removing the library entry must not invalidate the live
     * mesh plan.
     */
    assembly->removePart(rebindBoxPart);
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != rebindPlanBuilds) {
        std::fprintf(stderr,
            "retired tombstone part removal rebuilt the frame plan\n");
        root->unref();
        return 1;
    }
    const size_t sharedProxyBaseline =
        assembly->lastUncollapsedStructuralProxyCount();

    /*
     * Real cold delivery waves mix newly discovered occurrences with older
     * occurrences promoted out of one deduplicated structural-box part.  The
     * shared box range must stay compiled while the promoted occurrence is
     * internally tombstoned and redirected to its mesh tail record.
     */
    const Obol::PartId sharedBoxPart =
        Obol::CadIdBuilder::partId("stream-shared-box");
    requireCadMutation(admitAndUpsertPart(assembly, sharedBoxPart, rebindBox),
        "shared box part");
    Obol::InstanceRecord sharedBoxA = rebindRecord;
    sharedBoxA.part = sharedBoxPart;
    sharedBoxA.lodStructuralProxy = true;
    sharedBoxA.childName = "stream-shared-box-a";
    sharedBoxA.occurrenceIndex = 9002;
    sharedBoxA.localToRoot.setTranslate(SbVec3f(-4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxAId =
        requireCadValue(assembly->upsertInstanceAuto(sharedBoxA),
            "shared box instance A").instance;
    Obol::InstanceRecord sharedBoxB = sharedBoxA;
    sharedBoxB.childName = "stream-shared-box-b";
    sharedBoxB.occurrenceIndex = 9003;
    sharedBoxB.localToRoot.setTranslate(SbVec3f(4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxBId =
        requireCadValue(assembly->upsertInstanceAuto(sharedBoxB),
            "shared box instance B").instance;
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u) {
        std::fprintf(stderr, "shared stream rebind setup did not render\n");
        root->unref();
        return 1;
    }
    const auto sharedStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (sharedStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(sharedStructural.begin(), sharedStructural.end(),
                sharedBoxAId) == sharedStructural.end() ||
            std::find(sharedStructural.begin(), sharedStructural.end(),
                sharedBoxBId) == sharedStructural.end()) {
        std::fprintf(stderr,
            "structural proxy occurrence frontier did not match count\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyProjectionHistogram sharedProjection =
        assembly->lastStructuralProxyProjectionHistogram();
    bool cumulativeProjectionValid = sharedProjection.exact &&
        sharedProjection.revision != 0 &&
        sharedProjection.visibleCount >= sharedStructural.size();
    uint64_t previousProjectionCount = 0;
    for (const uint64_t count : sharedProjection.cumulativeCount) {
        cumulativeProjectionValid = cumulativeProjectionValid &&
            count >= previousProjectionCount &&
            count <= sharedProjection.visibleCount;
        previousProjectionCount = count;
    }
    if (!cumulativeProjectionValid) {
        std::fprintf(stderr,
            "structural projected-size histogram was not exact/cumulative "
            "exact=%d revision=%llu visible=%llu buckets=%llu,%llu,%llu,"
            "%llu,%llu,%llu,%llu frontier=%zu\n",
            sharedProjection.exact ? 1 : 0,
            static_cast<unsigned long long>(sharedProjection.revision),
            static_cast<unsigned long long>(sharedProjection.visibleCount),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[0]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[1]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[2]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[3]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[4]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[5]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[6]),
            sharedStructural.size());
        root->unref();
        return 1;
    }
    const std::array<float,
        Obol::CadStructuralProxyProjectionHistogram::BucketCount>
        structuralBucketLimits = {1.0f, 2.0f, 4.0f, 8.0f,
            16.0f, 32.0f, 64.0f};
    for (size_t bucket = 0; bucket < structuralBucketLimits.size();
            ++bucket) {
        const std::vector<Obol::InstanceId> above =
            assembly->lastStructuralProxyInstancesAbovePixels(
                structuralBucketLimits[bucket]);
        const uint64_t expected = sharedProjection.visibleCount -
            sharedProjection.cumulativeCount[bucket];
        bool sortedUnique = true;
        for (size_t instance = 1; instance < above.size(); ++instance) {
            const Obol::InstanceId &previous = above[instance - 1];
            const Obol::InstanceId &current = above[instance];
            sortedUnique = sortedUnique &&
                (previous.w0 < current.w0 ||
                 (previous.w0 == current.w0 && previous.w1 < current.w1));
        }
        if (above.size() != expected || !sortedUnique) {
            std::fprintf(stderr,
                "structural projected-size frontier mismatch "
                "limit=%.0f expected=%llu actual=%zu sorted=%d\n",
                structuralBucketLimits[bucket],
                static_cast<unsigned long long>(expected), above.size(),
                sortedUnique ? 1 : 0);
            root->unref();
            return 1;
        }
    }
    const uint64_t mixedStreamPlanBuilds =
        assembly->framePlanBuildCount();
    assembly->setHiddenInstances({sharedBoxAId});
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 1u ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount +
                1u != sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "sparse hide retained an uncollapsed structural proxy or "
            "rebuilt the complete frame plan\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({});
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount !=
                sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "sparse restore lost an uncollapsed structural proxy or "
            "rebuilt the complete frame plan\n");
        root->unref();
        return 1;
    }

    /* A structural occurrence wholly outside the camera frustum is not a
     * visible fallback and must not make a view-convergence client attempt to
     * realize geometry for it.  It shares the live box part so this also
     * exercises the batched range accounting used by large CAD assemblies. */
    Obol::InstanceRecord offscreenBox = sharedBoxB;
    offscreenBox.childName = "stream-offscreen-box";
    offscreenBox.occurrenceIndex = 9005;
    offscreenBox.localToRoot.setTranslate(
        SbVec3f(100000.0f, 24.0f, 0.0f));
    const Obol::InstanceId offscreenBoxId =
        requireCadValue(assembly->upsertInstanceAuto(offscreenBox),
            "offscreen box instance").instance;
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount !=
                sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "off-frustum box was counted as a visible structural proxy\n");
        root->unref();
        return 1;
    }
    const auto offscreenStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (offscreenStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(offscreenStructural.begin(), offscreenStructural.end(),
                offscreenBoxId) != offscreenStructural.end()) {
        std::fprintf(stderr,
            "off-frustum occurrence entered structural repair frontier\n");
        root->unref();
        return 1;
    }
    Obol::InstanceRecord promotedShared = sharedBoxA;
    promotedShared.part = rebindMeshPart;
    promotedShared.lodStructuralProxy = false;
    Obol::InstanceUpdate promotedUpdate;
    promotedUpdate.instance = sharedBoxAId;
    promotedUpdate.record = promotedShared;

    Obol::InstanceRecord newStreamed = promotedShared;
    newStreamed.childName = "stream-new-mesh";
    newStreamed.occurrenceIndex = 9004;
    newStreamed.localToRoot.setTranslate(SbVec3f(0.0f, 28.0f, 0.0f));
    const Obol::InstanceId newStreamedId =
        Obol::CadIdBuilder::childInstance(
            newStreamed.parent, newStreamed.childName,
            newStreamed.occurrenceIndex, newStreamed.boolOp);
    Obol::InstanceUpdate newStreamedUpdate;
    newStreamedUpdate.instance = newStreamedId;
    newStreamedUpdate.record = newStreamed;
    requireCadMutation(
        assembly->upsertInstances({promotedUpdate, newStreamedUpdate}),
        "promoted streamed instances");
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 1u ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount +
                1u != sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "mixed shared-box promotion rebuilt the complete frame plan "
            "or retained its stale box presentation\n");
        root->unref();
        return 1;
    }
    const auto promotedStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (promotedStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(promotedStructural.begin(), promotedStructural.end(),
                sharedBoxAId) != promotedStructural.end() ||
            std::find(promotedStructural.begin(), promotedStructural.end(),
                sharedBoxBId) == promotedStructural.end()) {
        std::fprintf(stderr,
            "box-to-mesh promotion left stale structural frontier entry\n");
        root->unref();
        return 1;
    }

    /* Sparse rebind repair touches both the append-only tombstone part and
     * the live replacement part.  Exercise varied part hashes so frontier
     * publication cannot accidentally depend on unordered affected-part
     * iteration order. */
    for (unsigned int variant = 0; variant < 16u; ++variant) {
        char partName[64] = {};
        std::snprintf(partName, sizeof(partName),
            "stream-rebind-order-%u", variant);
        const Obol::PartId variantPart =
            Obol::CadIdBuilder::partId(partName);
        requireCadMutation(
            admitAndUpsertPart(assembly, variantPart, progressiveGeometry),
            "variant rebind mesh part");

        Obol::InstanceUpdate toMesh;
        toMesh.instance = sharedBoxBId;
        toMesh.record = sharedBoxB;
        toMesh.record.part = variantPart;
        toMesh.record.lodStructuralProxy = false;
        requireCadMutation(assembly->upsertInstances({toMesh}),
            "variant box-to-mesh rebind");
        if (!render(renderer, root)) {
            std::fprintf(stderr,
                "variant box-to-mesh rebind did not render\n");
            root->unref();
            return 1;
        }
        const auto meshFrontier =
            assembly->lastUncollapsedStructuralProxyInstances();
        if (meshFrontier.size() !=
                assembly->lastUncollapsedStructuralProxyCount() ||
                std::find(meshFrontier.begin(), meshFrontier.end(),
                    sharedBoxBId) != meshFrontier.end()) {
            std::fprintf(stderr,
                "box-to-mesh frontier depended on affected-part order\n");
            root->unref();
            return 1;
        }

        Obol::InstanceUpdate toBox;
        toBox.instance = sharedBoxBId;
        toBox.record = sharedBoxB;
        requireCadMutation(assembly->upsertInstances({toBox}),
            "variant mesh-to-box rebind");
        if (!render(renderer, root)) {
            std::fprintf(stderr,
                "variant mesh-to-box rebind did not render\n");
            root->unref();
            return 1;
        }
        const auto boxFrontier =
            assembly->lastUncollapsedStructuralProxyInstances();
        if (boxFrontier.size() !=
                assembly->lastUncollapsedStructuralProxyCount() ||
                std::find(boxFrontier.begin(), boxFrontier.end(),
                    sharedBoxBId) == boxFrontier.end()) {
            std::fprintf(stderr,
                "mesh-to-box frontier depended on affected-part order\n");
            root->unref();
            return 1;
        }
        assembly->removePart(variantPart);
    }
    assembly->setHiddenInstances({offscreenBoxId});

    /*
     * Subsequent cold-delivery waves may append more occurrences of the
     * existing structural-box part after unrelated mesh records.  Extending
     * the old compiled range across those records is valid sparse storage,
     * but those intervening records are holes, not box instances.  Every
     * renderer and proxy-accounting consumer must honor current partIndex
     * membership rather than treating range containment as ownership.
     */
    for (uint32_t wave = 0; wave < 8u; ++wave) {
        char boxName[64] = {};
        std::snprintf(
            boxName, sizeof(boxName), "stream-late-box-%u", wave);
        Obol::InstanceRecord lateBox = sharedBoxB;
        lateBox.childName = boxName;
        lateBox.occurrenceIndex = 9100u + wave;
        lateBox.localToRoot.setTranslate(SbVec3f(
            -14.0f + 4.0f * static_cast<float>(wave),
            30.0f, 0.0f));
        Obol::InstanceUpdate lateBoxUpdate;
        lateBoxUpdate.instance =
            Obol::CadIdBuilder::childInstance(
                lateBox.parent, lateBox.childName,
                lateBox.occurrenceIndex, lateBox.boolOp);
        lateBoxUpdate.record = lateBox;

        char meshName[64] = {};
        std::snprintf(
            meshName, sizeof(meshName), "stream-late-mesh-%u", wave);
        Obol::InstanceRecord lateMesh = newStreamed;
        lateMesh.childName = meshName;
        lateMesh.occurrenceIndex = 9200u + wave;
        lateMesh.localToRoot.setTranslate(SbVec3f(
            -14.0f + 4.0f * static_cast<float>(wave),
            34.0f, 0.0f));
        Obol::InstanceUpdate lateMeshUpdate;
        lateMeshUpdate.instance =
            Obol::CadIdBuilder::childInstance(
                lateMesh.parent, lateMesh.childName,
                lateMesh.occurrenceIndex, lateMesh.boolOp);
        lateMeshUpdate.record = lateMesh;

        requireCadMutation(assembly->upsertInstances(
            {lateMeshUpdate, lateBoxUpdate}), "late streamed instances");
        if (!render(renderer, root) ||
                assembly->framePlanBuildCount() !=
                    mixedStreamPlanBuilds ||
                assembly->lastUncollapsedStructuralProxyCount() !=
                    sharedProxyBaseline + 2u + wave) {
            std::fprintf(stderr,
                "interleaved sparse delivery wave %u retained stale "
                "draw-range members\n", wave);
            root->unref();
            return 1;
        }
    }

    // Scene-wide LoD admission may retire thousands of distinct mesh parts
    // in one camera epoch.  The bulk API must remove the complete set without
    // changing unrelated parts or requiring one full instance scan per part.
    std::vector<Obol::PartId> removalParts;
    for (int i = 0; i < 3; ++i) {
        char name[64] = {};
        std::snprintf(name, sizeof(name), "bulk-remove-part-%d", i);
        const Obol::PartId id = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, id, geometry),
            "stream compaction part");
        removalParts.push_back(id);
    }
    const size_t beforeBulkRemove = assembly->partCount();
    assembly->removeParts(removalParts);
    if (assembly->partCount() + removalParts.size() != beforeBulkRemove) {
        std::fprintf(stderr, "bulk part removal did not remove exact set\n");
        root->unref();
        return 1;
    }

    /* Hosts use this token to distinguish a one-time retained preparation
     * deadline from steady draw overload.  An unchanged prepared replay must
     * eventually leave it stable, while a camera-classification input change
     * must advance it. */
    bool observedSteadyReplay = false;
    for (int attempt = 0; attempt < 3 && !observedSteadyReplay; ++attempt) {
        const uint64_t before = assembly->renderPreparationSerial();
        if (!render(renderer, root)) {
            std::fprintf(stderr,
                "preparation-serial replay did not render\n");
            root->unref();
            return 1;
        }
        observedSteadyReplay =
            assembly->renderPreparationSerial() == before;
    }
    const uint64_t beforeClassification =
        assembly->renderPreparationSerial();
    const float threshold = cadViewState(root)->pointProxyPixelThreshold.getValue();
    cadViewState(root)->pointProxyPixelThreshold.setValue(
        threshold < 64.0f ? threshold + 1.0f : threshold - 1.0f);
    if (!observedSteadyReplay || !render(renderer, root) ||
            assembly->renderPreparationSerial() == beforeClassification) {
        std::fprintf(stderr,
            "preparation serial did not separate replay from "
            "classification work\n");
        root->unref();
        return 1;
    }

    root->unref();
    return 0;
}

bool
clearReleasesCompiledPlanGeometry()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    setCadDrawMode(root, SoCADViewState::SHADED);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("clear-plan-retention-part");
    const Obol::InstanceId instance =
        Obol::CadIdBuilder::instanceId("clear-plan-retention-instance");
    std::weak_ptr<const Obol::PartGeometry> geometryLifetime;
    {
        Obol::PartGeometryBuilder builder;
        builder.shaded.emplace();
        builder.shaded->positions = {
            SbVec3f(-0.5f, -0.5f, 0.0f),
            SbVec3f(0.5f, -0.5f, 0.0f),
            SbVec3f(0.0f, 0.5f, 0.0f)};
        builder.shaded->indices = {0u, 1u, 2u};
        builder.shaded->bounds = SbBox3f(
            SbVec3f(-0.5f, -0.5f, 0.0f),
            SbVec3f(0.5f, 0.5f, 0.0f));
        const Obol::CadGeometryAdmission admitted =
            Obol::cadAdmitPartGeometry(std::move(builder));
        if (!admitted) {
            root->unref();
            return false;
        }
        geometryLifetime = admitted.geometry.shared();
        requireCadMutation(assembly->upsertParts(
            {{part, admitted.geometry, false}}),
            "clear retention part");
        Obol::InstanceRecord record;
        record.part = part;
        requireCadMutation(assembly->upsertInstance(instance, record),
            "clear retention instance");
    }

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    const bool warmed = render(renderer, root) &&
        assembly->framePlanBuildCount() != 0u &&
        !geometryLifetime.expired();
    assembly->clear();
    const bool released = geometryLifetime.expired() &&
        assembly->partCount() == 0u && assembly->instanceCount() == 0u;
    root->unref();
    return warmed && released;
}

#include "render_test_registration.h"

class CadSubpixelProxyContracts : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        SoCADAssembly::initClass();
    }
};

TEST_F(CadSubpixelProxyContracts, SparseUniformClustersRemainValid)
{
    EXPECT_TRUE(sparseUniformClusterContract());
}

TEST_F(CadSubpixelProxyContracts, ProjectedProxyClassificationHandlesClipEdges)
{
    EXPECT_TRUE(sharedProjectedProxyContract());
}

TEST_F(CadSubpixelProxyContracts, DegenerateStructuralProxyKeepsItsPlane)
{
    EXPECT_TRUE(degenerateStructuralProxyContract());
}

TEST_F(CadSubpixelProxyContracts, OrientedAggregateProxyIsRetainedAndRendered)
{
    EXPECT_TRUE(orientedAggregateProxyContract());
}

TEST_F(CadSubpixelProxyContracts, SoftwareAggregationPreservesLogicalCoverage)
{
    EXPECT_TRUE(softwareSubpixelProxyAggregationContract());
}

TEST_F(CadSubpixelProxyContracts, LifecycleAndStreamingStateRemainCoherent)
{
    EXPECT_EQ(runCadSubpixelProxyLifecycleContract(), 0);
}

TEST_F(CadSubpixelProxyContracts, FlatFaceLightingMatchesExplicitNormals)
{
    EXPECT_TRUE(flatFaceLightingMatchesExplicitNormals(true));
}

TEST_F(CadSubpixelProxyContracts, FixedCutFaceLightingMatchesExplicitNormals)
{
    EXPECT_TRUE(flatFaceLightingMatchesExplicitNormals(false));
}

TEST_F(CadSubpixelProxyContracts, NormalFreeTwoSidedGlslMatchesFixedPipeline)
{
    EXPECT_TRUE(normalFreeTwoSidedGlslMatchesFixed());
}

TEST_F(CadSubpixelProxyContracts, NonUniformNormalTransformMatchesFixedPipeline)
{
    EXPECT_TRUE(nonUniformNormalTransformMatchesFixed());
}

TEST_F(CadSubpixelProxyContracts,
       ProgressiveNormalFacingIgnoresQuantizedWinding)
{
    EXPECT_TRUE(progressiveNormalFacingIgnoresQuantizedWinding());
}

TEST_F(CadSubpixelProxyContracts,
       TransformedSpotlightStateAffectsBothPipelines)
{
    EXPECT_TRUE(transformedSpotlightStateAffectsBothPipelines());
}

TEST_F(CadSubpixelProxyContracts, IndirectProgressiveAtlasGrows)
{
    EXPECT_TRUE(indirectProgressiveAtlasGrows(false));
}

TEST_F(CadSubpixelProxyContracts, IndirectCeilingGrowsResidentAtlas)
{
    EXPECT_TRUE(indirectProgressiveAtlasGrows(true));
}

TEST_F(CadSubpixelProxyContracts, IndirectGenerationAppendsOnlyItsSuffix)
{
    EXPECT_TRUE(indirectProgressiveGenerationAppendsSuffix());
}

TEST_F(CadSubpixelProxyContracts, OrdinaryGenerationAppendsOnlyItsSuffix)
{
    EXPECT_TRUE(ordinaryProgressiveGenerationAppendsSuffix());
}

TEST_F(CadSubpixelProxyContracts, ZeroLineageReplacementDoesNotOverread)
{
    EXPECT_TRUE(ordinaryProgressiveZeroLineageReplacesWithoutOverread());
}

TEST_F(CadSubpixelProxyContracts, OrdinaryExecutorHonorsAbortSafePoints)
{
    EXPECT_TRUE(ordinaryExecutorHonorsAbortSafePoints());
}

TEST_F(CadSubpixelProxyContracts, IndirectValidationResumesAcrossAborts)
{
    EXPECT_TRUE(indirectAtlasValidationResumesAcrossAborts());
}

TEST_F(CadSubpixelProxyContracts, IndirectAtlasPreservesCoverageUnderPressure)
{
    EXPECT_TRUE(indirectProgressiveAtlasPreservesCoverageUnderPressure());
}

TEST_F(CadSubpixelProxyContracts, IndirectPressureProxyPreservesProjectedExtent)
{
    EXPECT_TRUE(indirectPressureProxyPreservesProjectedExtent());
}

TEST_F(CadSubpixelProxyContracts, PreparationReservationCoversBoundedScratch)
{
    EXPECT_TRUE(subpixelPreparationReservationCoversBoundedScratch());
}

TEST_F(CadSubpixelProxyContracts, FlatShadedPlanningResumesAcrossAborts)
{
    EXPECT_TRUE(flatShadedPlanningResumesAcrossAborts());
}

TEST_F(CadSubpixelProxyContracts, FlatShadedAtlasMakesProgressInsideLargeSourceRange)
{
    EXPECT_TRUE(flatShadedAtlasMakesProgressInsideLargeSourceRange());
}

TEST_F(CadSubpixelProxyContracts, ProgressiveReplacementTombstoneKeepsActiveIndex)
{
    EXPECT_TRUE(progressiveReplacementTombstoneKeepsActiveIndex());
}

TEST_F(CadSubpixelProxyContracts, ClearReleasesCompiledPlanGeometry)
{
    EXPECT_TRUE(clearReleasesCompiledPlanGeometry());
}

TEST_F(CadSubpixelProxyContracts, EnclosingModelTransformMovesCadRendering)
{
    EXPECT_TRUE(enclosingModelTransformMovesCadRendering());
}

TEST_F(CadSubpixelProxyContracts, SparseGeometryTopologyChangesRebuildPlan)
{
    EXPECT_TRUE(sparseGeometryTopologyChangesRebuildPlan());
}

TEST_F(CadSubpixelProxyContracts, TriangleDerivedProgressiveWireGrows)
{
    EXPECT_TRUE(triangleDerivedProgressiveWireGrowsWithRequestedCut());
}

TEST_F(CadSubpixelProxyContracts, FixedCutPreparationDoesNotBecomeDrawCost)
{
    const char *backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
    if (backend && std::strcmp(backend, "system") == 0)
        GTEST_SKIP() << "The fixed-function cut cache is a software route";
    const char *previous = std::getenv("OBOL_CAD_FLAT_SHADED");
    const bool hadPrevious = previous != nullptr;
    const std::string previousValue = previous ? previous : "";
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "0", 1);
    EXPECT_TRUE(fixedCutPreparationIsFinite(false));
    EXPECT_TRUE(fixedCutPreparationIsFinite(true));
    if (hadPrevious)
        setTestEnvironment("OBOL_CAD_FLAT_SHADED", previousValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_FLAT_SHADED");
}

TEST_F(CadSubpixelProxyContracts, AssemblyDestructionReleasesGpuResources)
{
    EXPECT_TRUE(assemblyDestructionReleasesGpuResourcesOnLiveContext());
}

TEST_F(CadSubpixelProxyContracts, DisplayPlanePixelsSurviveIndependentCameras)
{
    auto *assembly = new SoCADAssembly;
    Obol::PartGeometryBuilder builder;
    builder.displayPlane = Obol::CadDisplayPlane();
    builder.displayPlane->anchor = SbVec3f(0.25f, -0.1f, 0.0f);
    Obol::WireRep wire;
    wire.segmentPoints = {
        SbVec3f(0, 0, 0), SbVec3f(40, 0, 0),
        SbVec3f(40, 0, 0), SbVec3f(40, 16, 0),
        SbVec3f(40, 16, 0), SbVec3f(0, 16, 0),
        SbVec3f(0, 16, 0), SbVec3f(0, 0, 0)};
    wire.bounds = SbBox3f(SbVec3f(0, 0, 0), SbVec3f(40, 16, 0));
    builder.wire = std::move(wire);
    const auto geometry = Obol::cadAdmitPartGeometry(std::move(builder));
    ASSERT_TRUE(geometry);
    const auto part = Obol::CadIdBuilder::partId("pixel-plane");
    const auto instance = Obol::CadIdBuilder::instanceId("pixel-plane-instance");
    Obol::InstanceRecord record;
    record.part = part;
    record.localToRoot.setTransform(SbVec3f(0.3f, 0.2f, 0),
        SbRotation(SbVec3f(0, 0, 1), 0.5f), SbVec3f(2, 3, 1));
    ASSERT_TRUE(assembly->replaceScene({{part, geometry.geometry, false}}, {{instance, record}}));
    std::array<SoSeparator *, 2> roots;
    std::array<SoOrthographicCamera *, 2> cameras;
    for (size_t view = 0; view < roots.size(); ++view) {
        roots[view] = new SoSeparator;
        roots[view]->ref();
        cameras[view] = new SoOrthographicCamera;
        cameras[view]->position = SbVec3f(0, 0, 10);
        cameras[view]->nearDistance = 1.0f;
        cameras[view]->farDistance = 100.0f;
        cameras[view]->height = view == 0 ? 4.0f : 8.0f;
        cameras[view]->orientation = SbRotation(SbVec3f(0, 0, 1), float(view) * 0.7f);
        roots[view]->addChild(cameras[view]);
        setCadDrawMode(roots[view], SoCADViewState::WIREFRAME);
        roots[view]->addChild(assembly);
    }
    const auto release = [](std::array<SoSeparator *, 2> *nodes) {
        for (SoSeparator *root : *nodes)
            root->unref();
    };
    const std::unique_ptr<std::array<SoSeparator *, 2>, decltype(release)> owner(&roots, release);
    uint64_t firstBuildCount = 0;
    for (const int size : {200, 320, 200}) {
        for (size_t view = 0; view < roots.size(); ++view) {
            SoOffscreenRenderer renderer(SbViewportRegion(size, size));
            renderer.setComponents(SoOffscreenRenderer::RGB);
            renderer.setBackgroundColor(SbColor(0, 0, 0));
            ASSERT_TRUE(render(renderer, roots[view]));
            int minX = size, minY = size, maxX = -1, maxY = -1;
            const unsigned char *pixels = renderer.getBuffer();
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x) {
                    const size_t offset = (size_t(y) * size + x) * 3;
                    if (pixels[offset] < 32 && pixels[offset + 1] < 32 && pixels[offset + 2] < 32)
                        continue;
                    minX = std::min(minX, x); minY = std::min(minY, y);
                    maxX = std::max(maxX, x); maxY = std::max(maxY, y);
                }
            ASSERT_GE(maxX, minX);
            EXPECT_NEAR(maxX - minX, 40, 2);
            EXPECT_NEAR(maxY - minY, 16, 2);
            SbVec3f anchor;
            record.localToRoot.multVecMatrix(geometry.geometry.get()->displayPlane->anchor, anchor);
            cameras[view]->getViewVolume(1.0f).projectToScreen(anchor, anchor);
            EXPECT_NEAR(minX, anchor[0] * size, 2);
            EXPECT_NEAR(minY, anchor[1] * size, 2);
            if (!firstBuildCount)
                firstBuildCount = assembly->framePlanBuildCount();
            EXPECT_EQ(assembly->framePlanBuildCount(), firstBuildCount);
            EXPECT_EQ(assembly->getInstanceRecord(instance)->localToRoot, record.localToRoot);
        }
    }
}

TEST_F(CadSubpixelProxyContracts, AuthoredWireStylesSurviveViewsAndRasterPaths)
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const std::string savedGlsl = previousGlsl ? previousGlsl : "";
    const bool hadGlsl = previousGlsl != nullptr;
    const char *previousFlat = std::getenv("OBOL_CAD_FLAT_WIRE");
    const std::string savedFlat = previousFlat ? previousFlat : "";
    const bool hadFlat = previousFlat != nullptr;
    const char *previousImmediate = std::getenv("OBOL_CAD_FORCE_IMMEDIATE");
    const std::string savedImmediate = previousImmediate ? previousImmediate : "";
    const bool hadImmediate = previousImmediate != nullptr;
    const auto restore = [&](int *) {
        if (hadGlsl) setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", savedGlsl.c_str(), 1);
        else unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
        if (hadFlat) setTestEnvironment("OBOL_CAD_FLAT_WIRE", savedFlat.c_str(), 1);
        else unsetTestEnvironment("OBOL_CAD_FLAT_WIRE");
        if (hadImmediate) setTestEnvironment("OBOL_CAD_FORCE_IMMEDIATE", savedImmediate.c_str(), 1);
        else unsetTestEnvironment("OBOL_CAD_FORCE_IMMEDIATE");
    };
    int unused = 0;
    const std::unique_ptr<int, decltype(restore)> environment(&unused, restore);
    setTestEnvironment("OBOL_CAD_FLAT_WIRE", "1", 1);
    const std::array<float, 3> lineY = {{-0.6f, 0.0f, 0.6f}};
    const std::array<int, 3> widths = {{5, 1, 3}};
    const std::array<SbColor4f, 3> colors = {{
        SbColor4f(1, 0, 0, 1), SbColor4f(0, 1, 0, 1),
        SbColor4f(0, 0, 1, 1)}};
    const std::array<uint16_t, 3> patterns = {{
        0xffffu, 0x1111u, 0xffffu}};
    struct Configuration { bool glsl; int partCount; bool adaptive; bool direct; bool immediate; };
    // The flat renderer starts at 128 occurrences/parts. An adaptive companion
    // prevents instancing and exercises the GLSL VBO route on capable contexts.
    const Configuration configurations[] = {
        {false, 1, false, false, false}, {true, 1, false, false, false},
        {false, 128, false, false, false}, {true, 128, false, false, false},
        {true, 1, true, false, false}, {false, 1, false, true, false},
        {false, 1, false, false, true}
    };
    for (const auto& configuration : configurations) {
        const bool glsl = configuration.glsl;
        const int partCount = configuration.partCount;
        setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", glsl ? "1" : "0", 1);
        setTestEnvironment("OBOL_CAD_FORCE_IMMEDIATE", configuration.immediate ? "1" : "0", 1);
        auto *root = new SoSeparator;
        root->ref();
        const auto release = [](SoSeparator *node) { node->unref(); };
        const std::unique_ptr<SoSeparator, decltype(release)> owner(root, release);
        auto *camera = new SoOrthographicCamera;
        camera->position = SbVec3f(0, 0, 10);
        camera->nearDistance = 1.0f;
        camera->farDistance = 100.0f;
        root->addChild(camera);
        setCadDrawMode(root, SoCADViewState::WIREFRAME);
        cadViewState(root)->softwareWireMode = configuration.direct ?
            SoCADViewState::SOFTWARE_WIRE_FAST : SoCADViewState::SOFTWARE_WIRE_QUALITY;
        auto *assembly = new SoCADAssembly;
        root->addChild(assembly);
        Obol::PartGeometryBuilder builder;
        Obol::WireRep wire;
        for (size_t i = 0; i < lineY.size(); ++i) {
            wire.segmentPoints.emplace_back(-0.6f, lineY[i], 0);
            wire.segmentPoints.emplace_back(0.6f, lineY[i], 0);
            Obol::WireStyle style;
            style.widthScale = float(widths[i]);
            style.colorValid = true;
            style.color = colors[i];
            style.patternValid = true;
            style.linePattern = patterns[i];
            wire.styleRuns.push_back({i, style});
        }
        wire.bounds = SbBox3f(SbVec3f(-0.6f, -0.6f, 0), SbVec3f(0.6f, 0.6f, 0));
        builder.wire = wire;
        const auto geometry = Obol::cadAdmitPartGeometry(builder);
        ASSERT_TRUE(geometry);
        std::vector<Obol::PartUpdate> parts;
        std::vector<Obol::InstanceUpdate> instances;
        for (int i = 0; i < partCount; ++i) {
            const std::string name = "width-part-" + std::to_string(i);
            const auto part = Obol::CadIdBuilder::partId(name.c_str());
            parts.push_back({part, geometry.geometry, false});
            Obol::InstanceRecord record;
            record.part = part;
            record.style.hasColorOverride = true;
            record.style.color = SbColor4f(1, 1, 1, 1);
            instances.push_back({Obol::CadIdBuilder::instanceId(name.c_str()), record});
        }
        if (configuration.adaptive) {
            Obol::PartGeometryBuilder companion;
            Obol::WireRep progressive;
            progressive.segmentPoints = {SbVec3f(0.8f, -0.2f, 0), SbVec3f(0.8f, 0.2f, 0)};
            progressive.bounds = SbBox3f(progressive.segmentPoints[0], progressive.segmentPoints[1]);
            progressive.progressiveCuts.resize(1);
            progressive.progressiveCuts[0].segmentCount = 1;
            progressive.progressiveMinimumCut = progressive.progressiveResidentCut = 0;
            Obol::ProgressiveWireCluster cluster;
            cluster.bounds = progressive.bounds;
            cluster.residentCut = 0;
            cluster.ranges.push_back({0, 1, 0});
            progressive.progressiveClusters.push_back(cluster);
            companion.wire = std::move(progressive);
            const auto admitted = Obol::cadAdmitPartGeometry(companion);
            ASSERT_TRUE(admitted);
            const auto part = Obol::CadIdBuilder::partId("width-adaptive-companion");
            parts.push_back({part, admitted.geometry, false});
            Obol::InstanceRecord record;
            record.part = part;
            instances.push_back({Obol::CadIdBuilder::instanceId("width-adaptive-companion"), record});
        }
        ASSERT_TRUE(assembly->replaceScene(parts, instances));
        const int imageSize = 200;
        SoOffscreenRenderer renderer(SbViewportRegion(imageSize, imageSize));
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0, 0, 0));
        // Keep the context, IDs and positions while replacing only width runs.
        for (bool replaced : {false, true}) {
            const std::array<int, 3> expectedWidths = replaced ?
                std::array<int, 3>{{2, 4, 1}} : widths;
            if (replaced) {
                for (size_t line = 0; line < expectedWidths.size(); ++line)
                    builder.wire->styleRuns[line].style.widthScale =
                        float(expectedWidths[line]);
                const auto replacement = Obol::cadAdmitPartGeometry(builder);
                ASSERT_TRUE(replacement);
                std::vector<Obol::PartUpdate> updates;
                for (int i = 0; i < partCount; ++i)
                    updates.push_back({parts[i].part, replacement.geometry, false});
                ASSERT_TRUE(assembly->upsertParts(updates));
            }
            for (int baseWidth : {1, 2}) {
                std::vector<Obol::InstanceStyleUpdate> styles;
                for (const auto& instance : instances) {
                    auto style = instance.record.style;
                    style.lineWidth = float(baseWidth);
                    style.useGeometryColor = baseWidth == 1;
                    styles.push_back({instance.instance, style});
                }
                ASSERT_TRUE(assembly->updateInstanceStyles(styles));
                for (float height : {2.0f, 4.0f}) {
                    camera->height = height;
                    ASSERT_TRUE(render(renderer, root));
                    std::fprintf(stderr, "wire styles glsl=%d parts=%d adaptive=%d immediate=%d replaced=%d base=%d height=%g tier=%d direct=%d\n",
                        glsl, partCount, configuration.adaptive, configuration.immediate,
                        replaced, baseWidth,
                        double(height), assembly->lastRenderTier(), assembly->lastRenderUsedDirectSoftwareWire());
                    if (partCount == 128) EXPECT_EQ(assembly->lastRenderTier(), 3);
                    if (configuration.adaptive) EXPECT_EQ(assembly->lastRenderTier(), 1);
                    if (configuration.immediate) EXPECT_EQ(assembly->lastRenderTier(), 0);
                    const char *backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
                    EXPECT_EQ(assembly->lastRenderUsedDirectSoftwareWire(), configuration.direct &&
                        backend && std::strcmp(backend, "swrast") == 0);
                    const unsigned char *pixels = renderer.getBuffer();
                    for (size_t line = 0; line < lineY.size(); ++line) {
                        const int center = int((0.5f + lineY[line] / height) * imageSize);
                        int covered = 0;
                        size_t brightestOffset = 0;
                        unsigned int brightest = 0;
                        const int radius = 12;
                        for (int x = imageSize / 2 - 8;
                                x <= imageSize / 2 + 8; ++x) {
                            int candidateCovered = 0;
                            for (int y = center - radius;
                                    y <= center + radius; ++y) {
                                const size_t offset =
                                    (size_t(y) * imageSize + x) * 3;
                                const unsigned int intensity = std::max({
                                    pixels[offset], pixels[offset + 1],
                                    pixels[offset + 2]});
                                if (intensity > 128)
                                    ++candidateCovered;
                                if (intensity > brightest) {
                                    brightest = intensity;
                                    brightestOffset = offset;
                                }
                            }
                            covered = std::max(covered, candidateCovered);
                        }
                        EXPECT_NEAR(covered, expectedWidths[line] * baseWidth, 1)
                            << "line " << line << " glsl " << glsl << " parts " << partCount;
                        for (size_t channel = 0; channel < 3; ++channel) {
                            if (baseWidth != 1)
                                EXPECT_GT(pixels[brightestOffset + channel], 192);
                            else if (channel == line)
                                EXPECT_GT(pixels[brightestOffset + channel], 192);
                            else
                                EXPECT_LT(pixels[brightestOffset + channel], 32);
                        }
                        int patternedPixels = 0;
                        int bestPatternedPixels = 0;
                        const int halfSpan = std::max(
                            2, int(0.6f / height * imageSize) - 2);
                        for (int y = center - radius; y <= center + radius; ++y) {
                            patternedPixels = 0;
                            for (int x = imageSize / 2 - halfSpan;
                                    x <= imageSize / 2 + halfSpan; ++x) {
                                const size_t offset =
                                    (size_t(y) * imageSize + x) * 3;
                                if (std::max({pixels[offset], pixels[offset + 1],
                                        pixels[offset + 2]}) > 128)
                                    ++patternedPixels;
                            }
                            bestPatternedPixels =
                                std::max(bestPatternedPixels, patternedPixels);
                        }
                        const int span = 2 * halfSpan + 1;
                        if (line == 1) {
                            EXPECT_GT(bestPatternedPixels, span / 8);
                            /* A wide square software brush can close the
                             * three-pixel gaps of this dotted mask.  Thin
                             * strokes still provide the pattern witness. */
                            if (expectedWidths[line] * baseWidth <= 2)
                                EXPECT_LT(bestPatternedPixels, span * 3 / 4);
                        } else {
                            EXPECT_GT(bestPatternedPixels, span * 9 / 10);
                        }
                    }
                }
            }
        }
    }
}

namespace {
Obol::PartGeometryBuilder filledRing(bool screen)
{
    Obol::PartGeometryBuilder builder;
    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(-0.8f, -0.8f, 0), SbVec3f(0.8f, -0.8f, 0),
        SbVec3f(0.8f, 0.8f, 0), SbVec3f(-0.8f, 0.8f, 0),
        SbVec3f(-0.3f, -0.3f, 0), SbVec3f(0.3f, -0.3f, 0),
        SbVec3f(0.3f, 0.3f, 0), SbVec3f(-0.3f, 0.3f, 0)};
    mesh.indices = {0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    mesh.bounds = SbBox3f(SbVec3f(-0.8f, -0.8f, 0), SbVec3f(0.8f, 0.8f, 0));
    builder.shaded = mesh;
    builder.shadedIsFill = true;
    if (screen) {
        builder.displayPlane = Obol::CadDisplayPlane();
        builder.displayPlane->pixelsPerUnit = 64.0f;
    }
    return builder;
}
}

TEST_F(CadSubpixelProxyContracts, FilledAreasRejectIncompatibleGeometry)
{
    auto builder = filledRing(false);
    EXPECT_TRUE(Obol::cadAdmitPartGeometry(builder));
    Obol::FillStyle authored;
    authored.colorValid = true;
    authored.color = SbColor4f(1, 0, 0, 1);
    builder.shaded->styleRuns.push_back({0, authored});
    EXPECT_TRUE(Obol::cadAdmitPartGeometry(builder));
    builder.shadedIsFill = false;
    EXPECT_EQ(Obol::cadAdmitPartGeometry(builder).validation.error,
        Obol::CadGeometryError::InvalidTriangleStyle);
    builder.shadedIsFill = true;
    builder.shaded->styleRuns[0].style.color[0] = 2.0f;
    EXPECT_EQ(Obol::cadAdmitPartGeometry(builder).validation.error,
        Obol::CadGeometryError::InvalidTriangleStyle);
    builder.shaded->styleRuns.clear();
    builder.shadedCullBackfaces = true;
    EXPECT_EQ(Obol::cadAdmitPartGeometry(builder).validation.error, Obol::CadGeometryError::InvalidFill);
    builder.shadedCullBackfaces = false;
    builder.subpixelProxyEligible = true;
    EXPECT_EQ(Obol::cadAdmitPartGeometry(builder).validation.error, Obol::CadGeometryError::InvalidFill);
    builder.subpixelProxyEligible = false;
    builder.shaded.reset();
    EXPECT_EQ(Obol::cadAdmitPartGeometry(builder).validation.error, Obol::CadGeometryError::InvalidFill);
}

TEST_F(CadSubpixelProxyContracts, FilledAreasSurviveModesAndReplacement)
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const std::string savedGlsl = previousGlsl ? previousGlsl : "";
    const bool hadGlsl = previousGlsl != nullptr;
    const auto restore = [&](int *) {
        if (hadGlsl) setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", savedGlsl.c_str(), 1);
        else unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };
    int unused = 0;
    const std::unique_ptr<int, decltype(restore)> environment(&unused, restore);
    for (bool glsl : {false, true}) {
        setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", glsl ? "1" : "0", 1);
        for (bool screen : {false, true}) {
            auto *root = new SoSeparator;
            root->ref();
            const auto release = [](SoSeparator *node) { node->unref(); };
            const std::unique_ptr<SoSeparator, decltype(release)> owner(root, release);
            auto *camera = new SoOrthographicCamera;
            camera->position = SbVec3f(0, 0, 10);
            camera->nearDistance = 1.0f;
            camera->farDistance = 100.0f;
            root->addChild(camera);
            auto *view = cadViewState(root);
            auto *assembly = new SoCADAssembly;
            root->addChild(assembly);
            auto builder = filledRing(screen);
            // Exercise the shared unlit span with both a fill and a point;
            // the stroke across the hole must remain visible in shaded mode.
            Obol::WireRep wire;
            wire.segmentPoints = {SbVec3f(-0.25f, -0.2f, 0), SbVec3f(0.25f, -0.2f, 0)};
            wire.bounds = SbBox3f(wire.segmentPoints[0], wire.segmentPoints[1]);
            builder.wire = wire;
            Obol::PointRep points;
            points.positions = {SbVec3f(0, 0.2f, 0)};
            points.bounds = SbBox3f(points.positions[0], points.positions[0]);
            builder.points = points;
            const auto part = Obol::CadIdBuilder::partId("filled-ring");
            const auto instance = Obol::CadIdBuilder::instanceId("filled-ring-instance");
            Obol::InstanceRecord record;
            record.part = part;
            record.style.hasColorOverride = true;
            record.style.color = SbColor4f(1, 1, 1, 1);
            record.style.lineWidth = 3.0f;
            ASSERT_TRUE(admitAndUpsertPart(assembly, part, builder));
            ASSERT_TRUE(assembly->upsertInstances({{instance, record}}));
            const int imageSize = 200;
            SoOffscreenRenderer renderer(SbViewportRegion(imageSize, imageSize));
            renderer.setComponents(SoOffscreenRenderer::RGB);
            renderer.setBackgroundColor(SbColor(0, 0, 0));
            for (bool fill : {true, false, true}) {
                builder.shadedIsFill = fill;
                builder.shaded->styleRuns.clear();
                if (fill) {
                    Obol::FillStyle style;
                    style.colorValid = true;
                    style.color = SbColor4f(1, 0, 0, 1);
                    builder.shaded->styleRuns.push_back({0, style});
                }
                ASSERT_TRUE(admitAndUpsertPart(assembly, part, builder));
                for (const auto mode : {SoCADViewState::WIREFRAME,
                        SoCADViewState::SHADED, SoCADViewState::SHADED_WITH_EDGES,
                        SoCADViewState::HIDDEN_LINE}) {
                    view->drawMode = mode;
                    for (bool fast : {false, true}) {
                        view->softwareWireMode = fast ? SoCADViewState::SOFTWARE_WIRE_FAST :
                            SoCADViewState::SOFTWARE_WIRE_QUALITY;
                        for (float height : {2.0f, 4.0f}) {
                            camera->height = height;
                            SCOPED_TRACE(::testing::Message() << "fill=" << fill << " screen=" << screen
                                << " glsl=" << glsl << " mode=" << mode << " fast=" << fast << " height=" << height);
                            ASSERT_TRUE(render(renderer, root));
                            EXPECT_FALSE(assembly->lastRenderUsedDirectSoftwareWire());
                            const float scale = screen ? 64.0f : imageSize / height;
                            const auto pixelAt = [&](float x, float y) {
                                return SbVec2s(short(imageSize / 2 + x * scale), short(imageSize / 2 + y * scale));
                            };
                            const auto brightness = [&](const SbVec2s& pixel) {
                                return renderer.getBuffer()[(size_t(pixel[1]) * imageSize + pixel[0]) * 3];
                            };
                            const auto channel = [&](const SbVec2s& pixel,
                                    size_t component) {
                                return renderer.getBuffer()[
                                    (size_t(pixel[1]) * imageSize + pixel[0]) * 3 +
                                    component];
                            };
                            EXPECT_LT(brightness(pixelAt(0.12f, 0.1f)), 8);
                            if (fill) {
                                EXPECT_GT(brightness(pixelAt(0.6f, 0.0f)), 220);
                                EXPECT_LT(channel(pixelAt(0.6f, 0.0f), 1), 8);
                                EXPECT_LT(channel(pixelAt(0.6f, 0.0f), 2), 8);
                                EXPECT_GT(brightness(pixelAt(0.0f, -0.2f)), 220);
                                EXPECT_GT(brightness(pixelAt(0.0f, 0.2f)), 220);
                                EXPECT_EQ(assembly->lastRenderedWork().triangleCount, 8u);
                            } else if (mode == SoCADViewState::WIREFRAME) {
                                EXPECT_LT(brightness(pixelAt(0.6f, 0.0f)), 8);
                            }
                            SoRayPickAction pick(SbViewportRegion(imageSize, imageSize));
                            pick.setPoint(pixelAt(0.6f, 0));
                            pick.apply(root);
                            EXPECT_EQ(pick.getPickedPoint() != nullptr,
                                fill || mode != SoCADViewState::WIREFRAME);
                            pick.setPoint(pixelAt(0.12f, 0.1f));
                            pick.apply(root);
                            EXPECT_EQ(pick.getPickedPoint(), nullptr);
                        }
                    }
                }
            }
            record.style.useGeometryColor = false;
            record.style.color = SbColor4f(0, 1, 0, 1);
            ASSERT_TRUE(assembly->upsertInstances({{instance, record}}));
            view->drawMode = SoCADViewState::WIREFRAME;
            camera->height = 2.0f;
            ASSERT_TRUE(render(renderer, root));
            const float scale = screen ? 64.0f : imageSize / 2.0f;
            const SbVec2s replacementPixel(
                short(imageSize / 2 + 0.6f * scale), short(imageSize / 2));
            const size_t replacementOffset =
                (size_t(replacementPixel[1]) * imageSize + replacementPixel[0]) * 3;
            EXPECT_LT(renderer.getBuffer()[replacementOffset], 8);
            EXPECT_GT(renderer.getBuffer()[replacementOffset + 1], 220);
            EXPECT_LT(renderer.getBuffer()[replacementOffset + 2], 8);

            Obol::FillStyle backgroundMask;
            backgroundMask.backgroundMask = true;
            builder.shadedIsFill = true;
            builder.shaded->styleRuns = {{0, backgroundMask}};
            ASSERT_TRUE(admitAndUpsertPart(assembly, part, builder));
            const SbColor backgroundBottom(0.1f, 0.2f, 0.7f);
            const SbColor backgroundTop(0.7f, 0.8f, 0.1f);
            renderer.setBackgroundGradient(backgroundBottom, backgroundTop);
            for (const auto mode : {SoCADViewState::WIREFRAME,
                    SoCADViewState::SHADED, SoCADViewState::SHADED_WITH_EDGES,
                    SoCADViewState::HIDDEN_LINE}) {
                view->drawMode = mode;
                SCOPED_TRACE(::testing::Message() << "mask screen=" << screen
                    << " glsl=" << glsl << " mode=" << mode);
                ASSERT_TRUE(render(renderer, root));
                for (float y : {-0.4f, 0.4f}) {
                    const auto pixelAt = [&](float x) {
                        return SbVec2s(short(imageSize / 2 + x * scale),
                            short(imageSize / 2 + y * scale));
                    };
                    const SbVec2s masked = pixelAt(0.6f);
                    const SbVec2s exposed = pixelAt(0.9f);
                    for (size_t component = 0; component < 3; ++component) {
                        const size_t maskedOffset =
                            (size_t(masked[1]) * imageSize + masked[0]) * 3 +
                            component;
                        const size_t exposedOffset =
                            (size_t(exposed[1]) * imageSize + exposed[0]) * 3 +
                            component;
                        EXPECT_NEAR(renderer.getBuffer()[maskedOffset],
                            renderer.getBuffer()[exposedOffset], 5);
                    }
                }
            }
            renderer.clearBackgroundGradient();
        }
    }
}
