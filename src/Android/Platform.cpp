// Copyright 2025 orthopteroid@gmail.com, MIT License

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

#define GL_GLES_PROTOTYPES 1
#define GLM_ENABLE_EXPERIMENTAL

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>

#include <android/sensor.h>
#include <android/log.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include "android_native_app_glue.h"
#include "AppPlatform.hpp"
#include "AppLog.hpp"

/*
 * AcquireASensorManagerInstance(void)
 *    Workaround ASensorManager_getInstance() deprecation false alarm
 *    for Android-N and before, when compiling with NDK-r15
 */
#include <dlfcn.h>
#include <sys/stat.h>
#include <string>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, __FILENAME__, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, __FILENAME__, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, __FILENAME__, __VA_ARGS__))

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define APPNAME "com.orthopteroid.modelsaur"

#define ENABLE_SAVE_EXT_ROOT

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

struct State {
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;
    const char* externalHandyPath;

    int pumpingPaused = 1; // platform message delivery pump
    struct saved_state state;

    // time
    long tick_newMSec = 0;
    long tick_oldMSec = 0;
    long tick_deltaMSec = 0;
    long tick_deltaMSecAvg = 0;
    float tick_deltaSecAvg = 0;

    using EventDequeType = std::deque<AppPlatform::Event>;
    EventDequeType *pEventDeque; // use mutex to access
    std::mutex eventMutex;

    void safe_deque_push_back(AppPlatform::Event ev)
    {
        assert(pEventDeque);
        eventMutex.lock();
        pEventDeque->push_back(ev);
        eventMutex.unlock();
    }

    void safe_deque_deliver(std::function<void(const AppPlatform::Event &)> fnEvent)
    {
        // deliver messages from main-application-thread
        assert(pEventDeque);
        eventMutex.lock();
        for (auto ev : *pEventDeque) fnEvent(ev);
        pEventDeque->clear();
        eventMutex.unlock();
    }

    std::function<void(void)> rebindFn = 0;
    std::function<void(void)> releaseFn = 0;
};
static State state;

ANativeWindow* Platform_GetWindow() { return state.app ? state.app->window : 0; }
const char* Platform_InternalPath() { return state.app ? state.app->activity->internalDataPath : 0; }
const char* Platform_ExternalPath() {
    return
        state.app ?
        ( state.externalHandyPath ? state.externalHandyPath : state.app->activity->externalDataPath )
        : 0;
}

static ASensorManager* AcquireASensorManagerInstance(android_app* app)
{
    if(!app)
        return nullptr;

    typedef ASensorManager *(*PF_GETINSTANCEFORPACKAGE)(const char *name);
    void* androidHandle = dlopen("libandroid.so", RTLD_NOW);
    PF_GETINSTANCEFORPACKAGE getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE)dlsym(androidHandle, "ASensorManager_getInstanceForPackage");
    if (getInstanceForPackageFunc)
    {
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

static char* AcquireexternalHandyPath(android_app* app)
{
    char* szPath = nullptr;

    if(!app) return szPath;

    // File path = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES);
    // http://www.ouchangjian.com/question/5b2be34f74a3c8e0733225fd
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, NULL);

    jclass envClass = env->FindClass("android/os/Environment");

#if defined(ENABLE_SAVE_EXT_ROOT)
    jmethodID getExtStorageDirectoryMethod = env->GetStaticMethodID(
        envClass, "getExternalStorageDirectory",  "()Ljava/io/File;"
    );
    jobject extStorageFile = env->CallStaticObjectMethod(envClass, getExtStorageDirectoryMethod);
#else
    jstring stringArg = env->NewStringUTF("Pictures");
    jobject extStorageFile = env->CallStaticObjectMethod(envClass, getExtStorageDirectoryMethod, stringArg);
    env->DeleteLocalRef(stringArg);
#endif // ENABLE_SAVE_EXT_ROOT

    jclass fileClass = env->FindClass("java/io/File");
    jmethodID getPathMethod = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
    jstring extStoragePath = (jstring)env->CallObjectMethod(extStorageFile, getPathMethod);

    // actually, almost the same as UTF8 but not exactly ( https://stackoverflow.com/a/32215302 )
    const char* extStoragePathString = env->GetStringUTFChars(extStoragePath, 0);
    std::string strPath( extStoragePathString );
    strPath += "/";
    strPath += APPNAME;
    szPath = new char[ strPath.size() +1 ];
    strcpy( szPath, strPath.c_str() );
    env->ReleaseStringUTFChars(extStoragePath, extStoragePathString);

    app->activity->vm->DetachCurrentThread();

    return szPath;
}

