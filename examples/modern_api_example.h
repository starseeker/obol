/* Modern C++ example demonstrating context management API changes */

// NOTE: The ContextProvider API has been removed from SoOffscreenRenderer
// Context management should now be done via SoDB::init(context_manager) before library initialization

#ifdef OBOL_OSMESA_BUILD
/* OSMesa-specific code - Context management now handled globally */
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <memory>

// Example: Modern usage without ContextProvider API
inline void demonstrateModernOSMesaUsage() {
    // Context management is now handled globally via SoDB::init(context_manager)
    // No need for per-renderer context providers
    
    SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    
    // OpenGL capabilities are queried on the renderer instance, which uses
    // the per-instance context manager if set, else the global singleton.
    int major, minor, release;
    renderer.getOpenGLVersion(major, minor, release);
    
    SbBool hasFBO = renderer.hasFramebufferObjectSupport();
    SbBool hasExtension = renderer.isOpenGLExtensionSupported("GL_ARB_vertex_buffer_object");
    SbBool hasGL3 = renderer.isVersionAtLeast(3, 0);
}

#else
/* System OpenGL code - Uses standard context creation */
#include <GL/gl.h>
#include <GL/glu.h>
#include <Inventor/SoOffscreenRenderer.h>

// NOTE: With modern Coin3D APIs, applications can use the high-level
// SoOffscreenRenderer directly without needing custom context providers
// for most standard use cases.

// Example: Standard usage without custom context provider
inline void demonstrateModernStandardUsage() {
    // Check OpenGL capabilities using the renderer instance
    SbViewportRegion viewport(800, 600);
    SoOffscreenRenderer renderer(viewport);

    // Capabilities are queried via the instance, using per-instance context
    // manager if set, otherwise the global singleton.
    int major, minor, release;
    renderer.getOpenGLVersion(major, minor, release);
    
    bool hasModernOpenGL = renderer.isVersionAtLeast(3, 0);
    bool hasFBOSupport = renderer.hasFramebufferObjectSupport();
    
    printf("OpenGL Version: %d.%d.%d\n", major, minor, release);
    printf("Modern OpenGL (3.0+): %s\n", hasModernOpenGL ? "Yes" : "No");
    printf("FBO Support: %s\n", hasFBOSupport ? "Yes" : "No");
}

#endif

// ============================================================================
// Modern usage example - replacing old cc_glglue style code
// ============================================================================

/*
 * OLD WAY (no longer available):
 * 
 * #include <Inventor/SoOffscreenRenderer.h>
 * class MyProvider : public SoOffscreenRenderer::ContextProvider { ... };
 * SoOffscreenRenderer::setContextProvider(&provider);
 * SoOffscreenRenderer::hasFramebufferObjectSupport();
 * 
 * NEW WAY (current approach):
 * 
 * Context management should be done via SoDB::init(context_manager) at initialization.
 * OpenGL capabilities are queried via a renderer instance, which uses the
 * per-instance context manager if set, otherwise the global singleton:
 * 
 * #include <Inventor/SoOffscreenRenderer.h>
 * SbViewportRegion vp(256, 256);
 * SoOffscreenRenderer ren(vp);
 * ren.hasFramebufferObjectSupport();
 * ren.getOpenGLVersion(major, minor, release);
 * ren.isVersionAtLeast(3, 0);
 */

#endif // OBOL_OSMESA_BUILD