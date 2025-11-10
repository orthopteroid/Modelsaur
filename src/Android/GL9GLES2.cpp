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
#include <list>

#include <png.h>
#include <pngstruct.h>
#include <pngconf.h>

#define GL_GLES_PROTOTYPES 1

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <fcntl.h>
#include <unistd.h>

#include "AppPlatform.hpp"
#include "AppLog.hpp"
#include "GL9.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

inline void LOGWgl(const char* szFn)
{
    auto error = eglGetError();
    if (error != EGL_SUCCESS)
        __android_log_print(ANDROID_LOG_WARN, __FILENAME__, "%s failed with %X", szFn, error);
}

// fwd decl
ANativeWindow* Platform_GetWindow();
const char* Platform_InternalPath();
const char* Platform_ExternalPath();


////////////////////////

struct OSB565
{
    GLuint rbo, dbo, fbo;
    int w, h;
    void* pixels;

    OSB565(int w_, int h_, void* pixels_)
    {
        w = w_; h = h_;
        pixels = pixels_;

        glGenRenderbuffers(1,&rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, w, h);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        glGenRenderbuffers(1,&dbo);
        glBindRenderbuffer(GL_RENDERBUFFER, dbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        glGenFramebuffers(1,&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }

    virtual ~OSB565()
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteFramebuffers(1,&fbo);
        glDeleteRenderbuffers(1,&rbo);
        glDeleteRenderbuffers(1,&dbo);
    }

    void BeginFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable( GL_DEPTH_TEST );

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }

    void EndFrame()
    {
        glFinish();

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        ::glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels );

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }
};

////////////////////////

struct State
{
    EGLDisplay display = 0;
    EGLSurface surface = 0;
    EGLConfig config = 0;
    EGLContext context = 0;
    GLint imp_fmt = 0, imp_type = 0;

    GLuint hGeomFS = 0;
    GLuint hGeomVS = 0;
    GLuint hGeomPrg = 0;

    GLuint hMenuFS = 0;
    GLuint hMenuVS = 0;
    GLuint hMenuPrg = 0;

    GLuint hPointerFS;
    GLuint hPointerVS;
    GLuint hPointerPrg;

    // maps <prog, enum> to int
    using BindmapType = std::map< std::pair<GLuint, GLenum>, GLuint>;
    BindmapType *pBindmap = 0; // can't be static

    using DeqType = std::deque<glm::mat4>;
    DeqType *pModelmxDeq = 0;
    glm::mat4 mxProjection;
    GLenum eClientState = 0;
    GLenum eMXMode = 0;
    GLuint hProgram = 0;

    long frameTimerMSEC = 0;
    float frameDuration = 0;
};
static State state;

///////////////////////

