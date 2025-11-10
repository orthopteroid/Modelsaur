/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <initializer_list>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <jni.h>
#include <errno.h>
#include <cassert>
#include <algorithm>
#include <deque>
#include <functional>
#include <cmath>
#include <mutex>
#include <map>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define GL_GLES_PROTOTYPES 1
#define GLM_ENABLE_EXPERIMENTAL

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include "AppPlatform.hpp"

/*
 * AcquireASensorManagerInstance(void)
 *    Workaround ASensorManager_getInstance() deprecation false alarm
 *    for Android-N and before, when compiling with NDK-r15
 */
#include <dlfcn.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, __FILENAME__, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, __FILENAME__, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, __FILENAME__, __VA_ARGS__))

inline void LOGWgl(const char* szFn)
{
    auto error = eglGetError();
    if (error != EGL_SUCCESS)
        __android_log_print(ANDROID_LOG_WARN, __FILENAME__, "%s failed with %X", szFn, error);
}

// fwd decl
void app_main();

/**
 * Our saved state data.
 */
struct saved_state {
    ASensorVector   acceleration;
    float angle;
    int32_t x;
    int32_t y;
};

/**
 * Shared state for our app.
 */
struct AppPlatform::Opaque {
    static Opaque* pSingleton;
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    int paused = 1; // start pumpingPaused for platform init
    EGLDisplay display;
    EGLSurface surface;
    EGLConfig config;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct saved_state state;
    long frameTimerMSEC = 0;

    GLuint hProgram;
    GLuint hFragShader;
    GLuint hVertShader;
    std::deque<glm::mat4> deqMxModel;
    glm::mat4 mxProjection;
    GLenum eClientState;
    GLenum mxMode;
    std::map<GLenum, GLuint> bindmap;

    // time
    long tick_newMSec = 0;
    long tick_oldMSec = 0;
    long tick_deltaMSec = 0;
    long tick_deltaMSecAvg = 0;
    float tick_deltaSecAvg = 0;

    std::deque<AppPlatform::Event> eventDeque; // must be threadsafe
    std::mutex dequeMutex;
};

AppPlatform::Opaque* AppPlatform::Opaque::pSingleton = 0;

void safe_deque_push_back(AppPlatform::Opaque *pOpaque, AppPlatform::Event ev)
{
    pOpaque->dequeMutex.lock();
    pOpaque->eventDeque.push_back(ev);
    pOpaque->dequeMutex.unlock();
}

ASensorManager* AcquireASensorManagerInstance(android_app* app) {

    if(!app)
        return nullptr;

    typedef ASensorManager *(*PF_GETINSTANCEFORPACKAGE)(const char *name);
    void* androidHandle = dlopen("libandroid.so", RTLD_NOW);
    PF_GETINSTANCEFORPACKAGE getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE)dlsym(androidHandle, "ASensorManager_getInstanceForPackage");
    if (getInstanceForPackageFunc) {
        JNIEnv* env = nullptr;
        app->activity->vm->AttachCurrentThread(&env, NULL);

        jclass android_content_Context = env->GetObjectClass(app->activity->clazz);
        jmethodID midGetPackageName = env->GetMethodID(android_content_Context, "getPackageName", "()Ljava/lang/String;");
        jstring packageName= (jstring)env->CallObjectMethod(app->activity->clazz, midGetPackageName);

        const char *nativePackageName = env->GetStringUTFChars(packageName, 0);
        ASensorManager* mgr = getInstanceForPackageFunc(nativePackageName);
        env->ReleaseStringUTFChars(packageName, nativePackageName);
        app->activity->vm->DetachCurrentThread();
        if (mgr) {
            dlclose(androidHandle);
            return mgr;
        }
    }

    typedef ASensorManager *(*PF_GETINSTANCE)();
    PF_GETINSTANCE getInstanceFunc = (PF_GETINSTANCE)dlsym(androidHandle, "ASensorManager_getInstance");
    // by all means at this point, ASensorManager_getInstance should be available
    assert(getInstanceFunc);
    dlclose(androidHandle);

    return getInstanceFunc();
}

