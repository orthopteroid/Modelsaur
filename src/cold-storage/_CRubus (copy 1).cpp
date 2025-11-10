// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <functional>

#include "GL9.hpp"

#include "TriTools.hpp"
#include "CRubus.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// because of some ide issue...
inline float glLength(glm::vec3 vec)
{
    return std::sqrt( glm::dot(vec, vec) );
}

//#define ENABLE_COLLISION_FALLBACK
//#define DEBUG_COLLISION_TRACKING
#define CHATTY
//#define CHATTY_CRAWL

CRubus::CRubus()
{
    dimension = 32;
}

void CRubus::Bind(IDefineTri* p)
{
    pTriagonalnomial = p;

    std::multimap<binID_type, glm::vec3> binVecs; // use copies here to incl center tri point
    std::set<binID_type> binNames;
    std::set<bintri_type> uniqueBinTris;
    std::list<glm::vec3> centerPoints;

    // ask only for triangles from the renderable
    for(triID_type triID = TriIDBegin; triID != TriIDEnd; pTriagonalnomial->TriIterNext( triID ))
    {
        tri_type triDef = pTriagonalnomial->TriVertPos( triID );

        centerPoints.push_front( (triDef.v0 + triDef.v1 + triDef.v2) * glm::vec3( 1.f / 3.f ) );
        glm::vec3 & center = centerPoints.front();

        binID_type b0 = BinMake( triDef.v0, dimension);
        binID_type b1 = BinMake( triDef.v1, dimension);
        binID_type b2 = BinMake( triDef.v2, dimension);
        binID_type bc = BinMake( center, dimension);

        binVecs.insert( std::make_pair(b0, triDef.v0) );
        binVecs.insert( std::make_pair(b1, triDef.v1) );
        binVecs.insert( std::make_pair(b2, triDef.v2) );
        binVecs.insert( std::make_pair(bc, center) );

        binNames.insert( b0 );
        binNames.insert( b1 );
        binNames.insert( b2 );
        binNames.insert( bc );

        uniqueBinTris.insert( bintri_type(b0, triID) );
        uniqueBinTris.insert( bintri_type(b1, triID) );
        uniqueBinTris.insert( bintri_type(b2, triID) );
        uniqueBinTris.insert( bintri_type(bc, triID) );
    }

    // for each bin, calc center then determine radius
    for( auto iterBin = binNames.begin(); iterBin != binNames.end(); ++iterBin )
    {
        sph_markable sph;
        sph.radius = 0.f;
        sph.center = glm::vec3(0);
        sph.serial = 0;

        Inflate(sph, *iterBin, binVecs);

        binSpheres.insert(std::make_pair(*iterBin,sph));
    }

    for( auto iter = uniqueBinTris.begin(); iter != uniqueBinTris.end(); ++iter )
    {
        triID_markable markableTriID = { iter->id.tri, 0 };
        binTris.insert(std::make_pair(iter->id.bin, markableTriID));
    }

#ifdef DEBUG
    printf("rubus %zu spheres\n", binSpheres.size());
    printf("rubus %zu uniqueBinTris\n", uniqueBinTris.size());
#endif // DEBUG
}

void CRubus::Release()
{
    binSpheres.clear();
    binTris.clear();
    pTriagonalnomial = 0;
}