///////////////////////

static void platform_rebind()
{
    if(state.sensorEventQueue == nullptr) {
        state.sensorEventQueue = ASensorManager_createEventQueue(
                state.sensorManager,
                state.app->looper, LOOPER_ID_USER,
                NULL, NULL);
    }

    if(!state.rebindFn)
        AppLog::Info(__FILENAME__, "NULL state.rebindFn");
    else
        state.rebindFn();
}

static void platform_release()
{
    if(!state.releaseFn)
        AppLog::Info(__FILENAME__, "NULL state.releaseFn");
    else
        state.releaseFn();

    if (state.sensorEventQueue != nullptr) {
        ASensorManager_destroyEventQueue(
            state.sensorManager, state.sensorEventQueue
        ); // todo: submit diff
        state.sensorEventQueue = nullptr;
    }
}

static int32_t platform_input(struct android_app *app, AInputEvent *event) {
    assert( state.pumpingPaused == 0 ); // todo: check called from platform_pump via inputProcess->process

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        auto type = AppPlatform::Event::Kind::Nil;

        auto action = AKeyEvent_getAction(event);
        switch(action & AMOTION_EVENT_ACTION_MASK)
        {
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_DOWN:
                type = AppPlatform::Event::Kind::Begin;
                break;
            case AMOTION_EVENT_ACTION_POINTER_UP:
            case AMOTION_EVENT_ACTION_UP:
                type = AppPlatform::Event::Kind::End;
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                type = AppPlatform::Event::Kind::Move;
                break;
            default: ;
        }

        if( type != AppPlatform::Event::Kind::Nil )
        {
            for( int8_t i=0; i<AMotionEvent_getPointerCount(event); i++ )
            {
                int32_t id = AMotionEvent_getPointerId(event, i);
                auto x = int16_t(AMotionEvent_getX(event, i));
                auto y = int16_t(AMotionEvent_getY(event, i));
                state.safe_deque_push_back( { AppPlatform::Event::Kind::Touch, .u.touch = { type, i, x, y } } );
//                LOGI("%d %d %d %d ", type, i, x, y);
            };
        }

//        int32_t index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        return 1;
    }
    return 0;
}

static void platform_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            LOGI("APP_CMD_SAVE_STATE");
            // The system has asked us to save our current state.  Do so.
            state.app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)state.app->savedState) = state.state;
            state.app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            platform_rebind();

            state.pumpingPaused = 0;
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            platform_release();

            state.pumpingPaused = 1;
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("APP_CMD_GAINED_FOCUS");

            // When our app gains focus, we start monitoring the accelerometer.
            if (state.accelerometerSensor != NULL) {
                LOGI("Bind to sensorqueue %p", (void*)state.sensorEventQueue);
                ASensorEventQueue_enableSensor(state.sensorEventQueue, state.accelerometerSensor);
                ASensorEventQueue_setEventRate(state.sensorEventQueue, state.accelerometerSensor, (1000L/60)*1000); // 60 sps, in us
            }

            // thaw app
            state.safe_deque_push_back(
                {AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Resume}
            );
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("APP_CMD_LOST_FOCUS");

            // When our app loses focus, we stop monitoring the accelerometer.
            if (state.accelerometerSensor != NULL) {
                LOGI("Release sensorqueue %p", (void*)state.sensorEventQueue);
                ASensorEventQueue_disableSensor(state.sensorEventQueue, state.accelerometerSensor);
            }

            // freeze app
            state.safe_deque_push_back(
                {AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Pause});
            break;

        case APP_CMD_DESTROY:
            LOGI("APP_CMD_DESTROY");

            state.safe_deque_push_back(
                { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Close }
            );
            break;
    }
}

static void platform_pump()
{
    // Read all pending events.
    int ident;
    int events;
    struct android_poll_source* source;

    // If pumpingPaused, we will block forever waiting for events.
    // If not pumpingPaused, we loop until all events are read, then continue
    // to draw the next frame of animation.
    while ((ident=ALooper_pollAll(state.pumpingPaused ? -1 : 0, NULL, &events, (void**)&source)) >= 0)
    {
        // Process this event.
        if (source != NULL) {
            source->process(state.app, source);
        }

        // If a sensor has data, process it now.
        if (ident == LOOPER_ID_USER) {
            if (state.accelerometerSensor != nullptr && state.sensorEventQueue != nullptr) // todo: diff
            {
                ASensorEvent event;
                while (ASensorEventQueue_getEvents(state.sensorEventQueue, &event, 1) > 0)
                {
// chatty                    LOGI("accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y, event.acceleration.z);
                    state.state.acceleration = event.acceleration;
                }
            }
        }

        // Check if we are exiting.
        if (state.app->destroyRequested != 0)
        {
            state.safe_deque_push_back( { AppPlatform::Event::Kind::Adornment, AppPlatform::Event::Kind::Close } );
            return;
        }
    }
}

