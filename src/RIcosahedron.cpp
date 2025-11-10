// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "RIcosahedron.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define RndColour ((float)rand() / (float)RAND_MAX)

// https://en.wikipedia.org/wiki/Vertex_Buffer_Object#In_C.2C_using_OpenGL_2.1

static glm::vec3 scale(.05f);//.001f);

// http://www.opengl.org.ru/docs/pg/0208.html
#define X .525731112119133606
#define Z .850650808352039932

static GLfloat data[12][3] = {
    {-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z}, {X, 0.0, -Z},
    {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
    {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}
};
static GLubyte tindices[20][3] = {
    {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
    {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
    {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
    {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11} };

RIcosahedron::RIcosahedron() : color(RndColour,RndColour,RndColour)
{
    colorSwitcher.interval = 240;
}
void RIcosahedron::Bind()
{
    gl9GenBuffers( 1, &boPoints );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPoints );
    gl9BufferData( GL_ARRAY_BUFFER, sizeof( data ), data, GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    gl9GenBuffers( 1, &boIndicies );
    gl9BindBuffer( GL_ELEMENT_ARRAY_BUFFER, boIndicies );
    gl9BufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( tindices ), tindices, GL_STATIC_DRAW );
    gl9BindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}
void RIcosahedron::Release()
{
    if( boPoints ) { gl9DeleteBuffers(1, &boPoints); }
    if( boIndicies ) { gl9DeleteBuffers(1, &boIndicies); }
}
void RIcosahedron::Tick(uint32_t delta)
{
    colorSwitcher.Tick(delta);
    if(colorSwitcher.triggered)
        color = glm::vec3(RndColour,RndColour,RndColour);
}
void RIcosahedron::Render()
{
    glm::mat4 foo =
        glm::translate( glm::vec3( position.x, position.y, position.z ) ) *
        glm::scale( glm::vec3(scale.x, scale.y, scale.z) );
    gl9MultMatrixf(glm::value_ptr(foo));

    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPoints );
    gl9VertexPointer( 3, GL_FLOAT, 0, nullptr );

    gl9BufferUnbinder objectElements( GL_ELEMENT_ARRAY_BUFFER, boIndicies );
    gl9DrawElements( GL_TRIANGLES, sizeof( tindices ) / sizeof( GLubyte ), GL_UNSIGNED_BYTE, nullptr );
}
