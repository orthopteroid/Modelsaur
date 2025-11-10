// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include <csignal>

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <mtdev.h>
#include <mtdev-plumbing.h>
#include <sys/stat.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>

#define GL_GLEXT_PROTOTYPES
#include "GL9.hpp"

#include "AppPlatform.hpp"
#include "AppLog.hpp"

#define ENABLE_QUICKBAIL

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const int PlatformWidth = 600;
const int PlatformHeight = 400;
const int TickRateMSec = 50;

struct State
{
    static constexpr int batchSize = 20;

    // window
    Display* xDisplayPtr = 0;
    Colormap xColormap;
    Window xWindow;
    Atom xWMDeleteWindowAtom;
    XVisualInfo *xVisualPtr;
    GLXFBConfig glxFBConfig;

    // touchpad
    struct mtdev mtdevState;
    int mtEventSlot = 0; // ie. finger 0
    AppPlatform::Event::Kind mtEventSlotStateArr[ AppPlatform::Event::MaxTouch ] = {AppPlatform::Event::Kind::Nil,AppPlatform::Event::Kind::Nil}; // holds trackids
    struct input_event mtEventArr[batchSize];
    struct AxisInfo
    {
        float min, max, rez;
    } axisInfo[2];
    AppPlatform::Event touchEvent;

    AppPlatform::Event touchEventArr[AppPlatform::Event::MaxTouch];

    // mouse
    bool mousePressed = false;
    AppPlatform::Event mouseEvent;

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

    std::function<void(void)> rebindFn = 0;
    std::function<void(void)> releaseFn = 0;

    char* szInternalPath;
    char* szExternalPath;

};
static State state;

Display* Linux_GetDisplayPtr() { return state.xDisplayPtr; }
Window Linux_GetWindow() { return state.xWindow; }
GLXFBConfig Linux_GetFBConfig() { return state.glxFBConfig; }

const char* Platform_InternalPath() { return state.szInternalPath; }
const char* Platform_ExternalPath() { return state.szExternalPath; }