void CRubus::Reset()
{
    binSpheres.clear();
    binTris.clear();

    /////////

    std::multimap<binID_type, glm::vec3> binVecs; // use copies here to incl center tri point
    std::set<binID_type> binNames;
    std::set<bintri_type> uniqueBinTris;
    std::list<glm::vec3> centerPoints;

    // ask only for triangles from the renderable
    for(triID_type triID = TriIDBegin; triID != TriIDEnd; pTriagonalnomial->TriIterNext( triID ))
    {
        tri_type triDef = pTriagonalnomial->TriVertPos( triID );

        centerPoints.push_front( (triDef.v0 + triDef.v1 + triDef.v2) * glm::vec3( 1.f / 3.f ) );
        glm::vec3 & center = centerPoints.front();

        binID_type b0 = BinMake( triDef.v0, dimension);
        binID_type b1 = BinMake( triDef.v1, dimension);
        binID_type b2 = BinMake( triDef.v2, dimension);
        binID_type bc = BinMake( center, dimension);

        binVecs.insert( std::make_pair(b0, triDef.v0) );
        binVecs.insert( std::make_pair(b1, triDef.v1) );
        binVecs.insert( std::make_pair(b2, triDef.v2) );
        binVecs.insert( std::make_pair(bc, center) );

        binNames.insert( b0 );
        binNames.insert( b1 );
        binNames.insert( b2 );
        binNames.insert( bc );

        uniqueBinTris.insert( bintri_type(b0, triID) );
        uniqueBinTris.insert( bintri_type(b1, triID) );
        uniqueBinTris.insert( bintri_type(b2, triID) );
        uniqueBinTris.insert( bintri_type(bc, triID) );
    }

    // for each bin, calc center then determine radius
    for( auto iterBin = binNames.begin(); iterBin != binNames.end(); ++iterBin )
    {
        sph_markable sph;
        sph.radius = 0.f;
        sph.center = glm::vec3(0);
        sph.serial = 0;

        Inflate(sph, *iterBin, binVecs);

        binSpheres.insert(std::make_pair(*iterBin,sph));
    }

    for( auto iter = uniqueBinTris.begin(); iter != uniqueBinTris.end(); ++iter )
    {
        triID_markable markableTriID = { iter->id.tri, 0 };
        binTris.insert(std::make_pair(iter->id.bin, markableTriID));
    }

    AppLog::Info(__FILENAME__, "rubus %lu spheres\n", binSpheres.size());
    AppLog::Info(__FILENAME__, "rubus %lu uniqueBinTris\n", uniqueBinTris.size());
}

void CRubus::Inflate(sph_markable& sph, binID_type bin, std::multimap<binID_type, glm::vec3> const & binVecs)
{
    for( auto iterVec = binVecs.lower_bound(bin); iterVec != binVecs.upper_bound(bin); ++iterVec )
    {
        if( iterVec == binVecs.lower_bound(bin) )
            sph.center = iterVec->second;
        else
            sph.center = ( iterVec->second + sph.center ) / 2.f;
    }

    for( auto iterVec = binVecs.lower_bound(bin); iterVec != binVecs.upper_bound(bin); ++iterVec )
    {
        if( iterVec == binVecs.lower_bound(bin) )
        {
            // default radius is half the partition length of an n-partitioned circle-circumference
            sph.radius = 2.f * (float)M_PI * glLength(iterVec->second) / dimension / 2.f;

            // todo: incr in size should be dep on tri size?
            sph.radius *= 2.f; // hack doubles size to assist overlap
        }
        else
        {
            sph.radius = std::max( sph.radius, glLength(iterVec->second - sph.center) );
        }
    }
}

void CRubus::Inflate(triID_type triID, tri_type const & triDef)
{
    std::multimap<binID_type, glm::vec3> binVecs; // use copies here to incl center tri point
    std::set<binID_type> binNames;
    std::set<bintri_type> uniqueBinTris;

    std::list<glm::vec3> centerPoints;
    centerPoints.push_front( (triDef.v0 + triDef.v1 + triDef.v2) * glm::vec3( 1.f / 3.f ) );
    glm::vec3 & center = centerPoints.front();

    binID_type b0 = BinMake( triDef.v0, dimension);
    binID_type b1 = BinMake( triDef.v1, dimension);
    binID_type b2 = BinMake( triDef.v2, dimension);
    binID_type bc = BinMake( center, dimension);

    binVecs.insert( std::make_pair(b0, triDef.v0) );
    binVecs.insert( std::make_pair(b1, triDef.v1) );
    binVecs.insert( std::make_pair(b2, triDef.v2) );
    binVecs.insert( std::make_pair(bc, center) );

    binNames.insert( b0 );
    binNames.insert( b1 );
    binNames.insert( b2 );
    binNames.insert( bc );

    uniqueBinTris.insert( bintri_type(b0, triID) );
    uniqueBinTris.insert( bintri_type(b1, triID) );
    uniqueBinTris.insert( bintri_type(b2, triID) );
    uniqueBinTris.insert( bintri_type(bc, triID) );

    // todo: clean old tri from bin?

    // for each of the vertex-bins...
    for( auto iterBin = binNames.begin(); iterBin != binNames.end(); ++iterBin )
    {
        // get the bin-sphere...
        auto iterSph = binSpheres.find(*iterBin);
        if( iterSph == binSpheres.end() )
        {
            sph_markable sph;
            sph.radius = 0.f;
            sph.center = glm::vec3(0);
            sph.serial = 0;
            auto rv = binSpheres.insert(std::make_pair(*iterBin,sph));
            iterSph = rv.first;
        }

        // adjust the sphere so it holds all the verticies in it
        Inflate(iterSph->second, *iterBin, binVecs); // todo: check shrinking sphere should not exclude verts that are in the bin!

        binSpheres.insert(std::make_pair(*iterBin,iterSph->second));
    }

    for( auto iter = uniqueBinTris.begin(); iter != uniqueBinTris.end(); ++iter )
    {
        triID_markable markableTriID = { iter->id.tri, 0 };
        binTris.insert(std::make_pair(iter->id.bin, markableTriID));
    }
}

