// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <mtdev.h>
#include <mtdev-plumbing.h>
#include <time.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#define GL_GLEXT_PROTOTYPES 1

//#define ENABLE_LIGHTING

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AppPlatform.hpp"

static constexpr int batchSize = 20; // batch size

struct AppPlatform::Opaque
{
    // window
    Display* xDisplayPtr = 0;
    Colormap xColormap;
    Window xWindow;
    Atom xWMDeleteWindowAtom;
    XVisualInfo *glVisualPtr;
    // opengl
    GLXContext glContext;
    uint frameTimer;
    uint frameTimerValue;
    // touchpad
    struct mtdev mtdevState;
    int mtEventSlot = 0; // ie. finger 0
    Event::Kind mtEventSlotStateArr[ Event::MaxTouch ] = {Event::Kind::Nil,Event::Kind::Nil}; // holds trackids
    struct input_event mtEventArr[batchSize];
    struct AxisInfo
    {
        float min, max, rez;
    } axisInfo[2];
    Event touchEvent;
    // mouse
    bool mousePressed = false;
    Event mouseEvent;
    // platform
    struct timespec timer;
    int windowFD, touchFD;
    fd_set fdReadSet;
    // time
    long tick_newMSec = 0;
    long tick_oldMSec = 0;
    long tick_deltaMSec = 0;
    long tick_deltaMSecAvg = 0;
    float tick_deltaSecAvg = 0;
};

uint AppPlatform::GetWidth() const { return 600; }
uint AppPlatform::GetHeight() const { return 400; }

void AppPlatform::BeginFrame()
{
    glClearColor( .0f, .0f, .0f, .0f ); // rgba
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glBeginQuery( GL_TIME_ELAPSED, pOpaque->frameTimer );
}

