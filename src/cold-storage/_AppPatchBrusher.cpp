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

#include "AppPatchBrusher.hpp"

void AppPatchBrusher::Bind(IIdentifyTri* p)
{
    pCollisionBody = p;
}
void AppPatchBrusher::Release()
{
    Stop(); // todo: check
    pCollisionBody = 0;
}

void AppPatchBrusher::Start(glm::vec3 const & p)
{
    pos = vecLastEnd = p;
    lastTriangle = TriIDEnd;
}

void AppPatchBrusher::Stop()
{
    deqSegments.clear();
    lastTriangle = TriIDEnd;
}

void AppPatchBrusher::Continue(glm::vec3 const & p)
{
    glm::vec3 vecDelta(p - vecLastEnd);

    // touch coordinates are 2d screen pixels
    float touchLen = glm::length(vecDelta);
    if(std::fabs(touchLen) < 1)
    {
        vecLastEnd = p;
        return;
    }

    deqSegments.push_back( { vecLastEnd, vecDelta / touchLen, int(touchLen) } );
    vecLastEnd = p;
}

void AppPatchBrusher::Stroke(glm::vec3 &posCamera,
                              std::function<glm::vec3(glm::vec3)> fnUnproject,
                            PaintFn fnPaint,
                              uint maxiter)
{
    while(--maxiter)
    {
        if(deqSegments.size()== 0) break;

        deqSegments.front().pos += deqSegments.front().delta;
        pos = deqSegments.front().pos;

        deqSegments.front().count--;
        if(deqSegments.front().count < 0) deqSegments.pop_front();

        ///

        auto posCursor = fnUnproject( pos );
        auto incidentVec = glm::normalize( posCursor - posCamera );

        triID_type tri = pCollisionBody->IdentifyTri( posCamera, incidentVec );

        // are we out of triangles?
        if( tri == TriIDEnd ) continue;

        // is this the same triangle as the last one we painted?
        if( tri == lastTriangle ) continue;
        lastTriangle = tri;

        // new triangle... paint it!
        fnPaint( tri, incidentVec );
    }
}
