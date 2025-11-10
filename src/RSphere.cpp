// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <csignal>

#include <unistd.h>
#include <math.h>

#ifdef __ANDROID_API__

#else // __ANDROID_API__

#include <signal.h>

#endif // __ANDROID_API__

#include "GL9.hpp"

#include "TriTools.hpp"
#include "AppLog.hpp"
#include "AppFile.hpp"

#include "RSphere.hpp"

#define HIREZ

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define STRINGIFY_WORKER(sym) #sym
#define STRINGIFY(sym) STRINGIFY_WORKER(sym)

#define RndColour ((float)rand() / (float)RAND_MAX)

//#define SUBDATA_UPDATE_MODE 0 // instant
//#define SUBDATA_UPDATE_MODE 1 // extents
#define SUBDATA_UPDATE_MODE 2 // coalesced chicklets

//#define CHECK_SUBDATA

// https://stackoverflow.com/a/23782939
constexpr unsigned floorlog2(unsigned x) { return x == 1 ? 0 : 1+floorlog2(x >> 1); }
constexpr unsigned ceillog2(unsigned x) { return x == 1 ? 0 : floorlog2(x - 1) + 1; }

// because of some ide issue...
inline float glLength(const glm::vec3& vec)
{
    return sqrt( glm::dot(vec, vec) );
}

//////////////////

// how many tris per 360'
#if defined(HIREZ)
const uint DivisionSize = 60;
#else
const uint DivisionSize = 20;
#endif

// make 4 chicklets per 360'
const uint16_t ChickletBitSize = std::max<uint16_t>( 1, ceillog2( DivisionSize >> 2 ) );

template<class T>
void RSphere::BufferSubData_Chicklet(
    const GLenum& target,
    const std::vector<T>& vec,
    const std::set<uint16_t>& markings,
    const uint16_t& chickletBitSize
)
{
    if(markings.size() == 0) return;

    const auto cItems = 1 << chickletBitSize;
    const auto cMask = cItems -1;
    const auto iBytes = sizeof(T);
    const auto cBytes = cItems * iBytes;
    const auto vBytes = iBytes * vec.size();
    for( const auto& i : markings )
    {
        const auto bStart = (i & ~cMask) * iBytes; // chicklets are item aligned
        const auto bCount = std::min<int>( vBytes, bStart + cBytes ) - bStart; // clip
#ifdef DEBUG
        assert( bCount <= vBytes );
#endif // DEBUG
#if defined(CHECK_SUBDATA)
        AppLog::Info( __FILENAME__, "%s %4d %4d    %8X", __func__, bStart, bCount, bStart + (uint8_t*)vec.data() );
#endif // CHECK_SUBDATA

        gl9BufferSubData( target, bStart, bCount, bStart + (uint8_t*)vec.data() );

        bytesUpdated += bCount;
    }
}

bool RSphere::CheatSphereOnly = true;

uint RSphere::GetDivisions() const
{
    return DivisionSize;
}

void RSphere::Backup()
{
    posVerts_backup = posVerts;
    colorVerts_backup = colorVerts;
}

