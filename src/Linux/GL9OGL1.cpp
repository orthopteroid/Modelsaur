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

#define GLX_GLXEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GL/glx.h>

#include "GL9.hpp"

#include "AppPlatform.hpp"
#include "AppLog.hpp"
#include "AppTypes.hpp"

#define PNG_SETJMP_SUPPORTED

#include <png.h>
#include <pngconf.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

Display* Linux_GetDisplayPtr();
Window Linux_GetWindow();
GLXFBConfig Linux_GetFBConfig();

////////////////////////

struct State
{
    GLXContext glxContext;
    XVisualInfo *xVisualPtr;

    bool showDepthBuffer;
    uint frameTimer;
    uint frameTimerValue;
    float frameDuration;
};
static State state;

////////////////////////

void gl9Bind(int32_t &w, int32_t &h)
{
    setlocale(LC_ALL, "");
    XSupportsLocale();
    XSetLocaleModifiers("@im=none");

    state.xVisualPtr = glXGetVisualFromFBConfig( Linux_GetDisplayPtr(), Linux_GetFBConfig() );

    state.glxContext = glXCreateContext( Linux_GetDisplayPtr(), state.xVisualPtr, NULL, GL_TRUE );

    glXMakeCurrent( Linux_GetDisplayPtr(), Linux_GetWindow(), state.glxContext );

    {
        auto szVend = (char*)glGetString(GL_VENDOR);
        auto szRend = (char*)glGetString(GL_RENDERER);
        auto szVers = (char*)glGetString(GL_VERSION);
        AppLog::Info(__FILENAME__, "%s - %s - %s", szVend, szRend, szVers);
    }

    fflush(stdout);

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
    XGetWindowAttributes( Linux_GetDisplayPtr(), Linux_GetWindow(), &xWindowAttributes );
    glViewport( 0, 0, xWindowAttributes.width, xWindowAttributes.height );
    w = xWindowAttributes.width;
    h = xWindowAttributes.height;

    glGenQueries(1, &state.frameTimer);

    state.frameDuration = 0.f;

    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
}

void gl9Release()
{
    XFree( state.xVisualPtr );

    glXMakeCurrent( Linux_GetDisplayPtr(), None, NULL );
    glXDestroyContext( Linux_GetDisplayPtr(), state.glxContext );
}

void gl9UseProgram(GLint p)
{
    const float defaultColor[] = {1,1,1};
    ::glColor3fv(defaultColor);
}

void gl9BeginFrame()
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glDisable(GL_TEXTURE_2D); // hack!?!?
	glEnable( GL_DEPTH_TEST );

    glBeginQuery( GL_TIME_ELAPSED, state.frameTimer );
}

void gl9EndFrame()
{
    gl9Flush(); // todo: unnecess?
    
    glEndQuery( GL_TIME_ELAPSED );

    if(state.showDepthBuffer)
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

    glXSwapBuffers( Linux_GetDisplayPtr(), Linux_GetWindow() );

    int done = 0;
    while( !done ) glGetQueryObjectiv( state.frameTimer, GL_QUERY_RESULT_AVAILABLE, &done );
    glGetQueryObjectuiv( state.frameTimer, GL_QUERY_RESULT, &state.frameTimerValue );

    state.frameDuration = state.frameDuration * .75f + (float)state.frameTimerValue * 10E-9f * .25f;
}

void gl9Circle(glm::vec3 const & axisIn, glm::vec3 const & axisUp, glm::vec3 const & posCenter, float radius, uint steps)
{
    glm::vec3 pos = axisUp * glm::vec3(radius);
    glm::quat q = glm::angleAxis(float(M_PI) / float(steps), axisIn);
    glBegin(GL_LINE_LOOP);
    for(uint i=0;i<steps;i++)
    {
        glm::vec3 point = posCenter + pos;
        glVertex3fv(glm::value_ptr(point));
        pos = q * pos * glm::conjugate(q);
    }
    glEnd();
}

void gl9Normal(
    const glm::vec3 & v0,
    const glm::vec3 & v1,
    const glm::vec3 & v2,
    glm::vec3 const & norm, float k)
{
    const glm::vec3 oneThird(1.f/3.f);
    glm::vec3 pos = (v0 + v1 + v2) * oneThird;
    glm::vec3 out = pos + norm * glm::vec3(k);
    glBegin(GL_LINES); // todo: ogles
    {
        glVertex3fv(glm::value_ptr(pos));
        glVertex3fv(glm::value_ptr(out));
    }
    glEnd();
}