void android_main(struct android_app* app) {
    state.app = app;
    state.pEventDeque = new State::EventDequeType();

    app->onAppCmd = platform_cmd;
    app->onInputEvent = platform_input;

    if (app->savedState != NULL) {
        state.state = *(struct saved_state*)app->savedState;
    }

    LOGI("app_main start");

    app_main();

    LOGI("app_main complete");

    // app->userData = /* unused */

    delete state.pEventDeque;
    state.pEventDeque = 0;
}

//////////////////

void AppPlatform::Bind(std::function<void(void)> rebindFn_, std::function<void(void)> releaseFn_, const char* szWindowname, const char* szDevName)
{
    state.rebindFn = rebindFn_;
    state.releaseFn = releaseFn_;

    state.sensorManager = AcquireASensorManagerInstance( state.app );
    state.accelerometerSensor = ASensorManager_getDefaultSensor( state.sensorManager, ASENSOR_TYPE_ACCELEROMETER );
    state.externalHandyPath = AcquireexternalHandyPath( state.app );

    // https://stackoverflow.com/questions/11294487/android-writing-saving-files-from-native-code-only#11537580
    if( Platform_InternalPath() ) {
        struct stat sb;
        int32_t res = stat( Platform_InternalPath(), &sb );
        char* szStat = (char*)"failed";
        if (0 == res && sb.st_mode & S_IFDIR) {
            szStat = (char*)"exists";
        } else if (ENOENT == errno) {
            res = mkdir( Platform_InternalPath(), 0770 );
            if(res == 0) szStat = (char*)"created";
        }
        AppLog::Info(__FILENAME__, "%s %s %s", __func__, Platform_InternalPath(), szStat );
    }

    if( Platform_ExternalPath() ) {
        struct stat sb;
        int32_t res = stat( Platform_ExternalPath(), &sb );
        char* szStat = (char*)"failed";
        if (0 == res && sb.st_mode & S_IFDIR) {
            szStat = (char*)"exists";
        } else if (ENOENT == errno) {
            res = mkdir( Platform_ExternalPath(), 0644 );
            if(res == 0) szStat = (char*)"created";
        }
        AppLog::Info(__FILENAME__, "%s %s %s", __func__, Platform_ExternalPath(), szStat );
    }

    // wait for APP_CMD_INIT_WINDOW which will call platform_rebind
    while(state.pumpingPaused) // todo: check flagging streamlined correctly?
    {
        LOGI("sync");
        platform_pump();
    }
/*
 * perhaps calc and return display dpi?
Display display = getWindowManager().getDefaultDisplay();
Point size = new Point();
display.getSize(size);
int width = size.x;
int height = size.y;

    EGLint w, h;
    eglQuerySurface(state.display, state.surface, EGL_WIDTH, &w);
    eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &h);

    width = w;
    height = h;
*/
}

void AppPlatform::Release()
{
   // platform_release();
}

void AppPlatform::Tick(std::function<void(const Event &)> fnEvent)
{
//    LOGI("tick");
    platform_pump();

    state.safe_deque_deliver(fnEvent); // deliver messages originating from main-application-thread

    {
        struct timespec spec;
        clock_gettime( CLOCK_REALTIME, &spec );
        state.tick_newMSec = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
        if( state.tick_oldMSec == 0 ) state.tick_oldMSec = state.tick_newMSec;
        if( state.tick_newMSec < state.tick_oldMSec ) state.tick_newMSec = state.tick_oldMSec;
        state.tick_deltaMSec = ( state.tick_newMSec - state.tick_oldMSec );
        state.tick_oldMSec = state.tick_newMSec;
        deltaMSec = state.tick_deltaMSec < 0 ? 0 : uint32_t(state.tick_deltaMSec);
        state.tick_deltaMSecAvg = state.tick_deltaMSecAvg * 8 / 10 + state.tick_deltaMSec * 2 / 10;
        state.tick_deltaSecAvg = float(state.tick_deltaMSecAvg) / 1000.f;
        deltaSecAvg = state.tick_deltaSecAvg;
    }
}