void RSphere::Reset(bool fromBackup)
{
    posVerts.clear();
    indTriVerts.clear();
    normVerts.clear();
    colorVerts.clear();
    normTris.clear();

    const uint divisions = GetDivisions();

    if(fromBackup)
    {
        posVerts = posVerts_backup;
    }
    else
    {
        int m0 = CheatSphereOnly ? 0 : rand() % 5;

        const int N = 2;

        int k[N];
        for(int i=0; i<N; i++) k[i] = 1 + rand() % 5;

        float c[N];
        for(int i=0; i<N; i++) c[i] = float( k[i] ) * .2f;

        // build the points from a 3d spiral created by moving
        // a point around rotation and inclination axis.
        glm::vec3 axisInclination( 0, 1, 0 );
        glm::vec3 axisRotation( 1, 0, 0 );

        float angleRotation = float( M_PI ) / float( divisions );
        glm::quat quatRotation = glm::angleAxis( angleRotation, axisRotation );
        while( true )
        {
            float angleInclination = float( M_PI ) * ( float( posVerts.size() / float( divisions * divisions ) ) );

            // stop when we're pointing the other way
            if( std::abs( angleInclination - float( M_PI ) / 2 ) < .01f ) break; // hack: /2

            float x;
            switch(m0)
            {
                case 1: x = std::abs( std::pow( angleInclination, 2 * c[0] ) - c[1] ); break;
                case 2: x = 2 * std::pow( angleInclination, 2 * c[0] ); break;
                case 3: x = std::pow( angleInclination - c[0] / 2, - c[1] / 2 ); break;
                default: x = 1;
            }

            const glm::vec3 unitVector( x, 0, 0 );
            glm::quat quatNet =
                glm::angleAxis( angleInclination, glm::normalize( glm::cross( axisRotation, axisInclination )));
            glm::vec3 vert = quatNet * unitVector * glm::conjugate( quatNet );
            posVerts.push_back( vert );

            axisRotation = glm::normalize( quatRotation * axisRotation * glm::conjugate( quatRotation ));
            axisInclination = glm::normalize( quatRotation * axisInclination * glm::conjugate( quatRotation ));
        }
    }

    // build body from triangles: endcaps made from fans, connected by a strip
    uint firstPoint = 0;
    uint secondPoint = 1;
    uint lastPoint = posVerts.size()-1;
    for(uint i=secondPoint;i<lastPoint;i++)
    {
        // 3 parts: trifans over one rotation at endcaps and quads in middle
        if(i < lastPoint - divisions) {
            if(i <= divisions) {
                // back-connect to firstPoint for first rotation
                indTriVerts.push_back( ind3_type( i, i + 1, firstPoint ));
            }
            // forward-connect one rotation
            indTriVerts.push_back( ind3_type( i, i + divisions, i + 1 ));
            indTriVerts.push_back( ind3_type( i + divisions, i + divisions + 1, i + 1 ));
        } else {
            // forward-connect to lastPoint for last rotation
            indTriVerts.push_back( ind3_type( i, lastPoint, i+1 ));
        }
    }

    // verify indicies
    for(uint i=0; i<indTriVerts.size(); i++)
    {
        uint p0 = indTriVerts[i].x, p1 = indTriVerts[i].y, p2 = indTriVerts[i].z;
        if( !( p0 < posVerts.size() && p1 < posVerts.size() && p2 < posVerts.size()))
        {
            printf("tri ind err %u: %u %u %u\n", i, p0,p1,p2);
            raise(SIGTRAP); //assert(false);
        }
    }

    IndTriAdjTris( indTriAdjTris, indTriVerts );

    normEffectVerts.resize( posVerts.size() );
    normVerts.resize( posVerts.size() );
    normTris.resize( indTriVerts.size() );
    TriVertNormals( normVerts, normTris, indTriVerts, posVerts );

    if(fromBackup)
    {
        colorVerts = colorVerts_backup;
    }
    else
    {
        colorVerts.resize( posVerts.size() );
        std::generate( colorVerts.begin(), colorVerts.end(),
                       []() -> glm::vec3 { return glm::vec3(RndColour, RndColour, RndColour); }
        );
    }

#ifdef DEBUG
    printf("sphere %zu verts\n", posVerts.size());
    printf("sphere %zu tris\n", indTriVerts.size());
#endif // DEBUG
}

void RSphere::Bind()
{
    if(posVerts.size() == 0)
    {
        Reset();
        Backup();
    }

    rubus.Bind(this);

    gl9GenBuffers( 1, &boPos );
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferData( GL_ARRAY_BUFFER, posVerts.size() * sizeof(glm::vec3), posVerts.data(), GL_STATIC_DRAW );

    gl9GenBuffers( 1, &boIndicies );
    gl9BindBuffer( GL_ELEMENT_ARRAY_BUFFER, boIndicies );
    gl9BufferData( GL_ELEMENT_ARRAY_BUFFER, indTriVerts.size() * sizeof(ind3_type), indTriVerts.data(), GL_STATIC_DRAW );
    gl9BindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

    gl9GenBuffers( 1, &boNormals );
    gl9BindBuffer( GL_ARRAY_BUFFER, boNormals );
    gl9BufferData( GL_ARRAY_BUFFER, normVerts.size() * sizeof(glm::vec3), normVerts.data(), GL_STATIC_DRAW );

    gl9GenBuffers( 1, &boColor );
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
    gl9BufferData( GL_ARRAY_BUFFER, colorVerts.size() * sizeof(glm::vec3), colorVerts.data(), GL_STATIC_DRAW );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}