void AppPlatform::EndFrame()
{
    glFlush();
    glEndQuery( GL_TIME_ELAPSED );

    if(showDepthBuffer)
    {
        // https://stackoverflow.com/q/2746051/968363
        int width, height;

        int iv[4];
        glGetIntegerv( GL_VIEWPORT, iv );
        width = iv[2];
        height = iv[3];

        float *data = new float[width * height];

        glReadPixels( 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, data );
        glDrawPixels( width, height, GL_LUMINANCE, GL_FLOAT, data );

        delete[] data;
    }

    glXSwapBuffers( pOpaque->xDisplayPtr, pOpaque->xWindow );

    int done = 0;
    while( !done ) glGetQueryObjectiv( pOpaque->frameTimer, GL_QUERY_RESULT_AVAILABLE, &done );
    glGetQueryObjectuiv( pOpaque->frameTimer, GL_QUERY_RESULT, &pOpaque->frameTimerValue );

    frameDuration = frameDuration * .75f + (float)pOpaque->frameTimerValue * 10E-9f * .25f;
}
/*
void AppPlatform::LoadProjection(const glm::mat4& m)
{
    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( glm::value_ptr(m) );
}

void AppPlatform::LoadModel(const glm::mat4& m)
{
    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( glm::value_ptr(m) );
}

void AppPlatform::DupAndMultModel(const glm::mat4& m)
{
    glPushMatrix();
    glMultMatrixf
    glm::mat4 m0 = pOpaque->modelStack.front();
    glm::mat4 m1 = m0 * m;
    pOpaque->modelStack.push_front(m1);

    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( glm::value_ptr(m1) );
}

void AppPlatform::PopModel()
{
    glPopMatrix();
}
*/
void AppPlatform::Bind(const char* szWindowname, const char* szDevName)
{
    pOpaque = new Opaque();
    pOpaque->timer.tv_nsec = 1000 * 1000 * 100; // 100ms
    pOpaque->timer.tv_sec = 0;

    setlocale(LC_ALL, "");
    XSupportsLocale();
    XSetLocaleModifiers("@im=none");

    // window

    pOpaque->xDisplayPtr = XOpenDisplay( NULL );
    assert( pOpaque->xDisplayPtr != NULL );

    XkbSetDetectableAutoRepeat(pOpaque->xDisplayPtr, true, 0); // disable auto-repeat of KeyRelease event

    // drawing window

    Window xRootWindow;
    xRootWindow = DefaultRootWindow( pOpaque->xDisplayPtr );

    GLint glVisualPrefArr[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};

    //XVisualInfo *glVisualPtr;
    pOpaque->glVisualPtr = glXChooseVisual( pOpaque->xDisplayPtr, 0, glVisualPrefArr );
    assert( pOpaque->glVisualPtr != NULL );

//        /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
//         * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
//         * As soon as we picked a EGLConfig, we can safely reconfigure the
//         * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID.
//         *
//         * May be X11 related: see EGL_NATIVE_VISUAL_ID glXChooseVisual XVisualInfo */
//        EGLint format;
//        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_NATIVE_VISUAL_ID, &format);
//        LOGWgl("eglGetConfigAttrib");

    pOpaque->xColormap = XCreateColormap( pOpaque->xDisplayPtr, xRootWindow, pOpaque->glVisualPtr->visual, AllocNone );

    XSetWindowAttributes xSetWindowAttributes;
    memset(&xSetWindowAttributes, 0, sizeof(xSetWindowAttributes));
    xSetWindowAttributes.colormap = pOpaque->xColormap;
    xSetWindowAttributes.event_mask =
        FocusChangeMask |
        ExposureMask |
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask |
        PointerMotionMask |
        StructureNotifyMask;

    pOpaque->xWindow = XCreateWindow(
        pOpaque->xDisplayPtr, xRootWindow,
        0, 0, GetWidth(), GetHeight(),
        0, pOpaque->glVisualPtr->depth,
        InputOutput, pOpaque->glVisualPtr->visual, CWColormap | CWEventMask, &xSetWindowAttributes );

    XMapWindow( pOpaque->xDisplayPtr, pOpaque->xWindow );

    // tell window-manager our window title
    XStoreName( pOpaque->xDisplayPtr, pOpaque->xWindow, szWindowname );

    // register a new atom for window-manager close-events for delivery to the event-queue
    pOpaque->xWMDeleteWindowAtom = XInternAtom( pOpaque->xDisplayPtr, "WM_DELETE_WINDOW", True );
    XSetWMProtocols( pOpaque->xDisplayPtr, pOpaque->xWindow, &pOpaque->xWMDeleteWindowAtom, 1 );

    XSync( pOpaque->xDisplayPtr, false );
    XFlush( pOpaque->xDisplayPtr );

    // get fd/socket of X display
    pOpaque->windowFD = ConnectionNumber( pOpaque->xDisplayPtr );

    // opengl

    pOpaque->glContext = glXCreateContext( pOpaque->xDisplayPtr, pOpaque->glVisualPtr, NULL, GL_TRUE );
    glXMakeCurrent( pOpaque->xDisplayPtr, pOpaque->xWindow, pOpaque->glContext );

    glEnable( GL_DEPTH_TEST );
    glDepthRange( 0.f, 1.f );
    //glDepthRange( 1.f, 0.f ); // make par-of-the helix visible but not cursor! :(

    //glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ); // ?
    glShadeModel(GL_FLAT); // GL_SMOOTH vs GL_FLAT
    //glFrontFace(GL_CW); // GL_CW vs GL_CCW is default

#ifdef ENABLE_LIGHTING
    glEnable(GL_LIGHTING);
    //glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, glm::value_ptr(glm::vec3(.5,.5,.5)) );
    glEnable(GL_LIGHT0);

    // Create light components
    float ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    float diffuseLight[] = { 0.8f, 0.8f, 0.8, 1.0f };
    float specularLight[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    float position[] = { 50.0f, 50.0f, 50.0f, 1.0f };

    // Assign created components to GL_LIGHT0
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    glLightfv(GL_LIGHT0, GL_POSITION, position); // in viewspace?
#endif

    XWindowAttributes xWindowAttributes;
    XGetWindowAttributes( pOpaque->xDisplayPtr, pOpaque->xWindow, &xWindowAttributes );
    glViewport( 0, 0, xWindowAttributes.width, xWindowAttributes.height );

    glGenQueries(1, &pOpaque->frameTimer);

    frameDuration = 0.f;

    // touch

    char szDevNode[32];
    int rc;

    // find device node for specified named device
    // input.c from: input-utils_1.0.orig.tar.gz
    for( int i=0; i<32; i++ )
    {
        char buf[32];

        snprintf( szDevNode, sizeof(szDevNode), "/dev/input/event%d", i );
        pOpaque->touchFD = open( szDevNode, O_RDONLY );
        if(pOpaque->touchFD == -1)
            break; // not found

        rc = ioctl( pOpaque->touchFD, EVIOCGNAME(sizeof(buf)), buf );
        assert(rc >= 0);

        if( strncmp( buf, szDevName, sizeof(buf) ) == 0 ) { break; }

        close( pOpaque->touchFD );
        pOpaque->touchFD = -1;
    }
    if( pOpaque->touchFD == -1 )
    {
        printf("Touch Device '%s' not found.\n", szDevName);
        printf("Enabling mouse/keyboard emulation mode. Paint-mode keys are: q w e r\n");
    }
    else
    {
        rc = fcntl( pOpaque->touchFD, F_SETFL, fcntl(pOpaque->touchFD, F_GETFL) | O_NONBLOCK); // non-blocking
        assert( rc >= 0 );
        rc = ioctl( pOpaque->touchFD, EVIOCGRAB, 1 ); // grab
        assert( rc >= 0 );

        input_absinfo mtdevAxis[2];
        rc = ioctl( pOpaque->touchFD, EVIOCGABS( ABS_X ), &mtdevAxis[0] ); // axis info
        assert( rc >= 0 );
        rc = ioctl( pOpaque->touchFD, EVIOCGABS( ABS_Y ), &mtdevAxis[1] ); // axis info
        assert( rc >= 0 );
        pOpaque->axisInfo[0] = { float(mtdevAxis[0].minimum), float(mtdevAxis[0].maximum), float(mtdevAxis[0].resolution) };
        pOpaque->axisInfo[1] = { float(mtdevAxis[1].minimum), float(mtdevAxis[1].maximum), float(mtdevAxis[1].resolution) };

        rc = mtdev_open( &pOpaque->mtdevState, pOpaque->touchFD ); // open for events
        assert( rc >= 0 );
    }

    // start module in untouched state
    pOpaque->touchEvent.kind = Event::Touch;
    pOpaque->touchEvent.u.touch.toKind = Event::Kind::Nil;
    pOpaque->touchEvent.u.touch.id = 0; // finger 0

    pOpaque->mouseEvent = pOpaque->touchEvent; // same defaults for emulation
}
void AppPlatform::Release()
{
    // window

    glXMakeCurrent( pOpaque->xDisplayPtr, None, NULL );
    glXDestroyContext( pOpaque->xDisplayPtr, pOpaque->glContext );

    XSync( pOpaque->xDisplayPtr, true );
    XDestroyWindow( pOpaque->xDisplayPtr, pOpaque->xWindow );
    XFree( pOpaque->glVisualPtr );
    XFreeColormap( pOpaque->xDisplayPtr, pOpaque->xColormap );

    XCloseDisplay( pOpaque->xDisplayPtr );

    // touch

    if( pOpaque->touchFD != -1 )
    {
        mtdev_close( &pOpaque->mtdevState );
        ioctl( pOpaque->touchFD, EVIOCGRAB, 0 );
        close( pOpaque->touchFD );
    }

    // platform

    delete pOpaque;
    pOpaque = 0;
}
void AppPlatform::Tick(std::function<void(const Event &)> fnEvent)
{
    FD_ZERO( &pOpaque->fdReadSet );
    FD_SET( pOpaque->windowFD, &pOpaque->fdReadSet ); // set x-event bit
    if( pOpaque->touchFD != -1 )
        FD_SET( pOpaque->touchFD, &pOpaque->fdReadSet ); // set touch-event bit
    int nfds = std::max(pOpaque->windowFD, pOpaque->touchFD) + 1; // crazy posix!
    int fdReadyCount = pselect( nfds, &pOpaque->fdReadSet, NULL, NULL, &pOpaque->timer, NULL ); // nb: select(...) destroys timer value but not pselect(...)
    assert( fdReadyCount >= 0 );

    {
        struct timespec spec;
        clock_gettime( CLOCK_REALTIME, &spec );
        pOpaque->tick_newMSec = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
        if( pOpaque->tick_oldMSec == 0 ) pOpaque->tick_oldMSec = pOpaque->tick_newMSec;
        if( pOpaque->tick_newMSec < pOpaque->tick_oldMSec ) pOpaque->tick_newMSec = pOpaque->tick_oldMSec;
        pOpaque->tick_deltaMSec = ( pOpaque->tick_newMSec - pOpaque->tick_oldMSec );
        pOpaque->tick_oldMSec = pOpaque->tick_newMSec;
        deltaMSec = uint32_t(pOpaque->tick_newMSec);
        pOpaque->tick_deltaMSecAvg = pOpaque->tick_deltaMSecAvg * 8 / 10 + pOpaque->tick_deltaMSec * 2 / 10;
        pOpaque->tick_deltaSecAvg = float(pOpaque->tick_deltaMSecAvg) / 1000.f;
        deltaSecAvg = pOpaque->tick_deltaSecAvg;
    }

    Event event;

    if(fdReadyCount == 0)
    {
        event.kind = Event::Tick;
        fnEvent(event);
    }

    if( pOpaque->touchFD != -1  && FD_ISSET( pOpaque->touchFD, &pOpaque->fdReadSet ))
    {
        // https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt
        const float scaleFudge = 10.f; // is the driver confused between cm and mm?
        const float coefX = GetWidth() / ( scaleFudge * pOpaque->axisInfo[0].max / pOpaque->axisInfo[0].rez );
        const float coefY = GetHeight() / ( scaleFudge * pOpaque->axisInfo[1].max / pOpaque->axisInfo[1].rez );
        int batchCount = mtdev_get( &pOpaque->mtdevState, pOpaque->touchFD, pOpaque->mtEventArr, batchSize ); // requires O_NONBLOCK
        for( int i = 0; i < batchCount; i++ )
        {
            switch( pOpaque->mtEventArr[i].code )
            {
                case ABS_MT_SLOT:
                    if(pOpaque->mtEventArr[i].value < Event::MaxTouch)
                        pOpaque->mtEventSlot = pOpaque->mtEventArr[i].value;
                    break;
                case ABS_MT_TRACKING_ID:
                    if(pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] == Event::Kind::Nil)
                        // start drawing when coming from a nil state
                        pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] = Event::Kind::Begin;
                    else if(pOpaque->mtEventArr[i].value == -1)
                        // finish drawing when tracking is becomes invalid
                        pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] = Event::Kind::End;
                    break;
                case ABS_MT_POSITION_X:
                    pOpaque->touchEvent.u.touch.x = int32_t(std::lround(coefX * float(pOpaque->mtEventArr[i].value)));
                    break;
                case ABS_MT_POSITION_Y:
                    pOpaque->touchEvent.u.touch.y = int32_t(std::lround(coefY * float(pOpaque->mtEventArr[i].value)));
                    break;
                case EV_SYN:
                    // catch end-bounce
                    if(pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] == Event::Kind::Nil)
                        break;

                    pOpaque->touchEvent.u.touch.id = pOpaque->mtEventSlot;
                    pOpaque->touchEvent.u.touch.toKind = pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ];
                    fnEvent(pOpaque->touchEvent);

                    // convert 'begin's to 'continue's
                    if(pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] == Event::Kind::Begin)
                        pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] = Event::Kind::Move;

                    // ensure we catch end-bounce
                    if(pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] == Event::Kind::End)
                        pOpaque->mtEventSlotStateArr[ pOpaque->mtEventSlot ] = Event::Kind::Nil;
                    break;
                default: break;
            }
        }
    }

    if(FD_ISSET( pOpaque->windowFD, &pOpaque->fdReadSet ))
    {
        KeySym keysym = 0;
        char buf[2];

        XEvent xEvent;
        while( XPending( pOpaque->xDisplayPtr ) ) // gobble messages in batches
        {
            XNextEvent( pOpaque->xDisplayPtr, &xEvent );
            if (XFilterEvent(&xEvent, pOpaque->xWindow)) continue; // additional processing
            switch( xEvent.type )
            {
                case Expose:
                    event.kind = Event::Adornment;
                    event.u.adornment.adKind = Event::Kind::Refresh;
                    fnEvent(event);
                    break;
                case ClientMessage:
                    if( (ulong)xEvent.xclient.data.l[0] == pOpaque->xWMDeleteWindowAtom )
                    {
                        event.kind = Event::Adornment;
                        event.u.adornment.adKind = Event::Kind::Close;
                        fnEvent(event);
                    }
                    break;
                case FocusIn:
                case FocusOut:
                    event.kind = Event::Adornment;
                    event.u.adornment.adKind =
                        xEvent.xfocus.type == FocusIn ? Event::Kind::Resume : Event::Kind::Pause;
                    fnEvent(event);
                    break;
                case KeyPress:
                    event.kind = Event::Key;
                    if(0 < XLookupString(&xEvent.xkey, buf, 1, &keysym, nullptr))
                    {
                        event.u.key.key = (char) keysym; // may be non-ascii
                        event.u.key.press = true;
                        fnEvent( event );
                    }
                    break;
                case KeyRelease:
                    event.kind = Event::Key;
                    if(0 < XLookupString(&xEvent.xkey, buf, 1, &keysym, nullptr))
                    {
                        event.u.key.key = (char) keysym; // may be non-ascii
                        event.u.key.press = false;
                        fnEvent( event );
                    }
                    break;
                case ButtonPress:
                    pOpaque->mousePressed = true;
                    pOpaque->mouseEvent.u.touch.toKind = Event::Kind::Begin;
                    pOpaque->mouseEvent.u.touch.x = xEvent.xmotion.x;
                    pOpaque->mouseEvent.u.touch.y = xEvent.xmotion.y;
                    fnEvent(pOpaque->mouseEvent);
                    break;
                case ButtonRelease:
                    pOpaque->mousePressed = false;
                    pOpaque->mouseEvent.u.touch.toKind = Event::Kind::End;
                    pOpaque->mouseEvent.u.touch.x = xEvent.xmotion.x;
                    pOpaque->mouseEvent.u.touch.y = xEvent.xmotion.y;
                    fnEvent(pOpaque->mouseEvent);
                    break;
                case MotionNotify:
                    if(pOpaque->mousePressed)
                    {
                        pOpaque->mouseEvent.u.touch.toKind = Event::Kind::Move;
                        pOpaque->mouseEvent.u.touch.x = xEvent.xmotion.x;
                        pOpaque->mouseEvent.u.touch.y = xEvent.xmotion.y;
                        fnEvent( pOpaque->mouseEvent );
                    }
                    break;
                default: break;
            }
        }
    }
}

extern void app_main();

int main(int argc, char *argv[])
{
    app_main();

    return 0;
}
