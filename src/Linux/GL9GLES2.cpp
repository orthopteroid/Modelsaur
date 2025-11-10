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
#include <deque>
#include <map>
#include <list>

#include <stdbool.h>
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

#define PNG_SETJMP_SUPPORTED

#include <png.h>
#include <pngconf.h>

#define GLX_GLXEXT_PROTOTYPES

#include <GL/glx.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "GL9.hpp"

#include "AppPlatform.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

Display* Linux_GetDisplayPtr();
Window Linux_GetWindow();
GLXFBConfig Linux_GetFBConfig();

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

// Helper to check for extension string presence.  Adapted from:
//   http://www.opengl.org/resources/features/OGLextensions/
static bool isExtensionSupported(const char *extList, const char *extension)
{
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    for (start=extList;;) {
        where = strstr(start, extension);

        if (!where)
            break;

        terminator = where + strlen(extension);

        if ( where == start || *(where - 1) == ' ' )
            if ( *terminator == ' ' || *terminator == '\0' )
                return true;

        start = terminator;
    }

    return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}

////////////////////////

struct State
{
    GLXContext glxContext;

    GLuint hGeomFS;
    GLuint hGeomVS;
    GLuint hGeomPrg;

    GLuint hMenuFS;
    GLuint hMenuVS;
    GLuint hMenuPrg;

    GLuint hPointerFS;
    GLuint hPointerVS;
    GLuint hPointerPrg;

    // maps <prog, enum> to int
    using BindmapType = std::map< std::pair<GLuint, GLenum>, GLuint>;
    BindmapType *pBindmap = 0; // can't be static

    using DeqType = std::deque<glm::mat4>;
    DeqType *pModelmxDeq = 0;

    glm::mat4 mxProjection;
    GLenum eMXMode;
    GLuint hProgram;
    GLuint eClientState;

    uint frameTimer;
    uint frameTimerValue;
    float frameDuration;
};
static State state;

////////////////////////

