#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

#include <unistd.h>
#include <math.h>

#include "GL9.hpp"

#include "TriTools.hpp"
#include "RHelix.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define RndColour ((float)rand() / (float)RAND_MAX)

//////////////////

// because of some ide issue...
inline float glLength(glm::vec3 vec)
{
    return sqrt( glm::dot(vec, vec) );
}

RHelix::RHelix()
{
    Reset();
}

void RHelix::Reset()
{
    posVerts.clear();
    normVerts.clear();
    colorVerts.clear();
    normTris.clear();

    float c = 4.f;
    float steps = 40;
    float dh = .020f;
    float h = 0.f;
    //uint t = 0;
    for(float r=0.f; r<2.f*M_PI*c; r+=2.f*M_PI/steps)
    {
        posVerts.push_back( { cos(r), h + .25*dh*steps - .5*c*dh*steps, sin(r) } );
        posVerts.push_back( { cos(r), h - .25*dh*steps - .5*c*dh*steps, sin(r) } );
        h += dh;
    }

    const glm::vec3 zero( 0.f );
    normVerts.assign( posVerts.size(), zero );
    colorVerts.assign( posVerts.size(), zero );
    normTris.assign( posVerts.size() -2, zero ); // for a strip, 2 fewer tris than verts

    std::generate(colorVerts.begin(), colorVerts.end(),
                  [] () -> glm::vec3 { return glm::vec3(RndColour, RndColour, RndColour); }
    );

#ifdef DEBUG
    printf("helix %zu verts\n", posVerts.size());
#endif // DEBUG
}
void RHelix::Bind()
{
    rubus.Bind(this);

    gl9GenBuffers( 1, &boPos );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferData( GL_ARRAY_BUFFER, posVerts.size() * sizeof(glm::vec3), posVerts.data(), GL_STATIC_DRAW );

    gl9GenBuffers( 1, &boColor );
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
    gl9BufferData( GL_ARRAY_BUFFER, colorVerts.size() * sizeof(glm::vec3), colorVerts.data(), GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}
void RHelix::Release()
{
    if(boPos) { gl9DeleteBuffers( 1, &boPos ); }
    if(boColor) { gl9DeleteBuffers( 1, &boColor ); }

    rubus.Release();
}
void RHelix::Render()
{
    gl9MatrixMode( GL_MODELVIEW );
    gl9PushMatrix();
    gl9PushAttrib( GL_POLYGON_MODE );
    gl9PushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS ); // saves pixel storage modes and vertex array state
    {
        gl9EnableClientState( GL_VERTEX_ARRAY );
        gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPos );
        gl9VertexPointer( 3, GL_FLOAT, 0, NULL );

        gl9EnableClientState( GL_COLOR_ARRAY );
        gl9BufferUnbinder objectColor( GL_ARRAY_BUFFER, boColor );
        gl9ColorPointer( 3, GL_FLOAT, 0, NULL );

        gl9DrawArrays( GL_TRIANGLE_STRIP, 0, (GLsizei)posVerts.size());
    }
    gl9PopClientAttrib();
    gl9PopAttrib();
    gl9PopMatrix();

    gl9DisableClientState(GL_VERTEX_ARRAY);
    gl9DisableClientState(GL_COLOR_ARRAY);
}
void RHelix::RenderCollisionBody(glm::vec3 axisIn, glm::vec3 axisUp, glm::vec3 color)
{
    gl9MatrixMode( GL_MODELVIEW );
    gl9PushMatrix();
    gl9PushAttrib( GL_POLYGON_MODE );
    {
        gl9Color3fv(glm::value_ptr(color));
        for( auto iter = rubus.binSpheres.begin(); iter != rubus.binSpheres.end(); ++iter )
        {
            gl9Circle( axisIn, axisUp, iter->second.center, iter->second.radius, 10 );
        }
    }
    gl9PopAttrib();
    gl9PopMatrix();
}
void RHelix::RenderNormals()
{
    gl9MatrixMode( GL_MODELVIEW );
    gl9PushMatrix();
    gl9PushAttrib( GL_POLYGON_MODE );
    {
        for(triID_type triID = TriIDBegin; triID != TriIDEnd; TriIterNext( triID ))
        {
            auto indTri = TriVertInd(triID);
            gl9Color3fv(glm::value_ptr(colorVerts[triID] * .5f));
            gl9Normal(
                posVerts[ indTri.x ], posVerts[ indTri.y ], posVerts[ indTri.z ],
                normTris[triID], .5f
            );
        }
    }
    gl9PopAttrib();
    gl9PopMatrix();
}
void RHelix::BrushPos(triID_type triID, glm::vec3 const &normDeform, float const & k)
{
    posVerts[triID+0] += normDeform * k;
    posVerts[triID+1] += normDeform * k;
    posVerts[triID+2] += normDeform * k;
    UpdatePos(triID);
    rubus.Inflate(triID, posVerts[triID+0], posVerts[triID+1], posVerts[triID+2]);
}
void RHelix::UpdatePos(triID_type triID)
{
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferSubData( GL_ARRAY_BUFFER, triID * sizeof( glm::vec3 ), 3 * sizeof( glm::vec3 ), &posVerts[triID] );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}
void RHelix::BrushColor(triID_type triID, glm::vec3 const & color, float const blend)
{
    colorVerts[triID] = blend * color + (1.f - blend) * colorVerts[triID];
    UpdateColor(triID);
}
void RHelix::UpdateColor(triID_type triID)
{
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
    gl9BufferSubData( GL_ARRAY_BUFFER, triID * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &colorVerts[triID] );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}
void RHelix::UpdateAllStates()
{
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferSubData( GL_ARRAY_BUFFER, 0, posVerts.size() * sizeof( glm::vec3 ), posVerts.data() );
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
    gl9BufferSubData( GL_ARRAY_BUFFER, 0, colorVerts.size() * sizeof( glm::vec3 ), colorVerts.data() );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}