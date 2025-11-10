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
inline float glLength(const glm::vec3& vec)
{
    return std::sqrt( glm::dot(vec, vec) );
}

//#define ENABLE_COLLISION_FALLBACK
//#define CHATTY

// whats hacky about this is we assume bin and tri t_id types are each half of uint...
struct bintri_type
{
    union
    {
        binID_triID_key key;
        struct
        {
            binID_type bin;
            triID_type tri;
        } id;
    };
    bintri_type(binID_type b, triID_type t) { id.bin = b; id.tri = t; }
    inline bool operator<(const bintri_type& other) const { return this->key < other.key; }
};

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

    std::vector<ind3_type>& indTri = pTriagonalnomial->GetIndTris();

    const float a_third( 1.f / 3.f );
    glm::vec3 v0, v1, v2;

    // ask only for triangles from the renderable
    for(triID_type triID = 0; triID < indTri.size(); triID++)
    {
        pTriagonalnomial->GetTriVerts(v0, v1, v2, triID);

        centerPoints.push_front( (v0 + v1 + v2) * a_third );
        glm::vec3 & center = centerPoints.front();

        binID_type b0 = BinMake( v0, dimension);
        binID_type b1 = BinMake( v1, dimension);
        binID_type b2 = BinMake( v2, dimension);
        binID_type bc = BinMake( center, dimension);

        binVecs.insert( std::make_pair(b0, v0) );
        binVecs.insert( std::make_pair(b1, v1) );
        binVecs.insert( std::make_pair(b2, v2) );
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
        binTris.insert(std::make_pair(iter->id.bin, triID_markable( iter->id.tri ) ));
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

    std::vector<ind3_type>& indTri = pTriagonalnomial->GetIndTris();

    glm::vec3 v0, v1, v2;

    // ask only for triangles from the renderable
    for(triID_type triID = 0; triID < indTri.size(); triID++)
    {
        pTriagonalnomial->GetTriVerts(v0, v1, v2, triID);

        centerPoints.push_front( (v0 + v1 + v2) * glm::vec3( 1.f / 3.f ) );
        glm::vec3 & center = centerPoints.front();

        binID_type b0 = BinMake( v0, dimension);
        binID_type b1 = BinMake( v1, dimension);
        binID_type b2 = BinMake( v2, dimension);
        binID_type bc = BinMake( center, dimension);

        binVecs.insert( std::make_pair(b0, v0) );
        binVecs.insert( std::make_pair(b1, v1) );
        binVecs.insert( std::make_pair(b2, v2) );
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
        binTris.insert(std::make_pair( iter->id.bin, triID_markable(iter->id.tri ) ));
    }

    AppLog::Info(__FILENAME__, "rubus %lu spheres\n", binSpheres.size());
    AppLog::Info(__FILENAME__, "rubus %lu uniqueBinTris\n", uniqueBinTris.size());
}

