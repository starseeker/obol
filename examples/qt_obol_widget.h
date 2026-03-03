/* qt_obol_widget.h — Qt/Obol integration: QtObolContextManager + QtObolWidget
 *
 * This header provides a minimal but complete Qt widget for Obol-based 3D
 * rendering.  It is the Obol equivalent of Quarter's QuarterWidget, and
 * demonstrates that Obol's SoViewport API makes Qt integration far simpler
 * than the full Quarter approach.
 *
 * ── Quarter vs QtObolWidget ────────────────────────────────────────────────
 *
 * Quarter (~2 000 lines, 20+ files):
 *   • Subclasses QGLWidget/QOpenGLWidget
 *   • Manages SoRenderManager + SoEventManager as separate objects
 *   • SCXML-based navigation state machine (examiner, fly, …)
 *   • Interaction mode toggle (Alt-key), context menus, SpaceNavigator
 *   • Three-timer sensor bridge (idle, delay-timeout, timer-queue)
 *   • Qt Designer plugin, stereo/transparency/render-mode menus
 *
 * QtObolWidget (~300 lines, 1 file):
 *   • Same QOpenGLWidget base
 *   • Uses SoViewport directly (scene + camera + viewport in one object)
 *   • No navigation machinery — scene-graph nodes handle interaction
 *   • Same three-timer sensor bridge (the only non-trivial piece to port)
 *   • Inline Qt→Coin event translation (~100 lines, vs Quarter's 400)
 *
 * ── Platform-agnostic logic in Quarter that was relevant here ──────────────
 *
 * 1. Sensor bridge pattern (Quarter's SensorManager, ~170 lines):
 *      SoDB::getSensorManager()->setChangedCallback() is notified whenever
 *      the Coin sensor queue changes.  Three one-shot QTimers then process
 *      the appropriate queues (idle delay, timeout delay, timer queue).
 *      This pattern is reproduced verbatim in QtObolWidget below.
 *
 * 2. Event-translation abstraction (Quarter's Mouse + Keyboard devices):
 *      Qt events are mapped to SoLocation2Event, SoMouseButtonEvent, and
 *      SoKeyboardEvent.  Modifiers (Shift/Ctrl/Alt) and coordinate-system
 *      flip (Qt top-left vs Coin bottom-left origin) are handled here too.
 *      Reproduced inline in QtObolWidget rather than as separate classes,
 *      since with SoViewport the single processEvent() call path is simpler.
 *
 * 3. HiDPI device-pixel-ratio scaling:
 *      Quarter multiplies mouse positions by devicePixelRatio() so that
 *      Coin picks and navigation work correctly on Retina/HiDPI displays.
 *      QtObolWidget does the same in mapToObol().
 *
 * ── What Quarter does that is NOT reproduced here ─────────────────────────
 *
 *   • SCXML navigation (examiner.xml) — applications that need free-look
 *     camera control should add SoTrackballManip or a custom dragger.
 *   • Interaction-mode (Alt-key toggle), right-click context menu, stereo
 *     modes, transparency-type menu, SpaceNavigator, Qt Designer plugin.
 *
 * ── Usage ──────────────────────────────────────────────────────────────────
 *
 *   // 1. Initialize Obol (call before any Obol API).
 *   //    For an all-Qt application the GL context manager is not needed for
 *   //    the interactive widget, but IS needed if you also want to use
 *   //    SoOffscreenRenderer from the same process:
 *   //
 *   //    static QtObolContextManager mgr;
 *   //    SoDB::init(&mgr);
 *   //
 *   //    If you only need the interactive widget and no offscreen rendering,
 *   //    you can pass any other ContextManager (e.g. the OSMesa one) or NULL
 *   //    to SoDB::init() and QtObolWidget will still work because it drives
 *   //    SoGLRenderAction directly with Qt's current GL context.
 *
 *   SoDB::init(nullptr); // or pass a QtObolContextManager
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   // 2. Create the widget.
 *   QtObolWidget * viewer = new QtObolWidget(mainWindow);
 *   mainWindow->setCentralWidget(viewer);
 *
 *   // 3. Attach a scene graph.  If the root contains no camera, one is
 *   //    added automatically and viewAll() is called.
 *   viewer->setSceneGraph(myRoot);
 *
 *   // 4. Run the Qt event loop as usual.
 *   viewer->show();
 *   app.exec();
 */

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>

