#ifndef _GL9_HPP_
#define _GL9_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#ifdef __ANDROID_API__

#define GL_POLYGON_MODE 1

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#else // __ANDROID_API__

#include <GL/gl.h>
#include <GL/glext.h>

#endif // __ANDROID_API__

//#define GLM_FORCE_PURE
//#define GLM_FORCE_MESSAGES
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <functional>
#include <vector>

void gl9Bind(int32_t &w, int32_t &h);
void gl9Release();
void gl9BeginFrame();
void gl9EndFrame();

#define GL9_WORLD   1
#define GL9_MENU    2
#define GL9_POINTER 3
void gl9UseProgram(GLint p);

void gl9ActiveTexture( GLenum texture );
void gl9BindBuffer (GLenum target, GLuint buffer);
void gl9BindTexture( GLenum target, GLuint texture );
void gl9BlendFunc( GLenum sfactor, GLenum dfactor );
void gl9BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void gl9BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
void gl9Circle(glm::vec3 const & axisIn, glm::vec3 const & axisUp, glm::vec3 const & posCenter, float radius, uint steps);
void gl9ColorPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
void gl9Color3fv(const GLfloat *f);
void gl9Color4fv(const GLfloat *f);
void gl9ClearColor3fv(const GLfloat* rgb);
void gl9DeleteBuffers(GLsizei n, const GLuint *buffers);
void gl9DepthRange( GLclampf near_val, GLclampf far_val ); // was GLclampd on lappy
void gl9Disable( GLenum cap );
void gl9DisableClientState( GLenum cap );
void gl9DrawArrays( GLenum mode, GLint first, GLsizei count );
void gl9DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
void gl9DrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );
void gl9Enable( GLenum cap );
void gl9EnableClientState( GLenum cap );
void gl9Flush( void );
void gl9GenBuffers(GLsizei n, GLuint *buffers);
void gl9GenTextures( GLsizei n, GLuint *textures );
void gl9LoadIdentity( void );
void gl9LoadMatrixf( const GLfloat *m );
void gl9LoadLightf(const GLfloat *f);
void gl9MatrixMode( GLenum mode );
void gl9MultMatrixf( const GLfloat *f );
void gl9Normal( const glm::vec3 & v0, const glm::vec3 & v1, const glm::vec3 & v2, glm::vec3 const & norm, float k);
void gl9PopAttrib();
void gl9PopClientAttrib( void );
void gl9PopMatrix( void );
void gl9PushAttrib( GLbitfield mask );
void gl9PushClientAttrib( GLbitfield mask );
void gl9PushMatrix( void );
void gl9ReadColor3fv( GLint x, GLint y, GLfloat *f );
void gl9TexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
void gl9TexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
void gl9TexParameteri( GLenum target, GLenum pname, GLint param );
void gl9VertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr );
void gl9Viewport( GLint x, GLint y, GLsizei width, GLsizei height );

void gl9RenderPNG(const char *szFilename, int w, int h, std::function<void(void)> fnRender);
void gl9RenderGIF(const char *szFilename, int w, int h, std::function<void(void)> fnRender, const std::vector<glm::vec3> &colorVerts, int nf, float fps );

struct gl9ClientStateDisabler
{
    uint cs;
    gl9ClientStateDisabler(uint cs_) : cs(cs_) { gl9EnableClientState(cs); }
    ~gl9ClientStateDisabler() { gl9DisableClientState(cs); }
};

struct gl9BufferUnbinder
{
    uint target, id;
    gl9BufferUnbinder(uint t, uint b) : target(t), id(b) { gl9BindBuffer(target, id); }
    ~gl9BufferUnbinder() { gl9BindBuffer(target, 0); }
};

struct gl9TextureUnbinder
{
    uint target, id;
    gl9TextureUnbinder(uint t, uint b) : target(t), id(b) { gl9BindTexture(target, id); }
    ~gl9TextureUnbinder() { gl9BindTexture(target, 0); }
};

#endif // _GL9_HPP_