void CRubus::IdentifyTri(triID_type &tri_out, glm::vec3 const &position_, glm::vec3 const &direction_)
{
    position = position_;
    direction = direction_;

    char stat;
    uint tris = 0, sphs = 0, sphsc = 0;

    serial++;

    auto fnCheckTri = [&] (triID_markable triID) -> bool
    {
        if( triID.serial == serial ) return false;
        triID.serial = serial;
        tris++;

        // requires glm-0.9.7.6
        tri_type triDef = pTriagonalnomial->TriVertPos( triID.triID );
        glm::vec3 triangleBarycentricCoord;
        if( !glm::intersectRayTriangle<glm::vec3>(
                position, direction, triDef.v0, triDef.v1, triDef.v2, triangleBarycentricCoord
        ) ) return false;

        collisionTri = triID.triID;
        return true;
    };

    auto fnCheckBin = [&] (binID_type bin) -> bool
    {
        // mark bin/sphere
        auto iterSph = binSpheres.find(bin);
        if( iterSph == binSpheres.end() ) return false;
        if( iterSph->second.serial == serial ) return false;
        iterSph->second.serial = serial;
        sphs++;

        // check the tris in this bin
        for( auto iter = binTris.lower_bound(bin); iter != binTris.upper_bound(bin); ++iter )
        {
            if( fnCheckTri( iter->second ) ) { collisionBin = bin; return true; }
        }
        return false;
    };

    auto fnCheckSph = [&] (binID_type bin) -> bool
    {
        // mark bin/sphere
        auto iterSph = binSpheres.find(bin);
        if( iterSph == binSpheres.end() ) return false;
        if( iterSph->second.serial == serial ) return false;
        iterSph->second.serial = serial;
        sphs++;

        if( iterSph->second.radius <= std::numeric_limits<float>::epsilon() ) return false;

        // skip non-relevant spheres
        // if the ray intersects the sphere, check the specified bin to see which triangle the ray hits
        float circleIntersectionDistance;
        if( !glm::intersectRaySphere<glm::vec3>(
                position, direction, iterSph->second.center, iterSph->second.radius * iterSph->second.radius, circleIntersectionDistance
        ) ) return false;
        sphsc++; // how many in collision with?

        // check the tris in this bin
        for( auto iter = binTris.lower_bound(bin); iter != binTris.upper_bound(bin); ++iter )
        {
            if( fnCheckTri( iter->second ) ) { collisionBin = bin; return true; }
        }
        return false;
    };

    stat = 'A';
    if( collisionTri != TriIDEnd && fnCheckTri( { collisionTri, 0 } ) ) goto hit; // nb: suboptimal stack object
    collisionTri = TriIDEnd;

    stat = 'B';
    if( collisionBin != BinIDEnd && fnCheckBin( collisionBin ) ) goto hit;
    collisionBin = BinIDEnd;

    // check the last valid bin
    stat = 'C';
    if( lastValidBin != BinIDEnd && fnCheckBin( lastValidBin ) ) goto hit;

    // check nearby bins
    stat = 'D';
    for(int8_t d=1;d<dimension/4;d++)
    {
        // major axis sides
        if( fnCheckBin( BinAdjust( lastValidBin, -d, 0, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, +d, 0, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, 0, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, 0, +d, dimension ) ) ) goto hit;

        // corners
        if( fnCheckBin( BinAdjust( lastValidBin, -d, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, +d, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, -d, +d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( lastValidBin, +d, +d, dimension ) ) ) goto hit;

        // others
        for(int8_t e=1;e<(d-1);e++)
        {
            // horizontals
            if( fnCheckBin( BinAdjust( lastValidBin, +e, -d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, -e, -d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, +e, +d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, -e, +d, dimension ) ) ) goto hit;
            // verticals
            if( fnCheckBin( BinAdjust( lastValidBin, -d, +e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, -d, -e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, +d, +e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( lastValidBin, +d, -e, dimension ) ) ) goto hit;
        }
    }

    // sweep the remaining bins
    stat = 'E';
    for(auto iter : binSpheres)
    {
        if( fnCheckSph( iter.first ) ) goto hit;
    }

    // and remaining tris for a bin!
    stat = 'Z';
    for( auto iter : binTris )
    {
        if( !fnCheckTri( iter.second ) ) continue;
        collisionBin = iter.first;
        goto hit;
    }

    stat = '?';

    hit:

    if( collisionBin != BinIDEnd ) lastValidBin = collisionBin;

#ifdef CHATTY_CRAWL
    AppLog::Info(__FILENAME__, "IdentifyTri %d %d %d %c", sphs, sphsc, tris, stat);
    AppLog::Info(__FILENAME__, "IdentifyTri collisionTri %d", collisionTri);
#endif

    tri_out = collisionTri;
}

void CRubus::IdentifyAdjTri(std::set<triadj_type> &triSet_out, float patchSize)
{
    if( collisionTri == TriIDEnd ) return;
    if( collisionBin == BinIDEnd ) return;

    uint tris = 0, sphs = 0, sphsc = 0;

    serial++;

    const glm::vec3 a_third( 1.f / 3.f );
    const tri_type cur_triDef = pTriagonalnomial->TriVertPos( collisionTri );
    const glm::vec3 cur_center = ( cur_triDef.v0 + cur_triDef.v1 + cur_triDef.v2 ) * a_third;

#if true
    // misses some... but is faster

    auto fnCheckNAdd = [&] (binID_type bin) -> bool
    {
        auto iterSph = binSpheres.find(bin);
        if( iterSph == binSpheres.end() ) return false;
        if( iterSph->second.serial == serial ) return false;
        iterSph->second.serial = serial;
        sphs++;

        bool outOfRange = true;

        // check each tri in the bin
        for( auto iter = binTris.lower_bound(bin); iter != binTris.upper_bound(bin); ++iter )
        {
            if( iter->second.serial == serial ) continue;
            iter->second.serial = serial;
            tris++;

            tri_type triDef = pTriagonalnomial->TriVertPos( iter->second.triID );
            glm::vec3 center = (triDef.v0 + triDef.v1 + triDef.v2) * a_third;

            // calc angle between vectors, then length of arc
            glm::vec3 cross = glm::cross(center, cur_center);
            float theta = std::asin( glLength(cross) / (glLength(center)*glLength(cur_center)) );
            float avgR = (glLength(center) + glLength(cur_center)) / 2.f;
            float dist = avgR * theta / float( 2 * M_PI ); // circumf identity: L = 2 pi r

            if( dist <= patchSize ) {
                triSet_out.insert( { iter->second.triID, dist } );
                outOfRange = false;
            }
        }
        return outOfRange;
    };

    // add current
    fnCheckNAdd(collisionBin);

    // enumerate all adjacent tris within range
    // todo: only with normals pointing in
    bool outOfRange = false;
    for(int8_t d=1;!outOfRange && d<dimension/4;d++)
    {
        // major axis sides
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -d, 0, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +d, 0, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, 0, -d, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, 0, +d, dimension ) ) ;

        // corners
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -d, -d, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +d, -d, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -d, +d, dimension ) ) ;
        outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +d, +d, dimension ) ) ;

        // others
        for(int8_t e=1;e<(d-1);e++)
        {
            // horizontals
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +e, -d, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -e, -d, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +e, +d, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -e, +d, dimension ) ) ;
            // verticals
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -d, +e, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, -d, -e, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +d, +e, dimension ) ) ;
            outOfRange |= fnCheckNAdd( BinAdjust( collisionBin, +d, -e, dimension ) ) ;
        }
    }

