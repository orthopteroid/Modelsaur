// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cmath>
#include <algorithm>
#include <iostream>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "RColorPicker.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// because of some ide issue...
inline float glLength(const glm::vec3& vec)
{
    return sqrt( glm::dot(vec, vec) );
}

void RColorPicker::Bind(int w, int h, int z, GLuint tx)
{
    txImage = tx;
    platWidth = w;
    platHeight = h;

    posTexVerts = { {0,0}, {0,1}, {1,1}, {1,0}, }; // CW (?)
    gl9GenBuffers( 1, &boTexVerts );
    gl9BindBuffer( GL_ARRAY_BUFFER, boTexVerts );
    gl9BufferData( GL_ARRAY_BUFFER, posTexVerts.size() * sizeof(glm::vec2), posTexVerts.data(), GL_STATIC_DRAW );

    posVerts = { { -.5, -.5, z }, { -.5, +.5, z }, { +.5, +.5, z }, { +.5, -.5, z }, }; // center @ 0,0,z
    gl9GenBuffers( 1, &boVerts );
    gl9BindBuffer( GL_ARRAY_BUFFER, boVerts );
    gl9BufferData( GL_ARRAY_BUFFER, posVerts.size() * sizeof(glm::vec3), posVerts.data(), GL_STATIC_DRAW );

    const auto mxIdent =  glm::mat4(1);

    auto smallestDimension = int(std::min(w,h));
    circleDiameter = smallestDimension * 4 / 5;

    translate = {w/2,h/2,0};
    mxPlacement = glm::scale( glm::translate( mxIdent, {w/2,h/2,0} ), { circleDiameter,-circleDiameter,1 } ); // -1 to flip vertical
}

void RColorPicker::Release()
{
    if(txImage) { gl9DeleteBuffers(1, &txImage); txImage=0; }
    gl9DeleteBuffers(1, &boTexVerts);
    gl9DeleteBuffers(1, &boVerts);
}

bool RColorPicker::PickColor(glm::vec3 pos)
{
    float dist = glLength( { translate.x - pos.x, translate.y - pos.y, 0 } );
    if(dist > circleDiameter/2 ) return false;

    ::gl9ReadColor3fv( GLint(pos.x), platHeight - GLint(pos.y), &color[0] );
    return true;
}

void RColorPicker::Render()
{
    gl9BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl9Enable( GL_BLEND );

    // for all images, reuse the texture and vert coordinates
    gl9ClientStateDisabler texCordArrState( GL_TEXTURE_COORD_ARRAY );
    gl9BufferUnbinder texVerts( GL_ARRAY_BUFFER, boTexVerts );
    gl9TexCoordPointer( 2, GL_FLOAT, 0, nullptr );

    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boVerts );
    gl9VertexPointer( 3, GL_FLOAT, 0, nullptr );

    gl9LoadMatrixf( glm::value_ptr( mxPlacement ));
    gl9TextureUnbinder texObject( GL_TEXTURE_2D, txImage );

    // first triangle (bottom left - top left - top right)
    // second triangle (bottom left - top right - bottom right)
    const GLubyte indices[] = {0,1,2, 0,2,3};
    gl9DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);

    glDisable( GL_BLEND );
}
