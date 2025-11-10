// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cmath>
#include <algorithm>
#include <iostream>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "RMenu.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// because of some ide issue...
inline float glLength(const glm::vec3& vec)
{
    return sqrt( glm::dot(vec, vec) );
}

void RMenu::Bind(uint8_t mt, int platMinSq, float z)
{
    menuToken = mt;

    posTexVerts = { {0,0}, {0,1}, {1,1}, {1,0}, }; // CW (?)
    gl9GenBuffers( 1, &boTexVerts );
    gl9BindBuffer( GL_ARRAY_BUFFER, boTexVerts );
    gl9BufferData( GL_ARRAY_BUFFER, posTexVerts.size() * sizeof(glm::vec2), posTexVerts.data(), GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    posVerts = { { -.5, -.5, z }, { -.5, +.5, z }, { +.5, +.5, z }, { +.5, -.5, z }, }; // center @ 0,0,z
    gl9GenBuffers( 1, &boVerts );
    gl9BindBuffer( GL_ARRAY_BUFFER, boVerts );
    gl9BufferData( GL_ARRAY_BUFFER, posVerts.size() * sizeof(glm::vec3), posVerts.data(), GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    // determine ui range
    int maxExtentX = 0, maxExtentY = 0;
    {
        int cx=-1, cy=0;
        for(item_type& i : menu)
        {
            if(menuToken == i.token) continue; // skip root
            if(i.placement == RMenu::Cat) cx++; else { cx=0; cy++; }
            maxExtentY = std::max( maxExtentY, cy );
            maxExtentX = std::max( maxExtentX, cx );
        }
        maxExtentY++; // for vert padding due to to item being .5 lower
        layoutDim = std::min( float(platMinSq) / float(maxExtentX +1), float(platMinSq) / float(maxExtentY +1) ); // +1, +1 for floor
    }

    const auto mxIdent =  glm::mat4(1);

    // set ui positions
    {
        int cx=-1, cy=0;
        for(item_type& i : menu)
        {
            if(i.placement == RMenu::Cat) cx++; else { cx=0; cy++; }
            ///
            i.translate = { layoutDim * (.5f + float(cx)), (layoutDim / 2) + layoutDim * (.5f + float(cy)), 0 }; // also used as center
            i.mxPlacement = glm::scale( glm::translate( mxIdent, i.translate ), { layoutDim, -layoutDim, 1 } ); // -1 to flip vertical // bug: glm:mat4() is not identity ???
            ///
            // for root, undo position advance
            if(menuToken == i.token) cx--;
        }
    }
}

void RMenu::Release()
{
    for(item_type& i : menu)
    {
        if(i.txImage) { gl9DeleteBuffers(1, &i.txImage); i.txImage=0; }
    }

    gl9DeleteBuffers(1, &boTexVerts);
    gl9DeleteBuffers(1, &boVerts);
}

void RMenu::Render()
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

    // for each image, place it, activate the texture unit and draw
    for(item_type &i : menu)
    {
        if(visible)
        {
            if(menuToken == i.token) continue; // skip root
        }
        else
        {
            if(menuToken != i.token) continue; // skip until we find the root
        }

        gl9LoadMatrixf( glm::value_ptr( i.mxPlacement ));
        gl9TextureUnbinder texObject( GL_TEXTURE_2D, i.txImage );

        // first triangle (bottom left - top left - top right)
        // second triangle (bottom left - top right - bottom right)
        const GLubyte indices[] = {0,1,2, 0,2,3};
        gl9DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);

        if(!visible) break; // only show the root?
    }

    gl9Disable( GL_BLEND );
}

uint8_t* RMenu::Translate(int32_t x, int32_t y)
{
    const float innerCircleRadius = layoutDim /2;
    for(item_type& i : menu)
    {
        if(visible)
        {
            if(menuToken == i.token) continue; // skip root
        }
        else
        {
            if(menuToken != i.token) continue; // skip until we find the root
        }

        float dist = glLength( { i.translate.x - float(x), i.translate.y - float(y), 0 } );
        if(dist < innerCircleRadius)
        {
            //AppLog::Info(__FILENAME__, "selected %c", i.token);
            return &i.token;
        }

        if(!visible) break; // only show the root?
    }
    return nullptr;
}

void RMenu::Tick() {}