#include <Inventor/SoDB.h>
#include <Inventor/SoViewport.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbTime.h>
#include <Inventor/sensors/SoSensorManager.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>

#include <GL/gl.h>

// ============================================================================
// QtObolContextManager
//
// SoDB::ContextManager implementation backed by QOffscreenSurface and
// QOpenGLContext.  Use this when you want SoOffscreenRenderer to create
// and manage its own OpenGL contexts that share resources with Qt's context.
//
// This class is optional for the interactive QtObolWidget: the widget drives
// SoGLRenderAction directly within Qt's existing GL context and does not call
// SoOffscreenRenderer at all.  QtObolContextManager is only needed when the
// same application also uses SoOffscreenRenderer for off-screen captures.
// ============================================================================

class QtObolContextManager : public SoDB::ContextManager {
public:
    // shareCtx: when non-null, newly created GL contexts share resources
    // (textures, VBOs, display lists) with this context.  Pass the context
    // returned by QOpenGLWidget::context() after GL initialisation.
    explicit QtObolContextManager(QOpenGLContext * shareCtx = nullptr)
        : shareCtx_(shareCtx) {}

    void * createOffscreenContext(unsigned int /*w*/, unsigned int /*h*/) override {
        // QOffscreenSurface has no fixed size; the actual render dimensions
        // are determined by the viewport region passed to SoGLRenderAction.
        Ctx * c = new Ctx;
        QSurfaceFormat fmt;
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);

        c->surface = new QOffscreenSurface;
        c->surface->setFormat(fmt);
        c->surface->create();
        if (!c->surface->isValid()) {
            delete c->surface; delete c; return nullptr;
        }

        c->context = new QOpenGLContext;
        c->context->setFormat(fmt);
        if (shareCtx_) c->context->setShareContext(shareCtx_);
        if (!c->context->create()) {
            delete c->context; delete c->surface; delete c;
            return nullptr;
        }
        return c;
    }

    SbBool makeContextCurrent(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        c->prev     = QOpenGLContext::currentContext();
        c->prevSurf = c->prev ? c->prev->surface() : nullptr;
        return c->context->makeCurrent(c->surface) ? TRUE : FALSE;
    }

    void restorePreviousContext(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        if (c->prev && c->prevSurf)
            c->prev->makeCurrent(c->prevSurf);
        else
            c->context->doneCurrent();
    }

    void destroyContext(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        c->context->doneCurrent();
        delete c->context;
        delete c->surface;
        delete c;
    }

    // Resolve GL extension entry points via the current Qt context.
    void * getProcAddress(const char * name) override {
        QOpenGLContext * cur = QOpenGLContext::currentContext();
        if (!cur) return nullptr;
        return reinterpret_cast<void *>(cur->getProcAddress(name));
    }

private:
    struct Ctx {
        QOffscreenSurface * surface  = nullptr;
        QOpenGLContext    * context  = nullptr;
        QOpenGLContext    * prev     = nullptr;
        QSurface          * prevSurf = nullptr;
    };

    QOpenGLContext * shareCtx_;
};

// ============================================================================
// QtObolWidget
//
// QOpenGLWidget that renders a Coin/Obol scene graph.
//
// Architecture notes
// ------------------
// • Uses SoViewport (Obol's new single-viewport manager) for all scene,
//   camera, viewport-region, and background-colour management.
// • Calls SoGLRenderAction directly in paintGL() instead of going through
//   SoRenderManager.  This eliminates ~600 lines of Quarter's bookkeeping.
// • Sensor bridge: three one-shot QTimers connected to Coin's sensor queue
//   via SoDB::getSensorManager()->setChangedCallback().  This is the same
//   three-timer approach Quarter uses in SensorManager.cpp; it is the only
//   genuinely non-trivial piece of logic required to host Coin in any Qt app.
// • Event translation: inline mapping from QMouseEvent/QKeyEvent to Coin's
//   SoLocation2Event/SoMouseButtonEvent/SoKeyboardEvent.  Quarter spreads
//   this across EventFilter + Mouse.cpp + Keyboard.cpp; here it is ~100 lines.
// ============================================================================

class QtObolWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit QtObolWidget(QWidget * parent = nullptr)
        : QOpenGLWidget(parent)
        , cacheContext_(allocCacheContext())
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);

        // --- Sensor bridge ---------------------------------------------------
        // Three one-shot timers mirror Quarter's SensorManager pattern:
        //   idleTimer_  → processDelayQueue(TRUE)  when idle
        //   delayTimer_ → processDelayQueue(FALSE) after delay-sensor timeout
        //   timerTimer_ → processTimerQueue()      when a timer sensor fires
        idleTimer_.setSingleShot(true);
        delayTimer_.setSingleShot(true);
        timerTimer_.setSingleShot(true);

        connect(&idleTimer_,  &QTimer::timeout, this, &QtObolWidget::onIdle);
        connect(&delayTimer_, &QTimer::timeout, this, &QtObolWidget::onDelay);
        connect(&timerTimer_, &QTimer::timeout, this, &QtObolWidget::onTimer);

        SoDB::getSensorManager()->setChangedCallback(sensorQueueChangedCB, this);
    }

    ~QtObolWidget() override {
        SoDB::getSensorManager()->setChangedCallback(nullptr, nullptr);
    }

    // ---- Scene graph -------------------------------------------------------

    // Attach a scene graph.  If the root contains no SoCamera, a
    // SoPerspectiveCamera is inserted and viewAll() is called automatically.
    // Passing nullptr removes the current scene.
    //
    // Camera ownership: when a camera is auto-inserted, SoViewport::setCamera()
    // takes ownership (refs it).  The caller does NOT need to delete the camera;
    // it is released when setSceneGraph(nullptr) or setCamera(nullptr) is called
    // on the underlying SoViewport.
    void setSceneGraph(SoNode * root) {
        if (!root) {
            viewport_.setSceneGraph(nullptr);
            viewport_.setCamera(nullptr);
            return;
        }

        // Search for an existing camera in the scene.
        SoSearchAction sa;
        sa.setType(SoCamera::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);

        SoCamera * cam = nullptr;
        if (sa.getPath()) {
            SoFullPath * fp = static_cast<SoFullPath *>(sa.getPath());
            cam = static_cast<SoCamera *>(fp->getTail());
        }

        if (!cam)
            cam = new SoPerspectiveCamera;

        viewport_.setSceneGraph(root);
        viewport_.setCamera(cam);
        viewport_.viewAll();
        update();
    }

    SoNode * getSceneGraph() const { return viewport_.getSceneGraph(); }

    // ---- SoViewport access (advanced) --------------------------------------

    SoViewport       & getViewport()       { return viewport_; }
    const SoViewport & getViewport() const { return viewport_; }

    // ---- Background colour -------------------------------------------------

    void setBackgroundColor(const SbColor & c) {
        viewport_.setBackgroundColor(c);
        update();
    }
    const SbColor & backgroundColor() const {
        return viewport_.getBackgroundColor();
    }

protected:
    // ---- QOpenGLWidget overrides -------------------------------------------

    void initializeGL() override {
        glEnable(GL_DEPTH_TEST);
    }

    void resizeGL(int w, int h) override {
        // Scale by the device pixel ratio so that pick coordinates and camera
        // projections are in physical pixels on HiDPI (Retina) displays, exactly
        // as Quarter does via its devicePixelRatio() logic.
        const qreal dpr = devicePixelRatioF();
        viewport_.setWindowSize(SbVec2s((short)(w * dpr), (short)(h * dpr)));
    }

    void paintGL() override {
        const SbColor & bg = viewport_.getBackgroundColor();
        glClearColor(bg[0], bg[1], bg[2], 0.0f);
        glClear(static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        if (viewport_.getRoot()) {
            SoGLRenderAction action(viewport_.getViewportRegion());
            action.setCacheContext(cacheContext_);
            SoNode * root = viewport_.getRoot();
            action.apply(root);
        }
    }

    // ---- Mouse events → SoEvents -------------------------------------------

    void mousePressEvent(QMouseEvent * e) override {
        SoMouseButtonEvent ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        ev.setState(SoButtonEvent::DOWN);
        ev.setButton(qtButtonToCoin(e->button()));
        viewport_.processEvent(&ev);
        update();
    }

    void mouseReleaseEvent(QMouseEvent * e) override {
        SoMouseButtonEvent ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        ev.setState(SoButtonEvent::UP);
        ev.setButton(qtButtonToCoin(e->button()));
        viewport_.processEvent(&ev);
        update();
    }

    void mouseMoveEvent(QMouseEvent * e) override {
        SoLocation2Event ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        viewport_.processEvent(&ev);
        update();
    }

    void wheelEvent(QWheelEvent * e) override {
        SoMouseButtonEvent ev;
        // angleDelta().y() > 0 = scroll forward (away from user) = zoom in
        int delta = e->angleDelta().y();
        if (delta == 0) delta = e->angleDelta().x(); // horizontal wheel
        ev.setButton(delta > 0 ? SoMouseButtonEvent::BUTTON4
                                : SoMouseButtonEvent::BUTTON5);
        ev.setState(SoButtonEvent::DOWN);
#if QT_VERSION >= 0x060000
        ev.setPosition(mapToObol(e->position().toPoint()));
#else
        ev.setPosition(mapToObol(e->pos()));
#endif
        viewport_.processEvent(&ev);
        update();
    }

    // ---- Keyboard events → SoKeyboardEvent ---------------------------------

    void keyPressEvent(QKeyEvent * e) override {
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::DOWN);
        ev.setKey(qtKeyToCoin(e->key()));
        viewport_.processEvent(&ev);
    }

    void keyReleaseEvent(QKeyEvent * e) override {
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::UP);
        ev.setKey(qtKeyToCoin(e->key()));
        viewport_.processEvent(&ev);
    }

