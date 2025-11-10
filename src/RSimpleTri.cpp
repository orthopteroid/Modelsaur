// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "RSimpleTri.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// https://en.wikipedia.org/wiki/Vertex_Buffer_Object#In_C.2C_using_OpenGL_2.1

#if false

// CW equliteral triangle in XY plane
GLubyte data[3][3] = { {0, 0, 0}, {127, 255, 0}, {255, 0, 0} }; // BL at 0,0
GLubyte offset[2] = { 127, 127 };

RSimpleTri::RSimpleTri() { }
void RSimpleTri::Bind(int platMinSq)
{
    gl9GenBuffers( 1, &boPoints );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPoints );
    gl9BufferData( GL_ARRAY_BUFFER, sizeof( data ), data, GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    float f = 256 * 25;
    scale = { float(platMinSq) / f, float(platMinSq) / -f, 1}; // nb: -f for inv y
}
void RSimpleTri::Update(glm::vec3 pos, bool vis)
{
    position = pos;
    visible = vis;
}

void RSimpleTri::Release()
{
    if( boPoints ) { gl9DeleteBuffers(1, &boPoints); }
}
void RSimpleTri::Render()
{
    const glm::vec3 vOffset( -offset[0], -offset[1], 0 );
    glm::mat4 foo = glm::translate( position ) * glm::scale( scale ) * glm::translate( vOffset );
    gl9MultMatrixf(glm::value_ptr(foo));

    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPoints );
    gl9VertexPointer( 3, GL_UNSIGNED_BYTE, 0, nullptr );

    gl9DrawArrays(GL_TRIANGLE_STRIP, 0, 3);
}

#else

// CW equliteral triangle in XY plane
GLfloat data[3][3] = { {0, 0, 0}, {.5f, -1, 0}, {-.5f, -1, 0} }; // top at 0,0
float offset[2] = { 0, .6f };

RSimpleTri::RSimpleTri() { }
void RSimpleTri::Bind(int platMinSq)
{
    gl9GenBuffers( 1, &boPoints );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPoints );
    gl9BufferData( GL_ARRAY_BUFFER, sizeof( data ), data, GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    float f = 1 * 25;
    scale = { float(platMinSq) / f, float(platMinSq) / -f, 1}; // nb: -f for inv y
}
void RSimpleTri::Update(glm::vec3 pos, bool vis)
{
    position = pos;
    visible = vis;
}

void RSimpleTri::Release()
{
    if( boPoints ) { gl9DeleteBuffers(1, &boPoints); }
}
void RSimpleTri::Render()
{
    const glm::vec3 vOffset( offset[0], offset[1], 0 );
    glm::mat4 foo = glm::translate( position ) * glm::scale( scale ) * glm::translate( vOffset );
    gl9MultMatrixf(glm::value_ptr(foo));

    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPoints );
    gl9VertexPointer( 3, GL_FLOAT, 0, nullptr );

    gl9DrawArrays(GL_TRIANGLES, 0, 3);
}

#endif