void gl9Bind( int32_t &width, int32_t &height )
{
    state.pBindmap = new State::BindmapType();
    state.pModelmxDeq = new State::DeqType();

    if(state.display == EGL_NO_DISPLAY)
    {
        state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        LOGWgl("eglGetDisplay");

        AppLog::Info(__FILENAME__,"Attaching to display %p", (void*)state.display);
    } else {
        AppLog::Info(__FILENAME__,"Reattaching to display %p", (void*)state.display);
    }

    EGLint maj, min;
    eglInitialize(state.display, &maj, &min);
    LOGWgl("eglInitialize");
    AppLog::Info(__FILENAME__,"eglInitialize %d %d", maj, min);

    if(state.surface == EGL_NO_SURFACE)
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
        eglChooseConfig(state.display, attribs, &state.config, 1, &numConfigs);
        LOGWgl("eglChooseConfig");
        assert(numConfigs);

        EGLint r, g, b, a, d;
        eglGetConfigAttrib(state.display, state.config, EGL_RED_SIZE, &r);
        eglGetConfigAttrib(state.display, state.config, EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(state.display, state.config, EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(state.display, state.config, EGL_ALPHA_SIZE, &a);
        eglGetConfigAttrib(state.display, state.config, EGL_DEPTH_SIZE, &d);
        AppLog::Info(__FILENAME__,"RGBAD %d %d %d %d %d", r, g, b, a, d);

        auto window = Platform_GetWindow();
        AppLog::Info(__FILENAME__,"window %p", window);

        state.surface = eglCreateWindowSurface(state.display, state.config, window, NULL);
        LOGWgl("eglCreateWindowSurface");

        AppLog::Info(__FILENAME__,"Attaching to surface %p", (void*)state.surface);
    } else {
        AppLog::Info(__FILENAME__,"Reattaching to surface %p", (void*)state.surface);
    }

    if(state.context == EGL_NO_CONTEXT)
    {
        EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE }; // request OpenGL ES 2.0
        state.context = eglCreateContext(state.display, state.config, NULL, attribs);
        LOGWgl("eglCreateContext");

        AppLog::Info(__FILENAME__,"Attaching to context %p", (void*)state.context);
    } else {
        AppLog::Info(__FILENAME__,"Reattaching to context %p", (void*)state.context);
    }

    eglMakeCurrent(state.display, state.surface, state.surface, state.context);
    LOGWgl("eglMakeCurrent");

    EGLint w, h;
    eglQuerySurface(state.display, state.surface, EGL_WIDTH, &w);
    eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &h);

    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &state.imp_fmt);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE,   &state.imp_type);
    AppLog::Info(__FILENAME__,"Supported Color Format/Type: %x/%x\n", state.imp_fmt, state.imp_type);

    auto fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(fbStatus == GL_FRAMEBUFFER_COMPLETE);

    width = w;
    height = h;

    // Check openGL on the system
    auto opengl_info = {GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS};
    for (auto name : opengl_info) {
        auto info = glGetString(name);
        AppLog::Info(__FILENAME__,"OpenGL Info: %s", info);
    }

    // shader

    char* szObject;
    GLint status = 0;
    GLint logsize = 0;

    auto fnCompileShader = [&] (const GLuint type, GLuint& hShader, const char* szSource)
    {
        hShader = glCreateShader(type);
        assert(hShader != 0);

        glShaderSource(hShader, 1, &szSource, NULL);
        glCompileShader(hShader);
        glGetShaderiv(hShader, GL_COMPILE_STATUS, &status);
        if(status != 1)
        {
            AppLog::Info(__FILENAME__, "%s... compilation status %X", szObject, status );
            glGetShaderiv( hShader, GL_INFO_LOG_LENGTH, &logsize );
            if( logsize > 1 )
            {
                std::unique_ptr<char[]> logbuffer( new char[logsize] );
                glGetShaderInfoLog( hShader, logsize - 1, NULL, logbuffer.get());
                AppLog::Warn(__FILENAME__, logbuffer.get());
            }
        }
    };

    auto fnLinkProgram = [&] (GLuint &hProgram, const GLuint hVS, const GLuint hFS)
    {
        hProgram = glCreateProgram();
        assert(hProgram != 0);

        glAttachShader(hProgram, hVS);
        glAttachShader(hProgram, hFS);
        glLinkProgram(hProgram);
        glGetProgramiv(hProgram, GL_LINK_STATUS, &status);
        if(status != 1)
        {
            AppLog::Info(__FILENAME__, "%s... link status %X", szObject, status );

            glGetProgramiv( hProgram, GL_INFO_LOG_LENGTH, &logsize );
            if( logsize > 1 )
            {
                std::unique_ptr<char[]> logbuffer( new char[logsize] );
                glGetProgramInfoLog( hProgram, logsize - 1, NULL, logbuffer.get());
                AppLog::Warn(__FILENAME__, logbuffer.get());
            }
        }
    };

    auto fnAttribBinder = [&] (GLuint hPrg, GLenum id, const char* sz)
    {
        glBindAttribLocation(hPrg, 0, sz);
        auto rc = glGetAttribLocation(hPrg, sz);
        if(rc == -1) AppLog::Warn(__FILENAME__, "%s... attribute %s not found", szObject, sz);
        (*state.pBindmap)[ std::make_pair(hPrg, id) ] = GLuint(rc);
        AppLog::Info(__FILENAME__, "%s... %s bound to %d", szObject, sz, rc );
    };

    auto fnUniformBinder = [&] (GLuint hPrg, GLenum id, const char* sz)
    {
        auto rc = glGetUniformLocation(hPrg, sz);
        if(rc == -1) AppLog::Warn(__FILENAME__, "%s... uniform %s not found", szObject, sz);
        (*state.pBindmap)[ std::make_pair(hPrg, id) ] = GLuint(rc);
        AppLog::Info(__FILENAME__, "%s... %s bound to %d", szObject, sz, rc );
    };

    ///////////////////

    szObject = (char*)"geom";
    fnCompileShader( GL_VERTEX_SHADER, state.hGeomVS,
                     "uniform mat4 u_MVP; "
                             "attribute mediump vec3 a_Position; "
                             "attribute vec3 a_Color; "
                             "varying vec4 v_Color; "
                             "void main() "
                             "{ "
                             "   v_Color = vec4( a_Color, 1.0 ); "
                             "   gl_Position = u_MVP * vec4( a_Position, 1.0 ); "
                             "} "
    );
    fnCompileShader( GL_FRAGMENT_SHADER, state.hGeomFS,
                     "precision mediump float; "
                             "varying vec4 v_Color; "
                             "void main() "
                             "{ "
                             "    gl_FragColor = v_Color; "
                             "} "
    );
    fnLinkProgram(state.hGeomPrg, state.hGeomVS, state.hGeomFS);

    fnAttribBinder(state.hGeomPrg, GL_VERTEX_ARRAY, "a_Position");
    fnAttribBinder(state.hGeomPrg, GL_COLOR_ARRAY, "a_Color");

    fnUniformBinder(state.hGeomPrg, GL_MODELVIEW, "u_MVP");

    ///////////////////

    szObject = (char*)"menu";
    fnCompileShader( GL_VERTEX_SHADER, state.hMenuVS,
                     "uniform mat4 u_MVP; "
                             "attribute mediump vec3 a_Position; "
                             "attribute mediump vec2 a_TexPosition; "
                             "varying mediump vec2 v_TexPosition; "
                             "void main() "
                             "{ "
                             "   v_TexPosition = a_TexPosition; "
                             "   gl_Position = u_MVP * vec4( a_Position, 1.0 ); "
                             "} "
    );
    fnCompileShader( GL_FRAGMENT_SHADER, state.hMenuFS,
                     "precision mediump float; "
                             "uniform sampler2D u_Texture; "
                             "uniform vec4 u_Color; "
                             "varying mediump vec2 v_TexPosition; "
                             "void main() "
                             "{ "
                             "    gl_FragColor = texture2D( u_Texture, v_TexPosition ) * u_Color; "
                             "} "
    );
    fnLinkProgram(state.hMenuPrg, state.hMenuVS, state.hMenuFS);

    fnAttribBinder(state.hMenuPrg, GL_VERTEX_ARRAY, "a_Position");
    fnAttribBinder(state.hMenuPrg, GL_TEXTURE_COORD_ARRAY, "a_TexPosition");

    fnUniformBinder(state.hMenuPrg, GL_TEXTURE0, "u_Texture");
    fnUniformBinder(state.hMenuPrg, GL_CURRENT_COLOR, "u_Color");
    fnUniformBinder(state.hMenuPrg, GL_MODELVIEW, "u_MVP");

    ///////////////////

    szObject = (char*)"pointer";
    fnCompileShader( GL_VERTEX_SHADER, state.hPointerVS,
                     "uniform mat4 u_MVP; "
                             "attribute mediump vec3 a_Position; "
                             "void main() "
                             "{ "
                             "   gl_Position = u_MVP * vec4( a_Position, 1.0 ); "
                             "} "
    );
    fnCompileShader( GL_FRAGMENT_SHADER, state.hPointerFS,
                     "precision mediump float; "
                             "uniform vec4 u_Color; "
                             "void main() "
                             "{ "
                             "    gl_FragColor = u_Color; "
                             "} "
    );
    fnLinkProgram(state.hPointerPrg, state.hPointerVS, state.hPointerFS);

    fnAttribBinder(state.hPointerPrg, GL_VERTEX_ARRAY, "a_Position");

    fnUniformBinder(state.hPointerPrg, GL_CURRENT_COLOR, "u_Color");
    fnUniformBinder(state.hPointerPrg, GL_MODELVIEW, "u_MVP");

    ///////////////////

    state.hProgram = state.hGeomPrg;
    glUseProgram(state.hProgram);

    // defaults

    glViewport( 0, 0, width, height );

    // Initialize GL state.

    state.frameDuration = 0.f;