///////////////////////

// initialize OpenGL ES and EGL
static void platform_rebind(AppPlatform::Opaque *pOpaque)
{
    EGLint error = EGL_SUCCESS;

    if(pOpaque->sensorEventQueue == nullptr) {
        pOpaque->sensorEventQueue = ASensorManager_createEventQueue(
                pOpaque->sensorManager,
                pOpaque->app->looper, LOOPER_ID_USER,
                NULL, NULL);
    }

    if(pOpaque->display == EGL_NO_DISPLAY)
    {
        pOpaque->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        LOGWgl("eglGetDisplay");

        LOGI("Attaching to display %p", (void*)pOpaque->display);
    } else {
        LOGI("Reattaching to display %p", (void*)pOpaque->display);
    }

    EGLint maj, min;
    eglInitialize(pOpaque->display, &maj, &min);
    LOGWgl("eglInitialize");
    LOGI("eglInitialize %d %d", maj, min);

    if(pOpaque->surface == EGL_NO_SURFACE)
    {
        /* Request a window surface that is GL ES 2 compatible */
        const EGLint attribs[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // request OpenGL ES 2.0
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
//                EGL_RED_SIZE, 8,
//                EGL_GREEN_SIZE, 8,
//                EGL_BLUE_SIZE, 8,
                EGL_BUFFER_SIZE, 16,
                EGL_DEPTH_SIZE, 16,
                EGL_NONE
        };

        EGLint numConfigs;
        eglChooseConfig(pOpaque->display, attribs, &pOpaque->config, 1, &numConfigs);
        LOGWgl("eglChooseConfig");
        assert(numConfigs);

        EGLint r, g, b, a, d;
        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_RED_SIZE, &r);
        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_ALPHA_SIZE, &a);
        eglGetConfigAttrib(pOpaque->display, pOpaque->config, EGL_DEPTH_SIZE, &d);
        LOGI("RGBAD %d %d %d %d %d", r, g, b, a, d);

        LOGI("window %p", pOpaque->app->window);

        pOpaque->surface = eglCreateWindowSurface(pOpaque->display, pOpaque->config, pOpaque->app->window, NULL);
        LOGWgl("eglCreateWindowSurface");

        LOGI("Attaching to surface %p", (void*)pOpaque->surface);
    } else {
        LOGI("Reattaching to surface %p", (void*)pOpaque->surface);
    }

    if(pOpaque->context == EGL_NO_CONTEXT)
    {
        EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE }; // request OpenGL ES 2.0
        pOpaque->context = eglCreateContext(pOpaque->display, pOpaque->config, NULL, attribs);
        LOGWgl("eglCreateContext");

        LOGI("Attaching to context %p", (void*)pOpaque->context);
    } else {
        LOGI("Reattaching to context %p", (void*)pOpaque->context);
    }

    eglMakeCurrent(pOpaque->display, pOpaque->surface, pOpaque->surface, pOpaque->context);
    LOGWgl("eglMakeCurrent");

    EGLint w, h;
    eglQuerySurface(pOpaque->display, pOpaque->surface, EGL_WIDTH, &w);
    eglQuerySurface(pOpaque->display, pOpaque->surface, EGL_HEIGHT, &h);

    pOpaque->width = w;
    pOpaque->height = h;
    pOpaque->state.angle = 0;

    // Check openGL on the system
    auto opengl_info = {GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS};
    for (auto name : opengl_info) {
        auto info = glGetString(name);
        LOGI("OpenGL Info: %s", info);
    }

    // shader

    GLint status = 0;
    GLint logsize = 0;

    const char* vsrc = " \
        uniform mat4 u_MVP; \
        attribute mediump vec3 a_Position; \
        attribute mediump vec2 a_TexPosition; \
        attribute vec3 a_Color; \
        varying mediump vec2 v_TexPosition; \
        varying vec4 v_Color; \
        void main() \
        { \
           v_TexPosition = a_TexPosition; \
           v_Color = vec4( a_Color, 1.0 ); \
           gl_Position = u_MVP * vec4( a_Position, 1.0 ); \
        } \
    ";

    const char* fsrc = " \
        precision mediump float; \
        uniform int u_fTexture; \
        uniform sampler2D u_Texture; \
        varying vec4 v_Color; \
        varying mediump vec2 v_TexPosition; \
        void main() \
        { \
            if(u_fTexture==1) \
                gl_FragColor = texture2D( u_Texture, v_TexPosition ); \
            else \
                gl_FragColor = v_Color; \
        } \
    ";