private slots:
    // ---- Coin sensor queue bridge ------------------------------------------
    // Called (via QueuedConnection) whenever Coin's sensor queue changes.
    // Starts or restarts the appropriate QTimer for each pending queue,
    // following the same three-timer pattern as Quarter's SensorManager.

    void onSensorQueueChanged() {
        SoSensorManager * sm = SoDB::getSensorManager();

        SbTime interval;
        if (sm->isTimerSensorPending(interval)) {
            interval -= SbTime::getTimeOfDay();
            int ms = qMax(1, (int)(interval.getValue() * 1000.0));
            if (!timerTimer_.isActive()) timerTimer_.start(ms);
            else timerTimer_.setInterval(ms);
        } else if (timerTimer_.isActive()) {
            timerTimer_.stop();
        }

        if (sm->isDelaySensorPending()) {
            idleTimer_.start(0);
            if (!delayTimer_.isActive()) {
                const SbTime & timeout = SoDB::getDelaySensorTimeout();
                if (timeout != SbTime::zero())
                    delayTimer_.start((int)(timeout.getValue() * 1000.0));
            }
        } else {
            idleTimer_.stop();
            delayTimer_.stop();
        }
    }

    void onIdle() {
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        onSensorQueueChanged();
    }

    void onDelay() {
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        onSensorQueueChanged();
    }

    void onTimer() {
        SoDB::getSensorManager()->processTimerQueue();
        onSensorQueueChanged();
    }