void AppPlatform::Bind(std::function<void(void)> pfnRebind, std::function<void(void)> pfnRelease, const char* szWindowname, const char* szDevName)
{
    state.szInternalPath = (char*)malloc(1024);
    getcwd(state.szInternalPath, 1024);
    strcat(state.szInternalPath, "/Prefs");

    {
        auto szStat = (0 == mkdir( Platform_InternalPath(), 0644 )) ? "created" : "failed";
        AppLog::Info(__FILENAME__, "%s %s %s", __func__, Platform_InternalPath(), szStat );
    }

    state.szExternalPath = (char*)malloc(1024);
    getcwd(state.szExternalPath, 1024);
    strcat(state.szExternalPath, "/Library");

    {
        auto szStat = (0 == mkdir( Platform_ExternalPath(), 0644 )) ? "created" : "failed";
        AppLog::Info(__FILENAME__, "%s %s %s", __func__, Platform_ExternalPath(), szStat );
    }

    state.rebindFn = pfnRebind;
    state.releaseFn = pfnRelease;

    state.timer.tv_nsec = 1000 * 1000 * TickRateMSec;
    state.timer.tv_sec = 0;

    setlocale(LC_ALL, "");
    XSupportsLocale();
    XSetLocaleModifiers("@im=none");

    state.xDisplayPtr = XOpenDisplay( NULL );
    assert( state.xDisplayPtr != NULL );

    XkbSetDetectableAutoRepeat(state.xDisplayPtr, true, 0); // disable auto-repeat of KeyRelease event

    // FBConfigs is needed but was added in GLX version 1.3.
    int glx_major, glx_minor;
    glXQueryVersion( state.xDisplayPtr, &glx_major, &glx_minor );
    if(!( ((glx_major << 8) | glx_minor) >= 0x0103) )
        AppLog::Info(__FILENAME__, "glXQueryVersion returned %d.%d 1.3 is required", glx_major, glx_minor );

    // Get a matching FB config
    static int visual_attribs[] =
        {
            GLX_X_RENDERABLE    , True,
            GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
            GLX_RENDER_TYPE     , GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR, // possibly unnecessary
            GLX_BUFFER_SIZE     , 16, // minimum
            GLX_DEPTH_SIZE      , 16, // minimum
            GLX_DOUBLEBUFFER    , True,
            None
        };

    // Pick the first FB config/visual with the most samples per pixel for MSAA
    int fbcount;
    GLXFBConfig* fbConfig = glXChooseFBConfig(state.xDisplayPtr, DefaultScreen(state.xDisplayPtr), visual_attribs, &fbcount);
    assert(fbConfig);
    int best_num_samp = -1;
    for(int i=0; i<fbcount; ++i)
    {
        XVisualInfo *vi = glXGetVisualFromFBConfig( state.xDisplayPtr, fbConfig[i] );
        if( vi )
        {
            int samp_buf, samples;
            glXGetFBConfigAttrib( state.xDisplayPtr, fbConfig[i], GLX_SAMPLE_BUFFERS, &samp_buf );
            glXGetFBConfigAttrib( state.xDisplayPtr, fbConfig[i], GLX_SAMPLES       , &samples  );
            if( samp_buf && samples > best_num_samp )
            {
                best_num_samp = samples;
                state.glxFBConfig = fbConfig[i];
            }
        }
        XFree( vi );
    }
    XFree( fbConfig );

    // Get a visual
    state.xVisualPtr = glXGetVisualFromFBConfig( state.xDisplayPtr, state.glxFBConfig );

    XSetWindowAttributes swa;
    swa.colormap = state.xColormap = XCreateColormap(
       state.xDisplayPtr,
       RootWindow( state.xDisplayPtr, state.xVisualPtr->screen ),
       state.xVisualPtr->visual, AllocNone
    );
    swa.background_pixmap = None ;
    swa.border_pixel      = 0;
    swa.event_mask        =
        FocusChangeMask |
        ExposureMask |
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask |
        PointerMotionMask |
        StructureNotifyMask;

    state.xWindow = XCreateWindow(
        state.xDisplayPtr,
        RootWindow( state.xDisplayPtr, state.xVisualPtr->screen ),
        0, 0, PlatformWidth, PlatformHeight, 0, state.xVisualPtr->depth, InputOutput,
        state.xVisualPtr->visual,
        CWBorderPixel|CWColormap|CWEventMask, &swa
    );

    XStoreName( state.xDisplayPtr, state.xWindow, szWindowname );
    XMapWindow( state.xDisplayPtr, state.xWindow );

    // register a new atom for window-manager close-events for delivery to the event-queue
    state.xWMDeleteWindowAtom = XInternAtom( state.xDisplayPtr, "WM_DELETE_WINDOW", True );
    XSetWMProtocols( state.xDisplayPtr, state.xWindow, &state.xWMDeleteWindowAtom, 1 );

    XSync( state.xDisplayPtr, false );
    XFlush( state.xDisplayPtr );

    // get fd/socket of X display
    state.windowFD = ConnectionNumber( state.xDisplayPtr );

    /*
     * determine display dpi using XLib...
     * HeightMMOfScreen(screen)
     * WidthMMOfScreen(screen)
     */

    // touch

    char szDevNode[32];
    int rc = 0;

    // find device node for specified named device
    // input.c from: input-utils_1.0.orig.tar.gz
    for( int i=0; i<32; i++ )
    {
        char buf[32];

        snprintf( szDevNode, sizeof(szDevNode), "/dev/input/event%d", i );
        state.touchFD = open( szDevNode, O_RDONLY );
        if(state.touchFD == -1)
            break; // not found

        rc = ioctl( state.touchFD, EVIOCGNAME(sizeof(buf)), buf );
        if(rc < 0) raise(SIGTRAP);

        if( strncmp( buf, szDevName, sizeof(buf) ) == 0 ) { break; }

        close( state.touchFD );
        state.touchFD = -1;
    }
    if( state.touchFD == -1 )
    {
        printf("Touch Device '%s' not found.\n", szDevName);
        printf("Enabling mouse/keyboard emulation mode.\n");
    }
    else
    {
        rc = fcntl( state.touchFD, F_SETFL, fcntl(state.touchFD, F_GETFL) | O_NONBLOCK); // non-blocking
        if(rc < 0) raise(SIGTRAP);
        rc = ioctl( state.touchFD, EVIOCGRAB, 1 ); // grab
        if(rc < 0) raise(SIGTRAP);

        input_absinfo mtdevAxis[2];
        rc = ioctl( state.touchFD, EVIOCGABS( ABS_X ), &mtdevAxis[0] ); // axis info
        if(rc < 0) raise(SIGTRAP);
        rc = ioctl( state.touchFD, EVIOCGABS( ABS_Y ), &mtdevAxis[1] ); // axis info
        if(rc < 0) raise(SIGTRAP);
        state.axisInfo[0] = { float(mtdevAxis[0].minimum), float(mtdevAxis[0].maximum), float(mtdevAxis[0].resolution) };
        state.axisInfo[1] = { float(mtdevAxis[1].minimum), float(mtdevAxis[1].maximum), float(mtdevAxis[1].resolution) };

        rc = mtdev_open( &state.mtdevState, state.touchFD ); // open for events
        if(rc < 0) raise(SIGTRAP);
    }

    // start module in untouched state
    state.touchEvent.kind = Event::Touch;
    state.touchEvent.u.touch.toKind = Event::Kind::Nil;
    state.touchEvent.u.touch.id = 0; // finger 0

    state.touchEventArr[0].kind = Event::Touch;
    state.touchEventArr[0].u.touch.toKind = Event::Kind::Nil;
    state.touchEventArr[0].u.touch.id = 0;
    state.touchEventArr[1].kind = Event::Touch;
    state.touchEventArr[1].u.touch.toKind = Event::Kind::Nil;
    state.touchEventArr[1].u.touch.id = 1;

    state.mouseEvent = state.touchEvent; // same defaults for emulation

    state.rebindFn();
}
void AppPlatform::Release()
{
    if(state.releaseFn)
    {
        state.releaseFn();

        state.rebindFn = 0;
        state.releaseFn = 0;
    }

    // window

    XSync( state.xDisplayPtr, true );
    XDestroyWindow( state.xDisplayPtr, state.xWindow );
    XFree( state.xVisualPtr );
    XFreeColormap( state.xDisplayPtr, state.xColormap );

    XCloseDisplay( state.xDisplayPtr );

    // touch

    if( state.touchFD != -1 )
    {
        mtdev_close( &state.mtdevState );
        ioctl( state.touchFD, EVIOCGRAB, 0 );
        close( state.touchFD );
    }

    if(state.szInternalPath) { free( state.szInternalPath ); state.szInternalPath = 0; }
    if(state.szExternalPath) { free( state.szExternalPath ); state.szExternalPath = 0; }
}