//            gl_FragColor = texture2D( u_Texture, v_TexPosition );

    pOpaque->hVertShader = glCreateShader(GL_VERTEX_SHADER);
    assert(pOpaque->hVertShader);

    pOpaque->hFragShader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(pOpaque->hFragShader);

    glShaderSource(pOpaque->hVertShader, 1, &vsrc, NULL);
    glCompileShader(pOpaque->hVertShader);
    glGetShaderiv(pOpaque->hVertShader, GL_COMPILE_STATUS, &status);
    LOGI("Vertex shader compilation status %X", status);

    glGetShaderiv(pOpaque->hVertShader, GL_INFO_LOG_LENGTH, &logsize);
    if(logsize)
    {
        std::unique_ptr<char[]> logbuffer(new char[logsize]);
        glGetShaderInfoLog(pOpaque->hVertShader, logsize - 1, NULL, logbuffer.get());
        LOGW("Vert shader log:\n%s", logbuffer.get());
    }

    glShaderSource(pOpaque->hFragShader, 1, &fsrc, NULL);
    glCompileShader(pOpaque->hFragShader);
    glGetShaderiv(pOpaque->hFragShader, GL_COMPILE_STATUS, &status);
    LOGI("Fragment shader compilation status %X", status);

    glGetShaderiv(pOpaque->hFragShader, GL_INFO_LOG_LENGTH, &logsize);
    if(logsize)
    {
        std::unique_ptr<char[]> logbuffer(new char[logsize]);
        glGetShaderInfoLog(pOpaque->hFragShader, logsize - 1, NULL, logbuffer.get());
        LOGW("Frag shader log:\n%s", logbuffer.get());
    }

    pOpaque->hProgram = glCreateProgram();
    LOGI("Shader program %X", pOpaque->hProgram);

    glAttachShader(pOpaque->hProgram, pOpaque->hVertShader);
    glAttachShader(pOpaque->hProgram, pOpaque->hFragShader);
    glLinkProgram(pOpaque->hProgram);
    glGetProgramiv(pOpaque->hProgram, GL_LINK_STATUS, &status);
    LOGI("Shader program link status %X", status);

    glBindAttribLocation(pOpaque->hProgram, 0, "a_Position");
    pOpaque->bindmap[GL_VERTEX_ARRAY] = GLuint(glGetAttribLocation(pOpaque->hProgram, "a_Position"));

    glBindAttribLocation(pOpaque->hProgram, 0, "a_Color");
    pOpaque->bindmap[GL_COLOR_ARRAY] = GLuint(glGetAttribLocation(pOpaque->hProgram, "a_Color"));

//    glBindAttribLocation(pOpaque->hProgram, 0, "a_Normal");
//    LOGWgl("glBindAttribLocation a_Normal");
//    pOpaque->bindmap[GL_NORMAL_ARRAY] = GLuint(glGetAttribLocation(pOpaque->hProgram, "a_Normal"));
//    LOGWgl("glGetAttribLocation a_Normal");

    glBindAttribLocation(pOpaque->hProgram, 0, "a_TexPosition");
    pOpaque->bindmap[GL_TEXTURE_COORD_ARRAY] = GLuint(glGetAttribLocation(pOpaque->hProgram, "a_TexPosition"));

    pOpaque->bindmap[GL_MODELVIEW] = GLuint(glGetUniformLocation(pOpaque->hProgram, "u_MVP"));

    glUseProgram(pOpaque->hProgram);

    // default
    glUniform1i(
            GLuint(glGetAttribLocation(AppPlatform::Opaque::pSingleton->hProgram, "u_fTexture")),
            0
    );

    // Initialize GL state.