private:
    // ---- Helpers -----------------------------------------------------------

    // Coin's sensor-queue callback fires from any thread.  Route it back to
    // the Qt main thread via a queued signal so QTimer calls are thread-safe,
    // exactly as Quarter's SensorManager does via SignalThread + QTimer.
    static void sensorQueueChangedCB(void * data) {
        QtObolWidget * w = static_cast<QtObolWidget *>(data);
        QMetaObject::invokeMethod(w, "onSensorQueueChanged",
                                  Qt::QueuedConnection);
    }

    // Convert a Qt widget position to Coin pixel coordinates.
    // Qt origin = top-left; Coin origin = bottom-left.
    // Scale by devicePixelRatioF() for correct HiDPI behaviour.
    SbVec2s mapToObol(const QPoint & p) const {
        const qreal dpr = devicePixelRatioF();
        return SbVec2s(
            (short)(p.x() * dpr),
            (short)((height() - p.y() - 1) * dpr));
    }

    void applyModifiers(SoEvent * ev, QInputEvent * e) const {
        Qt::KeyboardModifiers m = e->modifiers();
        ev->setShiftDown((m & Qt::ShiftModifier)   ? TRUE : FALSE);
        ev->setCtrlDown ((m & Qt::ControlModifier) ? TRUE : FALSE);
        ev->setAltDown  ((m & Qt::AltModifier)     ? TRUE : FALSE);
    }

    static SoMouseButtonEvent::Button qtButtonToCoin(Qt::MouseButton b) {
        switch (b) {
        case Qt::LeftButton:   return SoMouseButtonEvent::BUTTON1;
        case Qt::RightButton:  return SoMouseButtonEvent::BUTTON2;
        case Qt::MiddleButton: return SoMouseButtonEvent::BUTTON3;
        default:               return SoMouseButtonEvent::ANY;
        }
    }

    // Minimal Qt→Coin key mapping.  Printable ASCII letters and digits are
    // handled by the range checks at the bottom; special keys are listed
    // explicitly.  Coin's SoKeyboardEvent::Key enum uses X11 keysym values
    // for special keys and lowercase ASCII values for letters/digits.
    static SoKeyboardEvent::Key qtKeyToCoin(int qtKey) {
        // Special keys
        switch (qtKey) {
        case Qt::Key_Escape:    return SoKeyboardEvent::ESCAPE;
        case Qt::Key_Tab:       return SoKeyboardEvent::TAB;
        case Qt::Key_Backspace: return SoKeyboardEvent::BACKSPACE;
        case Qt::Key_Return:
        case Qt::Key_Enter:     return SoKeyboardEvent::RETURN;
        case Qt::Key_Delete:    return SoKeyboardEvent::KEY_DELETE;
        case Qt::Key_Home:      return SoKeyboardEvent::HOME;
        case Qt::Key_End:       return SoKeyboardEvent::END;
        case Qt::Key_Left:      return SoKeyboardEvent::LEFT_ARROW;
        case Qt::Key_Right:     return SoKeyboardEvent::RIGHT_ARROW;
        case Qt::Key_Up:        return SoKeyboardEvent::UP_ARROW;
        case Qt::Key_Down:      return SoKeyboardEvent::DOWN_ARROW;
        case Qt::Key_PageUp:    return SoKeyboardEvent::PAGE_UP;
        case Qt::Key_PageDown:  return SoKeyboardEvent::PAGE_DOWN;
        case Qt::Key_Space:     return SoKeyboardEvent::SPACE;
        case Qt::Key_F1:        return SoKeyboardEvent::F1;
        case Qt::Key_F2:        return SoKeyboardEvent::F2;
        case Qt::Key_F3:        return SoKeyboardEvent::F3;
        case Qt::Key_F4:        return SoKeyboardEvent::F4;
        case Qt::Key_F5:        return SoKeyboardEvent::F5;
        case Qt::Key_F6:        return SoKeyboardEvent::F6;
        case Qt::Key_F7:        return SoKeyboardEvent::F7;
        case Qt::Key_F8:        return SoKeyboardEvent::F8;
        case Qt::Key_F9:        return SoKeyboardEvent::F9;
        case Qt::Key_F10:       return SoKeyboardEvent::F10;
        case Qt::Key_F11:       return SoKeyboardEvent::F11;
        case Qt::Key_F12:       return SoKeyboardEvent::F12;
        default: break;
        }
        // Letters: Qt::Key_A–Key_Z (0x41–0x5A, uppercase) map to
        // SoKeyboardEvent::A–Z (0x61–0x7A, lowercase ASCII 'a'–'z').
        // SoKeyboardEvent::A == 0x61; Qt::Key_A == 0x41; the offset 0x20
        // is absorbed by computing (qtKey - Qt::Key_A) and adding to A.
        if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
            return static_cast<SoKeyboardEvent::Key>(
                SoKeyboardEvent::A + (qtKey - Qt::Key_A));
        // Digits: Qt::Key_0–Key_9 → SoKeyboardEvent::NUMBER_0–NUMBER_9
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
            return static_cast<SoKeyboardEvent::Key>(
                SoKeyboardEvent::NUMBER_0 + (qtKey - Qt::Key_0));
        return SoKeyboardEvent::ANY;
    }

    // Allocate a globally unique GL cache context ID for this widget.
    // Each QOpenGLWidget has its own GL context; Coin uses the cache context
    // to partition its display-list / VBO caches per context.
    static uint32_t allocCacheContext() {
        static uint32_t s_nextId = 1;
        return s_nextId++;
    }

    SoViewport  viewport_;
    uint32_t    cacheContext_;

    // Sensor bridge timers (same three-timer pattern as Quarter's SensorManager)
    QTimer      idleTimer_;   // processes delay queue on idle (0 ms)
    QTimer      delayTimer_;  // processes delay queue after timeout
    QTimer      timerTimer_;  // fires when a Coin timer sensor is due
};
