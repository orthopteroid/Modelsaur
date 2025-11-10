// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <iostream>
#include <math.h>
#include <cmath>

#include "GL9.hpp"

#include "TriTools.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// because of some ide issue...
inline float glLength(const glm::vec3& vec)
{
    return std::sqrt( glm::dot(vec, vec) );
}

// https://math.stackexchange.com/a/1105038
inline float atan2_(float y, float x)
{
    float a = std::min( std::fabs(x), std::fabs(y) ) / std::max( std::fabs(x), std::fabs(y) );
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    if( std::fabs(y) > std::fabs(x) ) r = 1.57079637 - r;
    if( x < 0 ) r = 3.14159274 - r;
    if(y < 0.f) r = -r;
    return r;
}

// https://web.archive.org/web/20160713122015/http://http.developer.nvidia.com:80/Cg/atan2.html
//float atan2_(float y, float x)
//{
//    float t0, t1, t2, t3, t4;
//
//    t3 = abs(x);
//    t1 = abs(y);
//    t0 = max(t3, t1);
//    t1 = min(t3, t1);
//    t3 = float(1) / t0;
//    t3 = t1 * t3;
//
//    t4 = t3 * t3;
//    t0 =         - float(0.013480470);
//    t0 = t0 * t4 + float(0.057477314);
//    t0 = t0 * t4 - float(0.121239071);
//    t0 = t0 * t4 + float(0.195635925);
//    t0 = t0 * t4 - float(0.332994597);
//    t0 = t0 * t4 + float(0.999995630);
//    t3 = t0 * t3;
//
//    t3 = (abs(y) > abs(x)) ? float(1.570796327) - t3 : t3;
//    t3 = (x < 0) ?  float(3.141592654) - t3 : t3;
//    t3 = (y < 0) ? -t3 : t3;
//
//    return t3;
//}

// https://web.archive.org/web/20161223122122/http://http.developer.nvidia.com:80/Cg/acos.html
float acos_(float x) {
    float negate = float(x < 0);
    x = abs(x);
    float ret = -0.0187293;
    ret = ret * x;
    ret = ret + 0.0742610;
    ret = ret * x;
    ret = ret - 0.2121144;
    ret = ret * x;
    ret = ret + 1.5707288;
    ret = ret * sqrt(1.0-x);
    ret = ret - 2 * negate * ret;
    return negate * 3.14159265358979 + ret;
}

//#define CHATTY

binID_type BinMake(glm::vec3 const & pos, uint8_t dim)
{
    const float pi = float(M_PI);
    const float two_pi = float(2*M_PI);
    float r = glm::length(pos);
    // https://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
    // and normalized to [0,1]
    float lon = (atan2_(pos.y, pos.x) + pi) / two_pi; // was std::atan2
    float lat = (acos_(pos.z / r) + pi) / two_pi; // was std::acos
    float fx = lon * float(dim);
    float fy = lat * float(dim);
    auto x = uint8_t(fx);
    auto y = uint8_t(fy);
    binID_type bin = x << 8 | y;
    return bin;
}

void TriVertNormals(
        std::vector<glm::vec3>& normVerts,
        std::vector<glm::vec3>& normTris,
        const std::vector<ind3_type>& indTriVerts,
        const std::vector<glm::vec3>& posVerts
)
{
    // calc normals for triangles and verticies
    for(uint i=0; i<indTriVerts.size(); i++)
    {
        uint p0=indTriVerts[i].x, p1=indTriVerts[i].y, p2=indTriVerts[i].z;
        glm::vec3 norm = glm::triangleNormal( posVerts[ p0 ], posVerts[ p1 ], posVerts[ p2 ] );
        normVerts[ p0 ] += norm; // borked
        normVerts[ p1 ] += norm;
        normVerts[ p2 ] += norm;
        normTris[i] = norm;
    }
    for(uint i=0; i<normVerts.size(); i++ )
    {
        const glm::vec3 third( 1.f / 3.f );
        normVerts[i] *= third; // normalize
    }
}

// build adjacent-tri list
void IndTriAdjTris(std::vector<ind3_type>& indTriAdjTris, const std::vector<ind3_type>& indTriVerts)
{
    struct edge_type
    {
        union
        {
            uint32_t key; // assumes vertID_type is uint16_t
            struct
            {
                vertID_type a;
                vertID_type b;
            } id;
        };
        edge_type(vertID_type a_, vertID_type b_)
        {
            if(a_<b_) { id.a = a_; id.b = b_; } else { id.a = b_; id.b = a_; }
        }
        bool operator<(const edge_type& other) const { return this->key < other.key; }
    };

    std::multimap<edge_type, triID_type> etmap;
    for (triID_type t = 0; t < indTriVerts.size(); t++)
    {
        etmap.insert(std::make_pair(edge_type(indTriVerts[t].x, indTriVerts[t].y), t));
        etmap.insert(std::make_pair(edge_type(indTriVerts[t].x, indTriVerts[t].z), t));
        etmap.insert(std::make_pair(edge_type(indTriVerts[t].z, indTriVerts[t].y), t));
    }
    std::multimap<triID_type, triID_type> ttmap; // two tris per edge
    auto edgeIter=etmap.begin();
    while( edgeIter!=etmap.end() )
    {
        auto e = edgeIter->first;
        auto t0 = edgeIter->second;
        edgeIter++;
        if(edgeIter == etmap.end()) continue;
        if(edgeIter->first.key == e.key)
        {
            auto t1 = edgeIter->second;
            ttmap.insert(std::make_pair(t0, t1));
            ttmap.insert(std::make_pair(t1, t0));
            edgeIter++;
            if(edgeIter == etmap.end()) continue;

            // except at ends!
            //assert(edgeIter->first != e); // 3 tris can't share an edge
        }
    }
    // compress the ttmap
    indTriAdjTris.resize( indTriVerts.size() );
    for (triID_type t = 0; t < indTriVerts.size(); t++)
    {
        int i=0;
        indTriAdjTris[t] = {TriIDEnd, TriIDEnd, TriIDEnd};
        for (auto iterTT = ttmap.lower_bound(t); iterTT != ttmap.upper_bound(t); iterTT++)
        {
            assert(i<3);
            switch(i++)
            {
                case 0: indTriAdjTris[t].x = iterTT->second; break;
                case 1: indTriAdjTris[t].y = iterTT->second; break;
                case 2: indTriAdjTris[t].z = iterTT->second; break;
                default: ;
            }
        }
    }
}

void AdjTriVisitor(
    std::deque<trieffect_type>& deq_out,
    triID_type triStart,
    const std::vector<ind3_type>& indTriAdjTris,
    const serial_type serial,
    std::vector<serial_type>& serialMarkings,
    EffectorFn& fnTriEffector
)
{
    std::deque<triID_type> triWorkingSet;

    triWorkingSet.push_back( triStart );
    while( !triWorkingSet.empty() )
    {
        auto fnQueueForCheck = [&](triID_type tID)
        {
            if(tID == TriIDEnd) return;
            if(serialMarkings[ tID ] != serial)
            {
                serialMarkings[ tID ] = serial;
                triWorkingSet.push_back( tID );
            }
        };

        auto triID = triWorkingSet.front();
        triWorkingSet.pop_front();

        auto calc = fnTriEffector( triID );
        if( calc.first )
        {
            deq_out.push_back( { triID, calc.second } );

            fnQueueForCheck( indTriAdjTris[ triID ].x );
            fnQueueForCheck( indTriAdjTris[ triID ].y );
            fnQueueForCheck( indTriAdjTris[ triID ].z );
        }
    }
}