void RSphere::Release()
{
    if(boPos) { gl9DeleteBuffers( 1, &boPos ); }
    if(boIndicies) { gl9DeleteBuffers( 1, &boIndicies ); }
    if(boNormals) { gl9DeleteBuffers( 1, &boNormals ); }
    if(boColor) { gl9DeleteBuffers( 1, &boColor ); }

    rubus.Release();
}

void RSphere::Render()
{
    gl9ClientStateDisabler objectVertArrState( GL_VERTEX_ARRAY );
    gl9BufferUnbinder objectVerts( GL_ARRAY_BUFFER, boPos );
    gl9VertexPointer( 3, GL_FLOAT, 0, nullptr );

#if !defined(OGL1)
    gl9ClientStateDisabler objNormalArrState( GL_NORMAL_ARRAY ); // lighting
    gl9BufferUnbinder objectNormals(GL_ARRAY_BUFFER, boNormals);
    gl9VertexPointer( 3, GL_FLOAT, 0, NULL );
#endif // OGL1

    gl9ClientStateDisabler objColorArrState( GL_COLOR_ARRAY );
    gl9BufferUnbinder objectColor(GL_ARRAY_BUFFER, boColor);
    gl9ColorPointer( 3, GL_FLOAT, 0, NULL );

    gl9BufferUnbinder objectTriIndicies(GL_ELEMENT_ARRAY_BUFFER, boIndicies);
    gl9DrawElements( GL_TRIANGLES, (GLsizei)indTriVerts.size() * 3, ind3_type::base_typeid, nullptr );
}
void RSphere::RenderCollisionBody(glm::vec3 axisIn, glm::vec3 axisUp, glm::vec3 color)
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
void RSphere::RenderNormals()
{
    gl9MatrixMode( GL_MODELVIEW );
    gl9PushMatrix();
    gl9PushAttrib( GL_POLYGON_MODE );
    {
        const glm::vec3 color = {1.f,.5f,.5f};
        gl9Color3fv(glm::value_ptr(color));
        for(triID_type triID = 0; triID < normTris.size(); triID++)
        {
            gl9Color3fv(glm::value_ptr(colorVerts[triID] * .5f));
            auto indTri = TriVertInd(triID);
            gl9Normal(
                posVerts[ indTri.x ], posVerts[ indTri.y ], posVerts[ indTri.z ],
                normTris[triID], .5f
            );
        }
    }
    gl9PopAttrib();
    gl9PopMatrix();
}

void RSphere::BrushPos(triID_type triID, glm::vec3 const &normDeform, float const & k)
{
    ind3_type tri = indTriVerts[ triID ];
    posVerts[ tri.x ] += normDeform * k;
    posVerts[ tri.y ] += normDeform * k;
    posVerts[ tri.z ] += normDeform * k;

    // hack to fix endcaps
    const auto last = posVerts.size() -1;
    if(tri.x == 0 || tri.y == 0 || tri.z == 0)
    {
        glm::vec3 sum;
        for(uint i=1; i<DivisionSize; i++) sum += posVerts[ i ];
        posVerts[0] = sum / float(DivisionSize);
    }
    else if(tri.x == last || tri.y == last || tri.z == last)
    {
        glm::vec3 sum;
        for(uint i=1; i<DivisionSize; i++) sum += posVerts[ last -i ];
        posVerts[last] = sum / float(DivisionSize);
    }

    UpdatePos(triID);
    rubus.Inflate(triID, posVerts[ tri.x ], posVerts[ tri.y ], posVerts[ tri.z ]);
}
void RSphere::UpdatePos(triID_type triID)
{
    ind3_type tri = indTriVerts[ triID ];

#if (SUBDATA_UPDATE_MODE==0)
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.x * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &posVerts[tri.x] );
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.y * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &posVerts[tri.y] );
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.z * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &posVerts[tri.z] );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
    bytesUpdated += sizeof(glm::vec3) * 3;