// i thought this would fix tearing but by itself it doesnt :(
//    glEnable( GL_DEPTH_TEST );
//    glDepthFunc( GL_LESS );

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9Release()
{
    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state.context != EGL_NO_CONTEXT) {
            AppLog::Info(__FILENAME__,"Releasing context %p", (void *) state.context);
            eglDestroyContext(state.display, state.context);
        }
        if (state.surface != EGL_NO_SURFACE) {
            AppLog::Info(__FILENAME__,"Releasing surface %p", (void *) state.surface);
            eglDestroySurface(state.display, state.surface);
        }
        AppLog::Info(__FILENAME__,"Releasing display %p", (void *) state.display);
        eglTerminate(state.display);
#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }
    state.display = EGL_NO_DISPLAY;
    state.context = EGL_NO_CONTEXT;
    state.surface = EGL_NO_SURFACE;

    // shader

    if (state.hGeomFS) glDeleteShader(state.hGeomFS);
    if (state.hGeomVS) glDeleteShader(state.hGeomVS);
    if (state.hGeomPrg) glDeleteProgram(state.hGeomPrg);

    state.hGeomFS = state.hGeomVS = state.hGeomPrg = 0;

    if (state.hMenuFS) glDeleteShader(state.hMenuFS);
    if (state.hMenuVS) glDeleteShader(state.hMenuVS);
    if (state.hMenuPrg) glDeleteProgram(state.hMenuPrg);

    state.hMenuFS = state.hMenuVS = state.hMenuPrg = 0;

    if (state.hPointerFS) glDeleteShader(state.hPointerFS);
    if (state.hPointerVS) glDeleteShader(state.hPointerVS);
    if (state.hPointerPrg) glDeleteProgram(state.hPointerPrg);

    state.hPointerFS = state.hPointerVS = state.hPointerPrg = 0;

    delete state.pBindmap; state.pBindmap = nullptr;
    delete state.pModelmxDeq; state.pModelmxDeq = nullptr;
}