#else

    std::vector<ind3_type> indTriVerts = pTriagonalnomial->GetIndTris();
    std::vector<glm::vec3> posVerts = pTriagonalnomial->GetPosVerts();

    // brute-force search
    for( auto iterSph = binSpheres.begin(); iterSph != binSpheres.end(); ++iterSph )
    {
        if( iterSph->second.serial == serial ) continue;
        iterSph->second.serial = serial;
        sphs++;

        // skip bins too far away
        auto bin = iterSph->first;
        if( BinLinearDist( uint8_t(uint(bin) >> 8), uint8_t(uint(collisionBin) >> 8), dimension ) > dimension / 4 ) continue;
        if( BinLinearDist( uint8_t(uint(bin) & 0xFF), uint8_t(uint(collisionBin) & 0xFF), dimension ) > dimension / 4 ) continue;
        sphsc++;

        for( auto iterTri = binTris.lower_bound( bin ); iterTri != binTris.upper_bound( bin ); ++iterTri )
        {
            if( iterTri->second.serial == serial ) continue;
            iterTri->second.serial = serial;
            tris++;

            const ind3_type indTri = indTriVerts[ iterTri->second.triID ];
            const glm::vec3 center = ( posVerts[ indTri.x ] + posVerts[ indTri.y ] + posVerts[ indTri.z ] ) * a_third;

            // calc angle between vectors, then length of arc
            const glm::vec3 cross = glm::cross( center, cur_center );
            const float theta = std::asin( glLength( cross ) / ( glLength( center ) * glLength( cur_center )));
            const float avgR = ( glLength( center ) + glLength( cur_center )) / 2.f;
            const float dist = avgR * theta / float( 2 * M_PI ); // circumf identity: L = 2 pi r

            if( dist <= patchSize ) triSet_out.insert( {iterTri->second.triID, dist} );
        }
    }

