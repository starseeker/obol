/* qt_obol_example.cpp — Minimal Qt/Obol application
 *
 * The Obol equivalent of Quarter's minimal.cpp example.  Shows a yellow cone
 * in a QtObolWidget.  Build with:
 *
 *   cmake -DOBOL_BUILD_QT_EXAMPLE=ON ...
 *
 * Compare with the Quarter version (quarter/src/examples/minimal.cpp):
 *
 *   Quarter:                          | Obol (this file):
 *   ----------------------------------+----------------------------------
 *   #include <Quarter/Quarter.h>      | (no separate init library)
 *   #include <Quarter/QuarterWidget.h>| #include "qt_obol_widget.h"
 *   Quarter::init();                  | (SoDB::init handles everything)
 *   QuarterWidget * viewer = new...   | QtObolWidget * viewer = new...
 *   viewer->setNavigationModeFile(...)| (not needed; scene handles it)
 *   viewer->setSceneGraph(root);      | viewer->setSceneGraph(root);
 *   app.exec();                       | app.exec();
 *   Quarter::clean();                 | SoDB::finish();
 *
 * Key observations:
 *  1. No separate "Quarter::init()" is needed; Obol's SoDB::init() suffices.
 *  2. No navigation mode file is required; the scene graph itself can embed
 *     an SoTrackballManip or similar node for interactive rotation.
 *  3. The QtObolContextManager is only needed if SoOffscreenRenderer is also
 *     used in the same process; it is omitted here for simplicity.
 */

#include <QApplication>
#include <QMainWindow>

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbColor.h>

// Self-contained Qt/Obol integration header (no separate library needed).
#include "qt_obol_widget.h"

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    // ── 1. Initialise Obol ─────────────────────────────────────────────────
    // SoDB::init(nullptr) is sufficient when only using the interactive widget.
    // Pass a QtObolContextManager instead if you also need SoOffscreenRenderer.
    SoDB::init(nullptr);
    SoNodeKit::init();
    SoInteraction::init();

    // ── 2. Build a simple scene graph ──────────────────────────────────────
    SoSeparator * root = new SoSeparator;
    root->ref();

    // Headlight-style directional light
    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    // Yellow cone (same as Quarter's minimal example)
    SoBaseColor * col = new SoBaseColor;
    col->rgb = SbColor(1.0f, 1.0f, 0.0f);
    root->addChild(col);

    root->addChild(new SoCone);

    // ── 3. Create the widget ───────────────────────────────────────────────
    QMainWindow * mainwin = new QMainWindow;
    mainwin->setWindowTitle("Qt/Obol minimal example");
    mainwin->resize(800, 600);

    QtObolWidget * viewer = new QtObolWidget(mainwin);
    mainwin->setCentralWidget(viewer);

    // Attach the scene (a camera is added automatically if none is present).
    viewer->setSceneGraph(root);

    // ── 4. Show and run ───────────────────────────────────────────────────
    mainwin->show();
    app.exec();

    // ── 5. Cleanup ────────────────────────────────────────────────────────
    root->unref();
    delete mainwin;
    SoDB::finish();

    return 0;
}