void gl9ActiveTexture( GLenum texture ) { ::glActiveTexture(texture); }
void gl9BindBuffer(GLenum target, GLuint buffer) { ::glBindBuffer ( target,  buffer); }
void gl9BindTexture( GLenum target, GLuint texture ) {
    if(texture == 0) glDisable(GL_TEXTURE_2D); else glEnable(GL_TEXTURE_2D);
    ::glBindTexture( target,  texture );
}
void gl9BlendFunc( GLenum sfactor, GLenum dfactor ) { ::glBlendFunc(  sfactor,  dfactor ); }
void gl9BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { ::glBufferData( target,  size,  data, usage); }
void gl9BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { ::glBufferSubData( target,  offset,  size, data); }
void gl9ColorPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) { ::glColorPointer(  size,  type,  stride, ptr ); }
void gl9Color3fv(const GLfloat *f) { ::glColor3fv(f); }
void gl9Color4fv(const GLfloat *f) { ::glColor4fv(f); }
void gl9ClearColor3fv(const GLfloat* rgb) { ::glClearColor(rgb[0],rgb[1],rgb[2],0); };
void gl9DeleteBuffers(GLsizei n, const GLuint *buffers) { ::glDeleteBuffers(n, buffers); }
void gl9DepthRange( GLclampf near_val, GLclampf far_val ) { ::glDepthRangef(  near_val,  far_val ); }
void gl9Disable( GLenum cap ) { ::glDisable( cap ); }
void gl9DisableClientState( GLenum cap ) { ::glDisableClientState(  cap ); }
void gl9DrawArrays( GLenum mode, GLint first, GLsizei count ) { ::glDrawArrays(  mode,  first,  count ); }
void gl9DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) { ::glDrawElements(  mode,  count,  type,  indices ); }
void gl9DrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels ) { ::glDrawPixels(  width,  height,  format,  type,  pixels ); }
void gl9Enable( GLenum cap ) { ::glEnable( cap ); }
void gl9EnableClientState( GLenum cap ) { ::glEnableClientState(  cap ); }
void gl9Flush( void ) { ::glFlush(); }
void gl9GenBuffers(GLsizei n, GLuint *buffers) { ::glGenBuffers(n, buffers); }
void gl9GenTextures( GLsizei n, GLuint *textures ) { ::glGenTextures(  n, textures ); }

void gl9MatrixMode( GLenum mode ) { ::glMatrixMode( mode ); }
void gl9LoadIdentity( void ) { ::glLoadIdentity( ); }
void gl9LoadMatrixf( const GLfloat *m ) { ::glLoadMatrixf(m); }
void gl9MultMatrixf(const GLfloat *f) { ::glMultMatrixf(f); }
void gl9PopMatrix( void ) { ::glPopMatrix(); }
void gl9PushMatrix( void ) { ::glPushMatrix(); }

void gl9PopAttrib() { ::glPopAttrib(); }
void gl9LoadLightf(const GLfloat *f) { }
void gl9PopClientAttrib( void ) { ::glPopClientAttrib(); }
void gl9PushAttrib( GLbitfield mask ) { ::glPushAttrib( mask ); }
void gl9PushClientAttrib( GLbitfield mask ) { ::glPushClientAttrib(  mask ); }
void gl9ReadColor3fv( GLint x, GLint y, GLfloat *f )
{
    const float f256 = float( 256 );
    uint8_t barr[4] = {0,0,0,0}; // 1 extra
    ::glReadPixels( x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, barr ); // todo: check linux
    f[0] = float(barr[0]) / f256;
    f[1] = float(barr[1]) / f256;
    f[2] = float(barr[2]) / f256;
}
void gl9TexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) { ::glTexCoordPointer(  size,  type,  stride,  ptr ); }
void gl9TexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) { ::glTexImage2D(  target,  level,  internalFormat,  width,  height,  border,  format,  type,  pixels ); }
void gl9TexParameteri( GLenum target, GLenum pname, GLint param ) { ::glTexParameteri(  target,  pname,  param ); }
void gl9VertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) { ::glVertexPointer(  size,  type,  stride,  ptr ); }
void gl9Viewport( GLint x, GLint y, GLsizei width, GLsizei height ) { ::glViewport(  x,  y,  width,  height ); }