#endif

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "IdentifyAdjTri %d %d %d", sphs, sphsc, tris);
#endif
}

void CRubus::AdjTriScan(float const patchSize, serial_type serial)
{
    if( collisionTri == TriIDEnd ) return;
    if( collisionBin == BinIDEnd ) return;

    uint incl = 0, tris = 0, sphs = 0, sphsc = 0;

    if( patchSize < std::numeric_limits<float>::min())
    {
        // failfast path
        triadjDeq.push_back( { collisionTri, 0 } );
    }
    else
    {
        serial++; // for enumeration

        std::vector<ind3_type>& indTris = pTriagonalnomial->GetIndTris();
        std::vector<ind3_type>& indTriAdjTris = pTriagonalnomial->GetIndTriAdjTris();
        std::vector<glm::vec3>& posVerts = pTriagonalnomial->GetPosVerts();

        const glm::vec3 a_third( 1.f / 3.f );

        const ind3_type indTri = indTris[ collisionTri ];
        const glm::vec3 cur_center = ( posVerts[ indTri.x ] + posVerts[ indTri.y ] + posVerts[ indTri.z ] ) * a_third;

        // mark all tris within patch distance with new serial number
        for( auto iterSph = binSpheres.begin(); iterSph != binSpheres.end(); ++iterSph )
        {
//            if( iterSph->second.serial == serial ) continue;
//            iterSph->second.serial = serial;
            sphs++;

            // skip bins too far away
            auto bin = iterSph->first;
            if( BinLinearDist( uint8_t(uint(bin) >> 8), uint8_t(uint(collisionBin) >> 8), dimension ) > dimension / 4 ) continue;
            if( BinLinearDist( uint8_t(uint(bin) & 0xFF), uint8_t(uint(collisionBin) & 0xFF), dimension ) > dimension / 4 ) continue;
            sphsc++;

            for( auto iterTri = binTris.lower_bound( bin ); iterTri != binTris.upper_bound( bin ); ++iterTri )
            {
//                if( iterTri->second.serial == serial ) continue;
//                iterTri->second.serial = serial;
                tris++;

                const ind3_type indTri = indTris[ iterTri->second.triID ];
                const glm::vec3 center = ( posVerts[ indTri.x ] + posVerts[ indTri.y ] + posVerts[ indTri.z ] ) * a_third;

                // calc angle between vectors, then length of arc
                const glm::vec3 cross = glm::cross( center, cur_center );
                const float theta = std::asin( glLength( cross ) / ( glLength( center ) * glLength( cur_center )));
                const float avgR = ( glLength( center ) + glLength( cur_center )) / 2.f;
                const float dist = avgR * theta / float( 2 * M_PI ); // circumf identity: L = 2 pi r

                if( dist <= patchSize )
                {
                    triadjDeq.push_back( { iterTri->second.triID, dist } );
//                    triadj.push_back( { iterTri->second.triID, dist, triAdjParseContext.serial } );
                    incl++;
                }
            }
        }
    }

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "AdjTriScan %d %d %d %d", sphs, sphsc, tris, incl);
#endif
}