#if false
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
//    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);
#else
    glEnable( GL_DEPTH_TEST );
    //glDepthRange( 0.f, 1.f );
//    glShadeModel(GL_FLAT); // todo: put in shader? NO!
#endif
    glViewport(0,0,pOpaque->width,pOpaque->height);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void platform_release(AppPlatform::Opaque *pOpaque) {
    if (pOpaque->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(pOpaque->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (pOpaque->context != EGL_NO_CONTEXT) {
            LOGI("Releasing context %p", (void *) pOpaque->context);
            eglDestroyContext(pOpaque->display, pOpaque->context);
        }
        if (pOpaque->surface != EGL_NO_SURFACE) {
            LOGI("Releasing surface %p", (void *) pOpaque->surface);
            eglDestroySurface(pOpaque->display, pOpaque->surface);
        }
        LOGI("Releasing display %p", (void *) pOpaque->display);
        eglTerminate(pOpaque->display);
    }
    pOpaque->display = EGL_NO_DISPLAY;
    pOpaque->context = EGL_NO_CONTEXT;
    pOpaque->surface = EGL_NO_SURFACE;

    if (pOpaque->sensorEventQueue != nullptr) {
        ASensorManager_destroyEventQueue(pOpaque->sensorManager,
                                         pOpaque->sensorEventQueue); // todo: submit diff
        pOpaque->sensorEventQueue = nullptr;
    }

    // shader

    if (pOpaque->hVertShader) {
        LOGI("Releasing shader %X", pOpaque->hVertShader);
        glDeleteShader(pOpaque->hVertShader);
        pOpaque->hVertShader = 0;
    }
    if (pOpaque->hFragShader) {
        LOGI("Releasing shader %X", pOpaque->hFragShader);
        glDeleteShader(pOpaque->hFragShader);
        pOpaque->hFragShader = 0;
    }
    if (pOpaque->hProgram) {
        LOGI("Releasing program %X", pOpaque->hProgram);
        glDeleteProgram(pOpaque->hProgram);
        pOpaque->hProgram = 0;
    }

}

/**
 * Process the next input event.
 */
static int32_t platform_input(struct android_app *app, AInputEvent *event) {
    AppPlatform::Opaque* pOpaque = (AppPlatform::Opaque*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        pOpaque->paused = 0; // resume event polling

        auto action = AKeyEvent_getAction(event);
        auto x = int32_t(AMotionEvent_getX(event, 0));
        auto y = int32_t(AMotionEvent_getY(event, 0));
        int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int count = AMotionEvent_getPointerCount(event);
        //int32_t AMotionEvent_getPointerId(const AInputEvent* motion_event, size_t pointer_index);

        switch(action & AMOTION_EVENT_ACTION_MASK)
        {
            //case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_DOWN:
                safe_deque_push_back( pOpaque, { AppPlatform::Event::Kind::Touch, .u.touch = { AppPlatform::Event::Kind::Begin, 0, x, y } } );
                break;
            //case AMOTION_EVENT_ACTION_POINTER_UP:
            case AMOTION_EVENT_ACTION_UP:
                safe_deque_push_back( pOpaque, { AppPlatform::Event::Kind::Touch, .u.touch = { AppPlatform::Event::Kind::End, 0, x, y } } );
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                safe_deque_push_back( pOpaque, { AppPlatform::Event::Kind::Touch, .u.touch = { AppPlatform::Event::Kind::Move, 0, x, y } } );
                break;
            default: ;
        }
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
static void platform_cmd(struct android_app *app, int32_t cmd) {
    AppPlatform::Opaque* pOpaque = (AppPlatform::Opaque*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            LOGI("APP_CMD_SAVE_STATE");
            // The system has asked us to save our current state.  Do so.
            pOpaque->app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)pOpaque->app->savedState) = pOpaque->state;
            pOpaque->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            platform_rebind(pOpaque);

            pOpaque->paused = 0; // ensure unpaused. ie to AppPlatform::Bind(...)
            safe_deque_push_back(pOpaque, { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Resume });
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            platform_release(pOpaque);

            safe_deque_push_back(pOpaque, { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Pause });
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("APP_CMD_GAINED_FOCUS");

            // When our app gains focus, we start monitoring the accelerometer.
            if (pOpaque->accelerometerSensor != NULL) {
                LOGI("Bind to sensorqueue %p", (void*)pOpaque->sensorEventQueue);
                ASensorEventQueue_enableSensor(pOpaque->sensorEventQueue, pOpaque->accelerometerSensor);
                ASensorEventQueue_setEventRate(pOpaque->sensorEventQueue, pOpaque->accelerometerSensor, (1000L/60)*1000); // 60 sps, in us
            }

            pOpaque->paused = 0; // resume event polling
            safe_deque_push_back(pOpaque, { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Resume });
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("APP_CMD_LOST_FOCUS");

            // When our app loses focus, we stop monitoring the accelerometer.
            if (pOpaque->accelerometerSensor != NULL) {
                LOGI("Release sensorqueue %p", (void*)pOpaque->sensorEventQueue);
                ASensorEventQueue_disableSensor(pOpaque->sensorEventQueue, pOpaque->accelerometerSensor);
            }

            pOpaque->paused = 1;
            safe_deque_push_back(pOpaque, { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Pause });
            break;
    }
}

static void platform_pump(AppPlatform::Opaque *pOpaque) {
    // Read all pending events.
    int ident;
    int events;
    struct android_poll_source* source;

    // If pumpingPaused, we will block forever waiting for events.
    // If not pumpingPaused, we loop until all events are read, then continue
    // to draw the next frame of animation.
    while ((ident=ALooper_pollAll(pOpaque->paused ? -1 : 0, NULL, &events, (void**)&source)) >= 0)
    {
        // Process this event.
        if (source != NULL) {
            source->process(pOpaque->app, source);
        }

        // If a sensor has data, process it now.
        if (ident == LOOPER_ID_USER) {
            if (pOpaque->accelerometerSensor != NULL) {
                ASensorEvent event;
                while (ASensorEventQueue_getEvents(pOpaque->sensorEventQueue, &event, 1) > 0)
                {
// chatty                    LOGI("accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y, event.acceleration.z);
                    pOpaque->state.acceleration = event.acceleration;
                }
            }
        }

        // Check if we are exiting.
        if (pOpaque->app->destroyRequested != 0)
        {
            safe_deque_push_back(pOpaque, { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Close } );
            return;
        }
    }
}

void android_main(struct android_app* app) {
    AppPlatform::Opaque::pSingleton = new AppPlatform::Opaque();
    AppPlatform::Opaque::pSingleton->app = app;

    app->userData = AppPlatform::Opaque::pSingleton;
    app->onAppCmd = platform_cmd;
    app->onInputEvent = platform_input;

    if (app->savedState != NULL) {
        AppPlatform::Opaque::pSingleton->state = *(struct saved_state*)app->savedState;
    }

    LOGI("app_main start");

    app_main();

    LOGI("app_main complete");

    if(AppPlatform::Opaque::pSingleton) delete AppPlatform::Opaque::pSingleton;
    app->userData = AppPlatform::Opaque::pSingleton = nullptr;
}

//////////////////

uint AppPlatform::GetWidth() const { return uint(Opaque::pSingleton->width); }
uint AppPlatform::GetHeight() const { return uint(Opaque::pSingleton->height); }

void AppPlatform::Bind(const char* szWindowname, const char* szDevName)
{
    pOpaque = Opaque::pSingleton;

    pOpaque->sensorManager = AcquireASensorManagerInstance(pOpaque->app);
    pOpaque->accelerometerSensor = ASensorManager_getDefaultSensor( pOpaque->sensorManager, ASENSOR_TYPE_ACCELEROMETER);

    // wait for APP_CMD_INIT_WINDOW which will call platform_rebind
    while(pOpaque->paused)
    {
        LOGI("sync");
        platform_pump(pOpaque);
    }
}

void AppPlatform::Release()
{
    platform_release(pOpaque);

    pOpaque = nullptr; // not owner
}

void AppPlatform::Tick(std::function<void(const Event &)> fnEvent)
{
//    LOGI("tick");
    platform_pump(pOpaque);

    pOpaque->dequeMutex.lock();
    for(auto ev : pOpaque->eventDeque)
        fnEvent(ev);
    pOpaque->eventDeque.clear();
    pOpaque->dequeMutex.unlock();

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
}

void AppPlatform::BeginFrame()
{
    if(!pOpaque || !pOpaque->display) return;

    glClearColor(
            std::fabs(pOpaque->state.acceleration.x/10),
            std::fabs(pOpaque->state.acceleration.y/10),
            std::fabs(pOpaque->state.acceleration.z/10),
            1
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    pOpaque->frameTimerMSEC = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}

void AppPlatform::EndFrame()
{
    if(!pOpaque || !pOpaque->display) return;

    glFlush();
//    LOGI("glFlush %X", eglGetError());
//    glEndQuery( GL_TIME_ELAPSED );

    eglSwapBuffers(pOpaque->display, pOpaque->surface);
//    LOGI("eglSwapBuffers %X", eglGetError());

    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    pOpaque->frameTimerMSEC = spec.tv_sec * 1000 + spec.tv_nsec / 1000000 - pOpaque->frameTimerMSEC;

/*
    int done = 0;
    while( !done ) glGetQueryObjectiv( pOpaque->frameTimer, GL_QUERY_RESULT_AVAILABLE, &done );
    glGetQueryObjectuiv( pOpaque->frameTimer, GL_QUERY_RESULT, &pOpaque->frameTimerValue );
*/

    frameDuration = frameDuration * .75f + (float)pOpaque->frameTimerMSEC * 1E-3f * .25f;
}
/*
void AppPlatform::LoadProjection(const glm::mat4& m)
{
    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( glm::value_ptr(m) );
}

void AppPlatform::LoadModel(const glm::mat4& m)
{
    if(pOpaque->modelStack.size() == 0)
        pOpaque->modelStack.push_front(m);
    else
        pOpaque->modelStack[0] = m;

    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( glm::value_ptr(m) );
}

void AppPlatform::DupAndMultModel(const glm::mat4& m)
{
    glm::mat4 m0 = pOpaque->modelStack.front();
    glm::mat4 m1 = m0 * m;
    pOpaque->modelStack.push_front(m1);

    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( glm::value_ptr(m1) );
}

void AppPlatform::PopModel()
{
    pOpaque->modelStack.pop_front();
}
*/

void glMatrixMode(GLenum mode)
{
    AppPlatform::Opaque::pSingleton->mxMode = mode;
}
void glPushMatrix()
{
    AppPlatform::Opaque* pOpaque = AppPlatform::Opaque::pSingleton;

    assert(pOpaque->mxMode == GL_MODELVIEW);

    glm::mat4 m = pOpaque->deqMxModel.front();
    pOpaque->deqMxModel.push_front(m);
}
void glPopMatrix()
{
    AppPlatform::Opaque* pOpaque = AppPlatform::Opaque::pSingleton;

    assert(pOpaque->mxMode == GL_MODELVIEW);
    assert(pOpaque->deqMxModel.size() > 1);

    pOpaque->deqMxModel.pop_front();

    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_MODELVIEW];
    glm::mat4 mvp = pOpaque->mxProjection * pOpaque->deqMxModel[0];
    glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(mvp));
}
void glLoadMatrixf(const GLfloat *f)
{
    AppPlatform::Opaque* pOpaque = AppPlatform::Opaque::pSingleton;

    glm::mat4 m = glm::make_mat4x4(f);
    if(pOpaque->mxMode == GL_MODELVIEW)
    {
        if(pOpaque->deqMxModel.size() == 0)
            pOpaque->deqMxModel.push_front(m);
        else
            pOpaque->deqMxModel[0] = m;
    } else {
        pOpaque->mxProjection = m;
    }

    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_MODELVIEW];
    glm::mat4 mvp = pOpaque->mxProjection * pOpaque->deqMxModel[0];
    glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(mvp));
}
void glLoadIdentity()
{
    glm::mat4 m(1.f);
    glLoadMatrixf( glm::value_ptr(m) );
}
void glMultMatrixf(const GLfloat *f)
{
    AppPlatform::Opaque* pOpaque = AppPlatform::Opaque::pSingleton;

    assert(pOpaque->mxMode == GL_MODELVIEW);
    assert(pOpaque->deqMxModel.size() > 0);

    glm::mat4 m = pOpaque->deqMxModel[0] * glm::make_mat4x4(f);
    pOpaque->deqMxModel[0] = m;

    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_MODELVIEW];
    glm::mat4 mvp = pOpaque->mxProjection * pOpaque->deqMxModel[0];
    glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(mvp));
}
void glEnableClientState(GLenum cap)
{
    AppPlatform::Opaque::pSingleton->eClientState = cap;

    if(cap == GL_TEXTURE_COORD_ARRAY) {
        // https://stackoverflow.com/questions/23001842/opengl-es-2-0-gl-texture1
        // https://ycpcs.github.io/cs370-fall2017/labs/lab21.html
        const GLuint textureUnit = 0; // shader only has one
        glActiveTexture(textureUnit +GL_TEXTURE0);

        GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_TEXTURE_COORD_ARRAY];
        glUniform1i(u, textureUnit);

        glUniform1i(
            GLuint(glGetAttribLocation(AppPlatform::Opaque::pSingleton->hProgram, "u_fTexture")),
            1
        );
    }
}
void glDisableClientState(GLenum cap)
{
    if(cap == GL_TEXTURE_COORD_ARRAY) {
        glUniform1i(
                GLuint(glGetAttribLocation(AppPlatform::Opaque::pSingleton->hProgram, "u_fTexture")),
                0
        );
    }
}
void glPushClientAttrib(GLbitfield mask) { /* do nothing*/ }
void glPopClientAttrib() { /* do nothing */ }
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    AppPlatform::Opaque* pOpaque = AppPlatform::Opaque::pSingleton;

    GLuint u = pOpaque->bindmap[pOpaque->eClientState];
    glEnableVertexAttribArray(u);
    glVertexAttribPointer(u, size, type, GL_FALSE, stride, pointer);
}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_COLOR_ARRAY];
    glEnableVertexAttribArray(u);
    glVertexAttribPointer(u, 3 /* RGB */, type, GL_FALSE, stride, pointer);
}
//void glNormalPointer(GLenum type, GLsizei stride, const GLvoid * pointer)
//{
//    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_NORMAL_ARRAY];
//    glEnableVertexAttribArray(u);
//    glVertexAttribPointer(u, 1 /* 1 value per vertex */, type, GL_FALSE, stride, pointer);
//}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    GLuint u = AppPlatform::Opaque::pSingleton->bindmap[GL_TEXTURE_COORD_ARRAY];
    glEnableVertexAttribArray(u);
    glVertexAttribPointer(u, 2 /* 2d texture */, type, GL_FALSE, stride, pointer);
}

//END_INCLUDE(all)
