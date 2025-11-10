// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <math.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <ctime>
#include <set>

#include "GL9.hpp"

#include "AppNormalBrusher.hpp"

void AppNormalBrusher::Bind(IRenormalizable* p)
{
    pRenormalizable = p;
}
void AppNormalBrusher::Release()
{
    pRenormalizable = 0;
}

void AppNormalBrusher::Start()
{
    lastTriangle = TriIDEnd;
}

void AppNormalBrusher::Stop()
{
    //deqSegments.clear(); // check. might cause problems if allowed to term early
}

void AppNormalBrusher::Continue(triID_type triID)
{
    deqSegments.push_back( triID );
}

void AppNormalBrusher::Stroke(uint maxiter)
{
    if(deqSegments.size() == 0) return; // fast fail
    if(pRenormalizable->HasDegenerates()) return; // TODO degenerate case

    const glm::vec3 zero( 0.f );
    const glm::vec3 half( 1.f / 2.f );
    const glm::vec3 third( 1.f / 3.f );
    const float halfDeg( float(M_PI) / 360.f );

    std::vector<glm::vec3>& normTris = pRenormalizable->GetNormTris();
    std::vector<glm::vec3>& normVerts = pRenormalizable->GetNormVerts();
    std::vector<glm::vec3> const & posVerts = pRenormalizable->GetPosVerts();

    while(--maxiter)
    {
        if(deqSegments.size() == 0) break;

        triID_type triID = deqSegments.front();
        deqSegments.pop_front();

        /////

        auto vertInd = pRenormalizable->TriVertInd( triID );
        auto triInd = pRenormalizable->TriInd(triID);
        auto triNormal = glm::triangleNormal( posVerts[ vertInd.x ], posVerts[ vertInd.y ], posVerts[ vertInd.z ] );
        const float angle = std::acos( glm::dot( normTris[ triInd ], triNormal ) ); // check

        // if above tolerance, continue with neighbours
        if( !std::isnan(angle) && angle > halfDeg )
        {
            normTris[triInd] = triNormal;

            // rough estimate...
            normVerts[vertInd.x] = (normVerts[vertInd.x] + normTris[triInd]) * half;
            normVerts[vertInd.y] = (normVerts[vertInd.y] + normTris[triInd]) * half;
            normVerts[vertInd.z] = (normVerts[vertInd.z] + normTris[triInd]) * half;

            // check neighbours
            auto others = pRenormalizable->AdjTriInd( triID );
            if( others.x != TriIDEnd ) deqSegments.push_back( others.x );
            if( others.y != TriIDEnd ) deqSegments.push_back( others.y );
            if( others.z != TriIDEnd ) deqSegments.push_back( others.z );
        }
    }
}

void AppNormalBrusher::ReStrokeObject()
{
    deqSegments.clear();

    const glm::vec3 zero( 0.f );
    const glm::vec3 half( 1.f / 2.f );
    const glm::vec3 third( 1.f / 3.f );

    std::vector<glm::vec3>& normTris = pRenormalizable->GetNormTris();
    std::vector<glm::vec3>& normVerts = pRenormalizable->GetNormVerts();
    std::vector<glm::vec3> const & posVerts = pRenormalizable->GetPosVerts();

    // clear first, so we can summate
    normVerts.assign( posVerts.size(), zero );

    if(pRenormalizable->HasDegenerates())
    {
        // summate
        for(triID_type triID = 0; triID < normTris.size(); triID++)
        {
            auto vertInd = pRenormalizable->TriVertInd( triID );

            if(posVerts[vertInd.x] == posVerts[vertInd.y] || posVerts[vertInd.y] == posVerts[vertInd.z])
            {
                // clearing here is only redundant when we took a cache loss due to resizing...
                normVerts[vertInd.x] = normVerts[vertInd.y] = normVerts[vertInd.z] = zero;
                continue; // degenerate
            }

            auto triInd = pRenormalizable->TriInd(triID);
            normTris[ triInd ] = glm::triangleNormal( posVerts[ vertInd.x ], posVerts[ vertInd.y ], posVerts[ vertInd.z ] );

            normVerts[ vertInd.x ] += normTris[ triInd ];
            normVerts[ vertInd.y ] += normTris[ triInd ];
            normVerts[ vertInd.z ] += normTris[ triInd ];
        }

        // normalize
        for(triID_type triID = 0; triID < normTris.size(); triID++)
        {
            auto vertInd = pRenormalizable->TriVertInd( triID );
            if(posVerts[vertInd.x] == posVerts[vertInd.y]) {
                normVerts[ vertInd.x ] *= half;
                normVerts[ vertInd.y ] *= half;
            } else if(posVerts[vertInd.y] == posVerts[vertInd.z]) {
                normVerts[ vertInd.y ] *= half;
                normVerts[ vertInd.z ] *= half;
            } else {
                normVerts[ vertInd.x ] *= third;
                normVerts[ vertInd.y ] *= third;
                normVerts[ vertInd.z ] *= third;
            }
        }
    } else {
        // summate
        for(triID_type triID = 0; triID < normTris.size(); triID++)
        {
            auto vertInd = pRenormalizable->TriVertInd( triID );
            auto triInd = pRenormalizable->TriInd(triID);
            normTris[ triInd ] = glm::triangleNormal( posVerts[ vertInd.x ], posVerts[ vertInd.y ], posVerts[ vertInd.z ] );
            normVerts[ vertInd.x ] += normTris[ triInd ];
            normVerts[ vertInd.y ] += normTris[ triInd ];
            normVerts[ vertInd.z ] += normTris[ triInd ];
        }

        // normalize
        for(triID_type triID = 0; triID < normTris.size(); triID++)
        {
            auto vertInd = pRenormalizable->TriVertInd( triID );
            normVerts[ vertInd.x ] *= third;
            normVerts[ vertInd.y ] *= third;
            normVerts[ vertInd.z ] *= third;
        }
    }
}