void gl9BeginFrame()
{
    if(!state.display) return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable( GL_DEPTH_TEST );

    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    state.frameTimerMSEC = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9EndFrame()
{
    if(!state.display) return;

    eglSwapBuffers(state.display, state.surface);

    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    state.frameTimerMSEC = spec.tv_sec * 1000 + spec.tv_nsec / 1000000 - state.frameTimerMSEC;
    state.frameDuration = state.frameDuration * .75f + (float)state.frameTimerMSEC * 1E-3f * .25f;
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9UseProgram(GLint p)
{
    switch(p)
    {
        case GL9_WORLD: state.hProgram = state.hGeomPrg; break;
        case GL9_MENU: state.hProgram = state.hMenuPrg; break;
        case GL9_POINTER: state.hProgram = state.hPointerPrg; break;
        default: return;
    }
    glUseProgram(state.hProgram);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9MatrixMode(GLenum mode)
{
    state.eMXMode = mode;
}

static void gl9UploadUniformMVPMatrix()
{
    assert((*state.pModelmxDeq).size() > 0);

#ifdef DEBUG
    GLuint u = (*state.pBindmap).at( std::pair<GLint,GLenum>(state.hProgram, GL_MODELVIEW) ); // throws
#else
    GLuint u = (*state.pBindmap)[ std::pair<GLint,GLenum>(state.hProgram, GL_MODELVIEW) ];
#endif
    glm::mat4 mvp = state.mxProjection * (*state.pModelmxDeq)[0];
    glUniformMatrix4fv(u, 1, GL_FALSE, glm::value_ptr(mvp));
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9LoadIdentity()
{
    glm::mat4 m(1.f);

    state.pModelmxDeq->emplace_front(m);

    gl9UploadUniformMVPMatrix();
}
void gl9LoadMatrixf(const GLfloat *f)
{
    glm::mat4 m = glm::make_mat4x4(f);

    if(state.eMXMode == GL_PROJECTION)
    {
        state.mxProjection = m;
        return;
    }

    state.pModelmxDeq->emplace_front(m);

    gl9UploadUniformMVPMatrix();
}
void gl9MultMatrixf(const GLfloat *f)
{
    assert(state.eMXMode == GL_MODELVIEW);
    assert((*state.pModelmxDeq).size() > 0);

    glm::mat4 m = (*state.pModelmxDeq)[0] * glm::make_mat4x4(f);
    (*state.pModelmxDeq)[0] = m;

    gl9UploadUniformMVPMatrix();
}
void gl9PushMatrix()
{
    assert(state.eMXMode == GL_MODELVIEW);

    glm::mat4 m = (*state.pModelmxDeq).front();
    (*state.pModelmxDeq).push_front(m);

    // no change to uniform
}
void gl9PopMatrix()
{
    assert(state.eMXMode == GL_MODELVIEW);
    assert((*state.pModelmxDeq).size() > 0);

    (*state.pModelmxDeq).pop_front();

    gl9UploadUniformMVPMatrix();
}

void gl9PushClientAttrib(GLbitfield mask) { /* do nothing*/ }
void gl9PopClientAttrib() { /* do nothing */ }

void gl9EnableClientState(GLenum cap)
{
#ifdef DEBUG
    state.eClientState = (*state.pBindmap).at( std::pair<GLint,GLenum>(state.hProgram, cap) ); // throws
#else
    state.eClientState = (*state.pBindmap)[std::pair<GLint,GLenum>(state.hProgram, cap)];
#endif
    glEnableVertexAttribArray( state.eClientState );

    if(cap == GL_TEXTURE_COORD_ARRAY) {
        // https://stackoverflow.com/questions/23001842/opengl-es-2-0-gl-texture1
        // https://ycpcs.github.io/cs370-fall2017/labs/lab21.html
        const GLuint textureUnit = 0; // shader only has one
        glActiveTexture(textureUnit +GL_TEXTURE0);

#ifdef DEBUG
        GLuint u = (*state.pBindmap).at( std::pair<GLint,GLenum>(state.hProgram, GL_TEXTURE0) ); // throws
#else
        GLuint u = (*state.pBindmap)[std::pair<GLint,GLenum>(state.hProgram, GL_TEXTURE0)];
#endif
        glUniform1i(u, textureUnit);
    }
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9DisableClientState(GLenum cap) {
#ifdef DEBUG
    state.eClientState = (*state.pBindmap).at( std::pair<GLint,GLenum>(state.hProgram, cap) ); // throws
#else
    state.eClientState = (*state.pBindmap)[std::pair<GLint,GLenum>(state.hProgram, cap)];
#endif
    glDisableVertexAttribArray( state.eClientState );

    if(cap == GL_TEXTURE_COORD_ARRAY) {
        ;
    }
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, 3 /* RGB */, type, GL_FALSE, stride, pointer);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, size, type, GL_FALSE, stride, pointer);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9TexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, 2 /* 2d texture */, type, GL_FALSE, stride, pointer);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9Color3fv(const GLfloat *f)
{
#ifdef DEBUG
    GLuint u = (*state.pBindmap).at( std::make_pair( state.hProgram, GL_CURRENT_COLOR ) ); // throws
#else
    GLuint u = (*state.pBindmap)[std::make_pair( state.hProgram, GL_CURRENT_COLOR )];
#endif
    GLfloat g[] = {f[0],f[1],f[2],1.f}; // opaque
    glUniform4fv( u, 1, g );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9Color4fv(const GLfloat *f)
{
#ifdef DEBUG
    GLuint u = (*state.pBindmap).at( std::make_pair( state.hProgram, GL_CURRENT_COLOR ) ); // throws
#else
    GLuint u = (*state.pBindmap)[std::make_pair( state.hProgram, GL_CURRENT_COLOR )];
#endif
    glUniform4fv( u, 1, f );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9Circle(glm::vec3 const & axisIn, glm::vec3 const & axisUp, glm::vec3 const & posCenter, float radius, uint steps) {}
void gl9Normal( const glm::vec3 & v0, const glm::vec3 & v1, const glm::vec3 & v2, glm::vec3 const & norm, float k) {}

void gl9ActiveTexture( GLenum texture ) {
    ::glActiveTexture(texture);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9BindBuffer(GLenum target, GLuint buffer) { ::glBindBuffer ( target,  buffer);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9BindTexture( GLenum target, GLuint texture ) { ::glBindTexture(  target,  texture );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9BlendFunc( GLenum sfactor, GLenum dfactor ) { ::glBlendFunc(  sfactor,  dfactor );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { ::glBufferData( target,  size,  data, usage);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { ::glBufferSubData( target,  offset,  size, data);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9ClearColor3fv(const GLfloat* rgb) { glClearColor(rgb[0],rgb[1],rgb[2],0);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
};
void gl9DeleteBuffers(GLsizei n, const GLuint *buffers) { ::glDeleteBuffers(n, buffers);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9DepthRange( GLclampf near_val, GLclampf far_val ) { ::glDepthRangef(  near_val,  far_val );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9Disable( GLenum cap ) {
    if(cap != GL_TEXTURE_2D) // unsupported in ogles2
        ::glDisable(  cap );
}
void gl9DrawArrays( GLenum mode, GLint first, GLsizei count ) { ::glDrawArrays(  mode,  first,  count );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) { ::glDrawElements(  mode,  count,  type,  indices );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9DrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels ) {}
void gl9Enable( GLenum cap ) {
    if(cap != GL_TEXTURE_2D) // unsupported in ogles2
        ::glEnable(  cap );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9Flush( void ) { ::glFlush();
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9GenBuffers(GLsizei n, GLuint *buffers) { ::glGenBuffers(n, buffers);
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9GenTextures( GLsizei n, GLuint *textures ) { ::glGenTextures(  n, textures );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9PopAttrib() {}
void gl9PushAttrib( GLbitfield mask ) {}
void gl9ReadColor3fv( GLint x, GLint y, GLfloat *f )
{
    // https://stackoverflow.com/questions/15592288/gl-invalid-framebuffer-operation-android-ndk-gl-framebuffer-and-glreadpixels-ret#15712743
    GLint fbContext;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbContext);
    assert(fbContext == 0);

    auto fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(fbStatus == GL_FRAMEBUFFER_COMPLETE);

    uint32_t data = 0xFFFFFFFF;
    ::glReadPixels( x,  y, 1, 1, state.imp_fmt, state.imp_type, &data );
    auto readCode = glGetError();
    assert(readCode == GL_NO_ERROR);
    assert(state.imp_type == GL_UNSIGNED_SHORT_5_6_5); // the only codepath here!

    int r = (data >> 11) & 0b11111; // 5 red
    int g = (data >>  5) & 0b111111; // 6 green
    int b = (data >>  0) & 0b11111; // 5 blue
    //GLenum err = glGetError();

    f[0] = float(r) / float(0b11111); // red
    f[1] = float(g) / float(0b111111); // green
    f[2] = float(b) / float(0b11111); // blue
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9TexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) { ::glTexImage2D(  target,  level,  internalFormat,  width,  height,  border,  format,  type,  pixels );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9TexParameteri( GLenum target, GLenum pname, GLint param ) { ::glTexParameteri(  target,  pname,  param );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}
void gl9Viewport( GLint x, GLint y, GLsizei width, GLsizei height ) { ::glViewport(  x,  y,  width,  height );
#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
}

void gl9RenderFrame(std::function<void(void)> fnRender, const char *filename)
{
    assert(state.imp_type == GL_UNSIGNED_SHORT_5_6_5); // the only codepath here!

    struct pxFormat565 {
        union {
            uint16_t u;
            struct {
                uint16_t b : 5;
                uint16_t g : 6;
                uint16_t r : 5;
            };
        };
    };
    struct pxFormat888 {
        uint8_t r, g, b;
    };

    EGLint w, h;
    eglQuerySurface(state.display, state.surface, EGL_WIDTH, &w);
    eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &h);

    auto pxData565 = new pxFormat565[w * h];

    GLuint rbo;
    glGenRenderbuffers(1,&rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, w, h);

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    GLuint dbo;
    glGenRenderbuffers(1,&dbo);
    glBindRenderbuffer(GL_RENDERBUFFER, dbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    GLuint fbo;
    glGenFramebuffers(1,&fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dbo);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable( GL_DEPTH_TEST );

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    fnRender();

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    glFinish();

    ::glReadPixels( 0, 0, w, h, state.imp_fmt, state.imp_type, pxData565 );

    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(1,&fbo);
    glDeleteRenderbuffers(1,&rbo);
    glDeleteRenderbuffers(1,&dbo);

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    // convert 565 to 888
    auto pxData888 = new pxFormat888[w * h];
    for(int r=0; r<h; r++) for(int c=0; c<w; c++)
    {
        pxData888[ r * w + c ].r = uint(255 * uint(pxData565[ r * w + c ].r)) >> 5; // todo: check should be 256 instead?
        pxData888[ r * w + c ].g = uint(255 * uint(pxData565[ r * w + c ].g)) >> 6;
        pxData888[ r * w + c ].b = uint(255 * uint(pxData565[ r * w + c ].b)) >> 5;
    }

    auto rows = new png_bytep[h];
    for (int i = 0; i < h; ++i) rows[i] = (png_bytep)(pxData888 + (h - i) * w); // nb: h-i inverts y axis

    int fint = 0;
    FILE *fp = 0;
    png_structp png_ptr = 0;
    png_infop info_ptr = 0;

    char fqFilename[256];
    int n = snprintf(fqFilename, sizeof(fqFilename), "%s/%s.png", Platform_ExternalPath(), filename);
    if(n < 0) AppLog::Warn(__FILENAME__, "snprintf format error");
    if(n > sizeof(fqFilename)) AppLog::Warn(__FILENAME__, "snprintf buffer size error");

    AppLog::Info(__FILENAME__, "%s opening %s for write", __func__, fqFilename );
    do{
        fint = open(fqFilename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if(fint < 0) { AppLog::Warn(__FILENAME__, "open failed"); break; }
        fp = fdopen(fint, "w");
        if(fp == 0) { AppLog::Warn(__FILENAME__, "fdopen failed"); break; }

        if( !(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) ) break;
        if( !(info_ptr = png_create_info_struct(png_ptr)) ) break;

        png_init_io(png_ptr, fp);
        png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png_ptr, info_ptr);
        png_set_packing(png_ptr);
        png_write_image(png_ptr, rows);
        png_write_end(png_ptr, info_ptr);
    } while(false);

    if(png_ptr) png_free(png_ptr, 0);
    if(info_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
    if(fp) fclose(fp);
    if(fint > 0) close(fint);
    delete[] rows;
    delete[] pxData888;
    delete[] pxData565;
}