void gl9Bind(int32_t &w, int32_t &h)
{
    assert( Linux_GetDisplayPtr() );
    assert( Linux_GetFBConfig() );
    assert( Linux_GetWindow() );

    state.pBindmap = new State::BindmapType();
    state.pModelmxDeq = new State::DeqType();

    // NOTE: It is not necessary to create or make current to a context before
    // calling glXGetProcAddressARB
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

    // Get the default screen's GLX extension list
    const char *glxExts = glXQueryExtensionsString(
        Linux_GetDisplayPtr(), DefaultScreen( Linux_GetDisplayPtr() )
    );

    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctxErrorHandler);

    // Check for the GLX_ARB_create_context extension string and the function.
    // If either is not present, use GLX 1.3 context creation method.
    if ( !isExtensionSupported( glxExts, "GLX_ARB_create_context" ) || !glXCreateContextAttribsARB )
    {
        state.glxContext = glXCreateNewContext(
            Linux_GetDisplayPtr(), Linux_GetFBConfig(), GLX_RGBA_TYPE, 0, True
        );
    }
    else
    {
        int context_attribs[] =
            {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
                GLX_CONTEXT_MINOR_VERSION_ARB, 0,
                GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_ES_PROFILE_BIT_EXT,
                None
            };

        state.glxContext = glXCreateContextAttribsARB(
            Linux_GetDisplayPtr(), Linux_GetFBConfig(), 0, True, context_attribs
        );
    }

    XSync( Linux_GetDisplayPtr(), False ); // sync errors
    XSetErrorHandler( oldHandler );

    assert( !ctxErrorOccurred && state.glxContext );
    Bool isGLXContextDirect = glXIsDirect( Linux_GetDisplayPtr(), state.glxContext ); // Bool is int
    if(!isGLXContextDirect) AppLog::Info(__FILENAME__, "glXIsDirect failed");

    glXMakeCurrent( Linux_GetDisplayPtr(), Linux_GetWindow(), state.glxContext );

    {
        auto szVend = (char*)glGetString(GL_VENDOR);
        auto szRend = (char*)glGetString(GL_RENDERER);
        auto szVers = (char*)glGetString(GL_VERSION);
        AppLog::Info(__FILENAME__, "%s - %s - %s", szVend, szRend, szVers);
    }

    fflush(stdout);

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

    // https://learnopengl.com/Lighting/Basic-Lighting
    szObject = (char*)"geom";
    fnCompileShader( GL_VERTEX_SHADER, state.hGeomVS,
        "uniform mat4 u_MVP; "
        "attribute mediump vec3 a_Position; "
        "attribute vec3 a_Color; "
        "varying vec4 v_Color; "
        "attribute vec3 a_Normal; " // lighting
        "varying vec3 v_Normal; " // lighting
        "varying vec3 v_FragPos; " // lighting
        "void main() "
        "{ "
        "   v_Color = vec4( a_Color, 1.0 ); "
        "   gl_Position = u_MVP * vec4( a_Position, 1.0 ); "
        "   v_Normal = a_Normal; " // lighting
        "   v_FragPos = a_Position; " // lighting. no transform
        "} "
    );
    fnCompileShader( GL_FRAGMENT_SHADER, state.hGeomFS,
        "precision mediump float; "
        "varying vec4 v_Color; "
        "uniform vec3 u_LightPos; " // lighting
        "varying vec3 v_Normal; " // lighting
        "varying vec3 v_FragPos; " // lighting
        "void main() "
        "{ "
        "    gl_FragColor = v_Color * max( dot( v_Normal, normalize( u_LightPos - v_FragPos ) ), 0.75 ); "
        "} "
    );
    fnLinkProgram(state.hGeomPrg, state.hGeomVS, state.hGeomFS);

    fnAttribBinder(state.hGeomPrg, GL_VERTEX_ARRAY, "a_Position");
    fnAttribBinder(state.hGeomPrg, GL_COLOR_ARRAY, "a_Color");
    fnAttribBinder(state.hGeomPrg, GL_NORMAL_ARRAY, "a_Normal"); // lighting

    fnUniformBinder(state.hGeomPrg, GL_MODELVIEW, "u_MVP");
    fnUniformBinder(state.hGeomPrg, GL_LIGHT0, "u_LightPos"); // lighting

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

    // default

    state.hProgram = state.hGeomPrg;
    glUseProgram(state.hProgram);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glEnable( GL_BLEND ); // because items have alpha
    gl9Enable( GL_DEPTH_TEST );
    gl9DepthRange( 0.f, 1.f );
    glClearColor( .0f, .0f, .0f, .0f ); // rgba
    //glDepthRange( 1.f, 0.f ); // make par-of-the helix visible but not cursor! :(

    //glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ); // ?
    //gl9ShadeModel(GL_FLAT); // GL_SMOOTH vs GL_FLAT
    //glFrontFace(GL_CW); // GL_CW vs GL_CCW is default

    XWindowAttributes xWindowAttributes;
    XGetWindowAttributes( Linux_GetDisplayPtr(), Linux_GetWindow(), &xWindowAttributes );
    gl9Viewport( 0, 0, xWindowAttributes.width, xWindowAttributes.height );
    w = xWindowAttributes.width;
    h = xWindowAttributes.height;

//    glGenQueries(1, &state.frameTimer);

    state.frameDuration = 0.f;

    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
}

void gl9Release()
{
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

    glXMakeCurrent( Linux_GetDisplayPtr(), Linux_GetWindow(), state.glxContext );
    glXDestroyContext( Linux_GetDisplayPtr(), state.glxContext );

    delete state.pBindmap; state.pBindmap = nullptr;
    delete state.pModelmxDeq; state.pModelmxDeq = nullptr;}

void gl9BeginFrame()
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );

}

void gl9EndFrame()
{
    gl9Flush(); // todo: unnecess?

    glXSwapBuffers ( Linux_GetDisplayPtr(), Linux_GetWindow() );

    state.frameDuration = state.frameDuration * .75f + (float)state.frameTimerValue * 10E-9f * .25f;
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

void gl9LoadLightf(const GLfloat *f)
{
#ifdef DEBUG
    GLuint u = (*state.pBindmap).at( std::pair<GLint,GLenum>(state.hProgram, GL_LIGHT0) ); // throws
#else
    GLuint u = (*state.pBindmap)[ std::pair<GLint,GLenum>(state.hProgram, GL_LIGHT0) ];
#endif
    glUniform3fv(u, 1, f);
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
}
void gl9ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, 3 /* RGB */, type, GL_FALSE, stride, pointer);
}
void gl9VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, size, type, GL_FALSE, stride, pointer);
}
void gl9TexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
    glVertexAttribPointer(state.eClientState, 2 /* 2d texture */, type, GL_FALSE, stride, pointer);
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
}