#else
    // async update
    const uint16_t chickletMask = (1 << ChickletBitSize) -1;
    triUpdateSet.insert( tri.x & ~chickletMask );
    triUpdateSet.insert( tri.y & ~chickletMask );
    triUpdateSet.insert( tri.z & ~chickletMask );
#endif
}
void RSphere::UpdatePosTick()
{
    if(triUpdateSet.size() == 0) return;

    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );

#if (SUBDATA_UPDATE_MODE==0)
    assert( false ); // already updated
#elif (SUBDATA_UPDATE_MODE==1)
    auto s = *triUpdateSet.begin();
    auto e = *triUpdateSet.rbegin();
    gl9BufferSubData( GL_ARRAY_BUFFER, sizeof(glm::vec3) * s, sizeof(glm::vec3) * (e -s +1), &posVerts[s] );
    bytesUpdated += sizeof(glm::vec3) * (e -s +1);
#elif (SUBDATA_UPDATE_MODE==2)
    BufferSubData_Chicklet( GL_ARRAY_BUFFER, posVerts, triUpdateSet, ChickletBitSize );
#endif

    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    triUpdateSet.clear();
}
void RSphere::UpdatePosFinalize()
{

}

void RSphere::BrushZ(triID_type triID, float const &k)
{
    ind3_type tri = indTriVerts[ triID ];
    normEffectVerts[ tri.x ] = std::max( normEffectVerts[ tri.x ], k );
    normEffectVerts[ tri.y ] = std::max( normEffectVerts[ tri.y ], k );
    normEffectVerts[ tri.z ] = std::max( normEffectVerts[ tri.z ], k );
    posVerts[ tri.x ] = posVerts_backup[ tri.x ] + normTris[triID] * normEffectVerts[ tri.x ];
    posVerts[ tri.y ] = posVerts_backup[ tri.y ] + normTris[triID] * normEffectVerts[ tri.y ];
    posVerts[ tri.z ] = posVerts_backup[ tri.z ] + normTris[triID] * normEffectVerts[ tri.z ];
    UpdatePos(triID);
    rubus.Inflate(triID, posVerts[ tri.x ], posVerts[ tri.y ], posVerts[ tri.z ]);
}
void RSphere::UpdateNormalFinalize()
{

}

void RSphere::BrushColor(triID_type triID, glm::vec3 const & color, float const blend)
{
    ind3_type tri = indTriVerts[ triID ];

#if defined(CHECK_SUBDATA)
    AppLog::Info( __FILENAME__, "%s  tri %4d  triID %4X", __func__, tri, triID );
#endif // CHECK_SUBDATA

#if !defined(OGL1) // OGL1 tri color set from 3rd vert only
    colorVerts[ tri.x ] = blend * color + (1.f - blend) * colorVerts[ tri.x ];
    colorVerts[ tri.y ] = blend * color + (1.f - blend) * colorVerts[ tri.y ];
#endif // OGL1
    colorVerts[ tri.z ] = blend * color + (1.f - blend) * colorVerts[ tri.z ];
    UpdateColor(triID);
}
void RSphere::UpdateColor(triID_type triID)
{
    ind3_type tri = indTriVerts[ triID ];

#if (SUBDATA_UPDATE_MODE==0)
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
#if !defined(OGL1) // OGL1 tri color set from 3rd vert only
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.x * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &colorVerts[tri.x] );
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.y * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &colorVerts[tri.y] );
    bytesUpdated += sizeof(glm::vec3) * 2;
#endif // OGL1
    gl9BufferSubData( GL_ARRAY_BUFFER, tri.z * sizeof( glm::vec3 ), sizeof( glm::vec3 ), &colorVerts[tri.z] );
    bytesUpdated += sizeof(glm::vec3);
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
#else
    // async update
    const uint16_t chickletMask = (1 << ChickletBitSize) -1;
    triUpdateSet.insert( tri.x & ~chickletMask );
    triUpdateSet.insert( tri.y & ~chickletMask );
    triUpdateSet.insert( tri.z & ~chickletMask );