// todo: 13jul profiled at 53/.7/40k
void CRubus::Inflate(sph_markable& sph, binID_type bin, std::multimap<binID_type, glm::vec3> const & binVecs)
{
    using const_BinVecIter = std::multimap<binID_type, glm::vec3>::const_iterator;

    for( const_BinVecIter iterVec = binVecs.lower_bound(bin); iterVec != binVecs.upper_bound(bin); ++iterVec )
    {
        if( iterVec == binVecs.lower_bound(bin) )
            sph.center = iterVec->second;
        else
            sph.center = ( iterVec->second + sph.center ) / 2.f;
    }

    for( const_BinVecIter iterVec = binVecs.lower_bound(bin); iterVec != binVecs.upper_bound(bin); ++iterVec )
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

// todo: 13jul profiled at 21/.8/86
void CRubus::Inflate(triID_type triID, const glm::vec3& v0, const glm::vec3 & v1, const glm::vec3& v2)
{
    std::list<glm::vec3> centerPoints;
    centerPoints.push_front( (v0 + v1 + v2) * glm::vec3( 1.f / 3.f ) );
    glm::vec3 & center = centerPoints.front();

    binID_type b0 = BinMake( v0, dimension);
    binID_type b1 = BinMake( v1, dimension);
    binID_type b2 = BinMake( v2, dimension);
    binID_type bc = BinMake( center, dimension);

    std::multimap<binID_type, glm::vec3> binVecs; // use copies here to incl center tri point
    binVecs.insert( std::make_pair(b0, v0) );
    binVecs.insert( std::make_pair(b1, v1) );
    binVecs.insert( std::make_pair(b2, v2) );
    binVecs.insert( std::make_pair(bc, center) );

    std::set<binID_type> binNames;
    binNames.insert( b0 );
    binNames.insert( b1 );
    binNames.insert( b2 );
    binNames.insert( bc );

    // todo: clean old tri from bin? difficult, easier to flush on reset...

    // for each of the vertex-bins...
    using const_BinNameIter = std::set<binID_type>::const_iterator;
    for( const_BinNameIter iterBin = binNames.begin(); iterBin != binNames.end(); ++iterBin )
    {
        // get the bin-sphere...
        auto iterSph = binSpheres.find(*iterBin); // todo: log time complexity
        if( iterSph == binSpheres.end() )
        {
            sph_markable sph;
            sph.radius = 0.f;
            sph.center.x = sph.center.y = sph.center.z = 0.f;
            sph.serial = 0;
            auto rv = binSpheres.insert( std::make_pair( *iterBin , sph ) );
            iterSph = rv.first;
        }

        // adjust the sphere so it holds all the verticies in it
        Inflate(iterSph->second, *iterBin, binVecs); // todo: check shrinking sphere should not exclude verts that are in the bin!

        binSpheres.insert( std::make_pair( *iterBin, iterSph->second ) ); // todo: review. retained? appears not.
    }

    // fold the bins relevant to the specified triangle into a set to reduce duplicates
    std::set<bintri_type> uniqueBinTris;
    uniqueBinTris.insert( bintri_type(b0, triID) );
    uniqueBinTris.insert( bintri_type(b1, triID) );
    uniqueBinTris.insert( bintri_type(b2, triID) );
    uniqueBinTris.insert( bintri_type(bc, triID) );

    // iterate through set of unique bins...
    using const_BinTriIter = std::set<bintri_type>::const_iterator;
    for( const_BinTriIter setIter = uniqueBinTris.begin(); setIter != uniqueBinTris.end(); ++setIter )
    {
        // ... for each of the triangle's bins, search through the bin multimap to ensure the triangle exists there
        bool foundInBin = false;
        for( auto binIter = binTris.lower_bound(setIter->id.bin); binIter != binTris.upper_bound(setIter->id.bin); ++binIter )
        {
            if( binIter->second.triID == triID )
            {
                foundInBin = true;
                break;
            }
        }

        if(!foundInBin)
            binTris.insert( std::make_pair( setIter->id.bin, triID_markable( triID ) ) ); // retained. must be flushed on reset.
    }
}

void CRubus::IdentifyTri(trisearch_type& cxt_out, glm::vec3 const &position, glm::vec3 const &direction)
{
    char stat;
    uint tris = 0, sphs = 0, sphsc = 0;

    serial++;

    auto fnCheckTri = [&] (triID_markable markableTri) -> bool
    {
        if( markableTri.triID == TriIDEnd ) return false;
        if( markableTri.serial == serial ) return false;
        markableTri.serial = serial;
        tris++;

        glm::vec3 v0, v1, v2;
        pTriagonalnomial->GetTriVerts(v0, v1, v2, markableTri.triID);

        // requires glm-0.9.7.6
        glm::vec3 triangleBarycentricCoord;
        if( !glm::intersectRayTriangle<glm::vec3>(
                position, direction, v0, v1, v2, triangleBarycentricCoord
        ) ) return false;

        cxt_out.collisionTri = markableTri.triID;
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
            if( fnCheckTri( iter->second ) )
            {
                cxt_out.collisionBin = bin;
                return true;
            }
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
            if( fnCheckTri( iter->second ) )
            {
                cxt_out.collisionBin = bin;
                return true;
            }
        }
        return false;
    };

    stat = 'A';
    if( cxt_out.collisionTri != TriIDEnd && fnCheckTri( triID_markable( cxt_out.collisionTri ) ) ) goto hit; // nb: suboptimal stack object
    cxt_out.collisionTri = TriIDEnd;

    stat = 'B';
    if( cxt_out.collisionBin != BinIDEnd && fnCheckBin( cxt_out.collisionBin ) ) goto hit;
    cxt_out.collisionBin = BinIDEnd;

    // check the last valid bin
    stat = 'C';
    if( cxt_out.lastValidBin != BinIDEnd && fnCheckBin( cxt_out.lastValidBin ) ) goto hit;

    // check nearby bins
    stat = 'D';
    for(int8_t d=1;d<dimension/4;d++)
    {
        // major axis sides
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -d, 0, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +d, 0, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, 0, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, 0, +d, dimension ) ) ) goto hit;

        // corners
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -d, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +d, -d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -d, +d, dimension ) ) ) goto hit;
        if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +d, +d, dimension ) ) ) goto hit;

        // others
        for(int8_t e=1;e<(d-1);e++)
        {
            // horizontals
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +e, -d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -e, -d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +e, +d, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -e, +d, dimension ) ) ) goto hit;
            // verticals
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -d, +e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, -d, -e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +d, +e, dimension ) ) ) goto hit;
            if( fnCheckBin( BinAdjust( cxt_out.lastValidBin, +d, -e, dimension ) ) ) goto hit;
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
        cxt_out.collisionBin = iter.first;
        goto hit;
    }

    stat = '?';

    hit:

    if( cxt_out.collisionBin != BinIDEnd ) cxt_out.lastValidBin = cxt_out.collisionBin;

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "IdentifyTri %d %d %d %c", sphs, sphsc, tris, stat);
    AppLog::Info(__FILENAME__, "IdentifyTri collisionTri %d", cxt_out.collisionTri);
#endif // CHATTY
}