void AppPlatform::Tick(std::function<void(const Event &)> fnEvent)
{
    while(true)
    {
        FD_ZERO( &state.fdReadSet );
        FD_SET( state.windowFD, &state.fdReadSet ); // set x-event bit
        if( state.touchFD != -1 )
            FD_SET( state.touchFD, &state.fdReadSet ); // set touch-event bit
        int nfds = std::max( state.windowFD, state.touchFD ) + 1; // crazy posix!
        int fdReadyCount = pselect( nfds, &state.fdReadSet, NULL, NULL, &state.timer,
                                    NULL ); // nb: select(...) destroys timer value but not pselect(...)
        assert( fdReadyCount >= 0 );

        {
            struct timespec spec;
            clock_gettime( CLOCK_REALTIME, &spec );
            state.tick_newMSec = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
            if( state.tick_oldMSec == 0 ) state.tick_oldMSec = state.tick_newMSec;
            if( state.tick_newMSec < state.tick_oldMSec ) state.tick_newMSec = state.tick_oldMSec;
            state.tick_deltaMSec = ( state.tick_newMSec - state.tick_oldMSec );
            deltaMSec = state.tick_deltaMSec < 0 ? 0 : uint32_t( state.tick_deltaMSec );
        }

        Event event;

        if( fdReadyCount == 0
#ifdef ENABLE_QUICKBAIL
            || deltaMSec > TickRateMSec
#endif
            ) // leave on pselect timeout or when 50msec is up during processing
        {
            break;
        }

        if( state.touchFD != -1 && FD_ISSET( state.touchFD, &state.fdReadSet ))
        {
            // https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt
            const float scaleFudge = 10.f; // is the driver confused between cm and mm?
            const float coefX = PlatformWidth / ( scaleFudge * state.axisInfo[0].max / state.axisInfo[0].rez );
            const float coefY = PlatformHeight / ( scaleFudge * state.axisInfo[1].max / state.axisInfo[1].rez );
            int batchCount = mtdev_get( &state.mtdevState, state.touchFD, state.mtEventArr,
                                        State::batchSize ); // requires O_NONBLOCK
            for( int i = 0; i < batchCount; i++ ) // todo: check can this freeze-up the tick-rate
            {
                switch( state.mtEventArr[i].code )
                {
                    case ABS_MT_SLOT:
                        if( state.mtEventArr[i].value < Event::MaxTouch )
                            state.mtEventSlot = state.mtEventArr[i].value;
                        break;
                    case ABS_MT_TRACKING_ID:
                        if( state.mtEventArr[i].value == -1 )
                            // finish drawing when tracking is becomes invalid
                            state.touchEventArr[state.mtEventSlot].u.touch.toKind = Event::Kind::End;
                        else if( state.touchEventArr[state.mtEventSlot].u.touch.toKind == Event::Kind::Nil )
                            // start drawing when coming from a nil state
                            state.touchEventArr[state.mtEventSlot].u.touch.toKind = Event::Kind::Begin;
                        break;
                    case ABS_MT_POSITION_X:
                        state.touchEventArr[state.mtEventSlot].u.touch.x = int32_t(
                            std::lround( coefX * float( state.mtEventArr[i].value )));
                        break;
                    case ABS_MT_POSITION_Y:
                        state.touchEventArr[state.mtEventSlot].u.touch.y = int32_t(
                            std::lround( coefY * float( state.mtEventArr[i].value )));
                        break;
                    case EV_SYN:
                        // catch end-bounce
                        if( state.touchEventArr[state.mtEventSlot].u.touch.toKind == Event::Kind::Nil )
                            break;

                        fnEvent( state.touchEventArr[state.mtEventSlot] );

                        // convert 'begin's to 'continue's
                        if( state.touchEventArr[state.mtEventSlot].u.touch.toKind == Event::Kind::Begin )
                            state.touchEventArr[state.mtEventSlot].u.touch.toKind = Event::Kind::Move;

                        // ensure we catch end-bounce
                        if( state.touchEventArr[state.mtEventSlot].u.touch.toKind == Event::Kind::End )
                            state.touchEventArr[state.mtEventSlot].u.touch.toKind = Event::Kind::Nil;
                        break;
                    default:
                        break;
                }
            }
        }

        if( FD_ISSET( state.windowFD, &state.fdReadSet ))
        {
            KeySym keysym = 0;
            char buf[2];

            XEvent xEvent;
            while( XPending( state.xDisplayPtr )) // gobble messages in batches. check: can this freeze-up the tick-rate
            {
                XNextEvent( state.xDisplayPtr, &xEvent );
                if( XFilterEvent( &xEvent, state.xWindow )) continue; // additional processing
                switch( xEvent.type )
                {
                    case Expose:
                        event.kind = Event::Adornment;
                        event.u.adornment.adKind = Event::Kind::Refresh;
                        fnEvent( event );
                        break;
                    case ClientMessage:
                        if((ulong) xEvent.xclient.data.l[0] == state.xWMDeleteWindowAtom )
                        {
                            event.kind = Event::Adornment;
                            event.u.adornment.adKind = Event::Kind::Close;
                            fnEvent( event );
                        }
                        break;
                    case FocusIn:
                    case FocusOut:
                        event.kind = Event::Adornment;
                        event.u.adornment.adKind =
                            xEvent.xfocus.type == FocusIn ? Event::Kind::Resume : Event::Kind::Pause;
                        fnEvent( event );
                        break;
                    case KeyPress:
                        event.kind = Event::Key;
                        // todo: capture ctrl key via https://chromium.googlesource.com/angle/angle/+/master/util/x11/X11Window.cpp
                        if( 0 < XLookupString( &xEvent.xkey, buf, 1, &keysym, nullptr ))
                        {
                            event.u.key.key = (char) keysym; // may be non-ascii
                            event.u.key.press = true;
                            fnEvent( event );
                        }
                        break;
                    case KeyRelease:
                        event.kind = Event::Key;
                        // todo: capture ctrl key via https://chromium.googlesource.com/angle/angle/+/master/util/x11/X11Window.cpp
                        if( 0 < XLookupString( &xEvent.xkey, buf, 1, &keysym, nullptr ))
                        {
                            event.u.key.key = (char) keysym; // may be non-ascii
                            event.u.key.press = false;
                            fnEvent( event );
                        }
                        break;
                    case ButtonPress:
                        state.mousePressed = true;
                        state.mouseEvent.u.touch.toKind = Event::Kind::Begin;
                        state.mouseEvent.u.touch.x = xEvent.xmotion.x;
                        state.mouseEvent.u.touch.y = xEvent.xmotion.y;
                        fnEvent( state.mouseEvent );
                        break;
                    case ButtonRelease:
                        state.mousePressed = false;
                        state.mouseEvent.u.touch.toKind = Event::Kind::End;
                        state.mouseEvent.u.touch.x = xEvent.xmotion.x;
                        state.mouseEvent.u.touch.y = xEvent.xmotion.y;
                        fnEvent( state.mouseEvent );
                        break;
                    case MotionNotify:
                        if( state.mousePressed )
                        {
                            state.mouseEvent.u.touch.toKind = Event::Kind::Move;
                            state.mouseEvent.u.touch.x = xEvent.xmotion.x;
                            state.mouseEvent.u.touch.y = xEvent.xmotion.y;
                            fnEvent( state.mouseEvent );
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    state.tick_oldMSec = state.tick_newMSec;

    state.tick_deltaMSecAvg = state.tick_deltaMSecAvg * .8f + state.tick_deltaMSec * .2f;
    state.tick_deltaSecAvg = float( state.tick_deltaMSecAvg ) / 1000.f;
    deltaSecAvg = state.tick_deltaSecAvg;
}

extern void app_main();

int main(int argc, char *argv[])
{
    app_main();

    return 0;
}
