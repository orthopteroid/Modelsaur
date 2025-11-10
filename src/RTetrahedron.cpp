// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

#include <unistd.h>
#include <math.h>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "AppLog.hpp"
#include "RTetrahedron.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define RndColour ((float)rand() / (float)RAND_MAX)

// https://en.wikipedia.org/wiki/Vertex_Buffer_Object#In_C.2C_using_OpenGL_2.1

static glm::vec3 scale(.001f);

// use GL_FRONT_AND_BACK so winding isn't an issue?
static GLfloat points[4][3] = { {0,-1,0}, /* bottom */ {1,1,1}, {0,1,-1}, {-1,1,1} /* triangle in y=1 */ };
static GLubyte indicies[4][3] = { {0,1,2}, {0,2,3}, {0,3,1}, {1,2,3} };

RTetrahedron::RTetrahedron() : color(RndColour,RndColour,RndColour)
{
    colorSwitcher.interval = 240;
}
void RTetrahedron::Bind()
{
    gl9GenBuffers( 1, &boPoints );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPoints );
    gl9BufferData( GL_ARRAY_BUFFER, sizeof( points ), points, GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}
void RTetrahedron::Release()
{
    if( boPoints ) { gl9DeleteBuffers(1, &boPoints); }
}
void RTetrahedron::Tick(long delta)
{
    colorSwitcher.Tick(delta);
    if(colorSwitcher.triggered) color = glm::vec3(RndColour,RndColour,RndColour);
}
void RTetrahedron::Render()
{
    glm::mat4 foo =
        glm::translate( glm::vec3( position.x, position.y, position.z ) ) *
        glm::scale( glm::vec3(scale.x, scale.y, scale.z) );
    gl9MultMatrixf(glm::value_ptr(foo));

    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPoints );
    gl9VertexPointer( 3, GL_FLOAT, 0, nullptr );

    gl9DrawElements( GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, indicies );
}