void gl9Color4fv(const GLfloat *f)
{
#ifdef DEBUG
    GLuint u = (*state.pBindmap).at( std::make_pair( state.hProgram, GL_CURRENT_COLOR ) ); // throws
#else
    GLuint u = (*state.pBindmap)[std::make_pair( state.hProgram, GL_CURRENT_COLOR )];
#endif
    glUniform4fv( u, 1, f );
}

void gl9Circle(glm::vec3 const & axisIn, glm::vec3 const & axisUp, glm::vec3 const & posCenter, float radius, uint steps) {}
void gl9Normal( const glm::vec3 & v0, const glm::vec3 & v1, const glm::vec3 & v2, glm::vec3 const & norm, float k) {}

void gl9ActiveTexture( GLenum texture ) { ::glActiveTexture(texture); }
void gl9BindBuffer(GLenum target, GLuint buffer) { ::glBindBuffer ( target,  buffer); }
void gl9BindTexture( GLenum target, GLuint texture ) { ::glBindTexture( target,  texture ); }
void gl9BlendFunc( GLenum sfactor, GLenum dfactor ) { ::glBlendFunc(  sfactor,  dfactor ); }
void gl9BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { ::glBufferData( target,  size,  data, usage); }
void gl9BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { ::glBufferSubData( target,  offset,  size, data); }
void gl9Clear( GLbitfield mask ) { ::glClear(  mask ); }
void gl9ClearColor3fv(const GLfloat* rgb) { glClearColor(rgb[0],rgb[1],rgb[2],0); };
void gl9DeleteBuffers(GLsizei n, const GLuint *buffers) { ::glDeleteBuffers(n, buffers); }
void gl9DepthRange( GLclampf near_val, GLclampf far_val ) { ::glDepthRangef(  near_val,  far_val ); }
void gl9DepthRange( GLclampd near_val, GLclampd far_val ) { ::glDepthRange(  near_val,  far_val ); }
void gl9Disable( GLenum cap ) {
    assert(cap != GL_TEXTURE_2D); // unsupported in ogles2
    ::glDisable( cap );
}
void gl9DrawArrays( GLenum mode, GLint first, GLsizei count ) { ::glDrawArrays(  mode,  first,  count ); }
void gl9DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) { ::glDrawElements(  mode,  count,  type,  indices ); }
void gl9DrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels ) { ::glDrawPixels(  width,  height,  format,  type,  pixels ); }
void gl9Enable( GLenum cap ) {
    assert(cap != GL_TEXTURE_2D); // unsupported in ogles2
    ::glEnable( cap );
}
void gl9Flush( void ) { ::glFlush(); }
void gl9GenBuffers(GLsizei n, GLuint *buffers) { ::glGenBuffers(n, buffers); }
void gl9GenTextures( GLsizei n, GLuint *textures ) { ::glGenTextures(  n, textures ); }
void gl9PopAttrib( void ) { }
void gl9PushAttrib( GLbitfield mask ) {}
void gl9ReadColor3fv( GLint x, GLint y, GLfloat *f )
{
	//assert(state.imp_type == GL_???); // the only codepath here!

    const float f256 = float( 256 );
    uint8_t barr[4] = {0,0,0,0}; // 1 extra
    ::glReadPixels( x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, barr ); // todo: check linux
    f[0] = float(barr[0]) / f256;
    f[1] = float(barr[1]) / f256;
    f[2] = float(barr[2]) / f256;
}
void gl9TexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) { ::glTexImage2D(  target,  level,  internalFormat,  width,  height,  border,  format,  type,  pixels ); }
void gl9TexParameteri( GLenum target, GLenum pname, GLint param ) { ::glTexParameteri(  target,  pname,  param ); }
void gl9Viewport( GLint x, GLint y, GLsizei width, GLsizei height ) { ::glViewport(  x,  y,  width,  height ); }