#endif
}
void RSphere::UpdateColorTick()
{
    if(triUpdateSet.size() == 0) return;

    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );

#if (SUBDATA_UPDATE_MODE == 0)
    assert( false ); // already updated
#elif (SUBDATA_UPDATE_MODE == 1)
    auto s = *triUpdateSet.begin();
    auto e = *triUpdateSet.rbegin();
    gl9BufferSubData( GL_ARRAY_BUFFER, sizeof(glm::vec3) * s, sizeof(glm::vec3) * (e -s +1), &colorVerts[s] );
    bytesUpdated += sizeof(glm::vec3) * (e -s +1);
#elif (SUBDATA_UPDATE_MODE == 2)
    BufferSubData_Chicklet( GL_ARRAY_BUFFER, colorVerts, triUpdateSet, ChickletBitSize );
#endif

    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );

    triUpdateSet.clear();
}
void RSphere::UpdateColorFinalize()
{

}


void RSphere::UpdateAllStates()
{
    gl9BindBuffer( GL_ARRAY_BUFFER, boPos );
    gl9BufferSubData( GL_ARRAY_BUFFER, 0, posVerts.size() * sizeof( glm::vec3 ), posVerts.data() );
    gl9BindBuffer( GL_ARRAY_BUFFER, boColor );
    gl9BufferSubData( GL_ARRAY_BUFFER, 0, colorVerts.size() * sizeof( glm::vec3 ), colorVerts.data() );
    gl9BindBuffer( GL_ARRAY_BUFFER, 0 );
}

void RSphere::SaveSTL(const char* szFilename)
{
    std::string f = std::string(szFilename) + ".stl";
    AppFile file(f.c_str(), AppFile::Library, AppFile::WriteMode);
    if( file.pFile )
    {
        char buf[80];
        memset( buf, 0, 80 );
        snprintf( buf, 80, "%s %s", szFilename, " Exported by Modelsaur (" STRINGIFY(PLATFORM) ")" );
        fwrite( buf, 80, 1, file.pFile );

        uint32_t nTris = indTriVerts.size();
        fwrite( &nTris, sizeof(uint32_t), 1, file.pFile );

        for( uint i=0; i<indTriVerts.size(); i++ )
        {
            fwrite( &normTris[ i ], sizeof(float), 3, file.pFile );
            fwrite( &posVerts[ indTriVerts[ i ].x ], sizeof(float), 3, file.pFile );
            fwrite( &posVerts[ indTriVerts[ i ].y ], sizeof(float), 3, file.pFile );
            fwrite( &posVerts[ indTriVerts[ i ].z ], sizeof(float), 3, file.pFile );

            uint16_t nAttrib = 0;
            fwrite( &nAttrib, sizeof(uint16_t), 1, file.pFile );
        }
    }
}

void RSphere::SavePLY(const char *szFilename)
{
    std::string f = std::string(szFilename) + ".ply";
    AppFile file(f.c_str(), AppFile::Library, AppFile::WriteMode);
    if( file.pFile )
    {
        file.Printf(
            "ply\n"
                "format ascii 1.0\n"
                "comment Created by Modelsaur (" STRINGIFY(PLATFORM) ")\n"
                "element vertex %u\n"
                "property float x\n"
                "property float y\n"
                "property float z\n"
                "property float nx\n"
                "property float ny\n"
                "property float nz\n"
                "property uchar red\n"
                "property uchar green\n"
                "property uchar blue\n"
                "element face %u\n"
                "property list uchar uint vertex_indices\n"
                "end_header\n",
            posVerts.size(),
            indTriVerts.size()
        );
        for( uint i=0; i<posVerts.size(); i++ ) {
            auto v = posVerts[i];
            auto n = normVerts[i];
            auto c = colorVerts[i];
            file.Printf( "%f %f %f %f %f %f %d %d %d\n", v.x, v.y, v.z, n.x, n.y, n.z, int(255 * c.x), int(255 * c.y), int(255 * c.z) );
        }
        for( auto i : indTriVerts ) file.Printf( "3 %d %d %d\n", i.x, i.y, i.z );
    }
}

