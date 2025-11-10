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

#define GLM_EXT_INCLUDED
#include <glm/gtx/vector_angle.hpp>

#include "GL9.hpp"

#include "AppTriBrusher.hpp"
#include "AppLog.hpp"
#include "TriTools.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

//#define CHECK_DEADZONES

inline float glLength(const glm::vec3& vec) { return sqrt( glm::dot(vec, vec) ); }

void AppTriBrusher::Bind(IIdentifyTri* pIT, IDefineTri* pDT)
{
    pCollisionBody = pIT;
    pTriangular = pDT;

    adjMarkings.resize( pTriangular->GetIndTris().size() );
    paintMarkings.resize( pTriangular->GetIndTris().size() );
}
void AppTriBrusher::Release()
{
    Stop(); // todo: check
    pCollisionBody = 0;
    pTriangular = 0;
}

void AppTriBrusher::Start(glm::vec3 const & p, glm::vec3 const & camera,
                          std::function<glm::vec3(glm::vec3)> fnProj,
                          std::function<glm::vec3(glm::vec3)> fnUnproj,
                          EffectorFn fnTE,
                          float patch) // todo: won't work under rotation
{
    posLast = posStart = vecLastEnd = p;
    deqSegments.clear(); // check

    posCamera = camera;
    fnProject = fnProj;
    fnUnproject = fnUnproj;
    fnTriEffector = fnTE;

    // cast 3d ray to find tri
    const auto posCursor = fnUnproject( posStart );
    const auto incidentVec = glm::normalize( posCursor - posCamera );
    pCollisionBody->IdentifyTri( searchCxt, posCamera, incidentVec );
    if(searchCxt.collisionTri == TriIDEnd)
    {
        AppLog::Warn(__FILENAME__,"searchCxt.collisionTri == TriIDEnd");
        return;
    }

    // find tri center and normal direction, per projection
    glm::vec3 triPos, triNorm;
    pTriangular->TriPosNorm(triPos, triNorm, searchCxt.collisionTri);

    // find 2d info about tri's normal
    glm::vec3 startNorm_Win2D = fnProject(triPos + triNorm) - fnProject(triPos); // 2d pos of normal end
    startNorm_Win2D.y = -startNorm_Win2D.y; // conv 2d cords from ogl 2d to window 2d
    startNorm_Angle = std::atan2(startNorm_Win2D.y, startNorm_Win2D.x);
    startNorm_Len = glLength( startNorm_Win2D );

    // build set of adjacent tris and their distance
    patchSize = patch;
    adjDeque.clear();

    strokeSerial++;

    if( patchSize < std::numeric_limits<float>::min())
    {
        adjDeque.push_back( { searchCxt.collisionTri, 1 } );
    }
    else
    {
        adjSerial++; // find next adj-set
        AdjTriVisitor(
            adjDeque,
            searchCxt.collisionTri, pTriangular->GetIndTriAdjTris(),
            adjSerial, adjMarkings,
            fnTriEffector
        );
    }
}

void AppTriBrusher::Stop()
{
    deqSegments.clear();
}

void AppTriBrusher::Continue(glm::vec3 const & p)
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

void AppTriBrusher::Stroke_handled( PaintFn fnPaint, uint batchSize )
{
    if(searchCxt.collisionTri == TriIDEnd) return;

    // stroke the initial triangle-patch-set based upon 2d cursor movement across the near-plane
    while (--batchSize) {
        if (deqSegments.empty()) break;

        deqSegments.front().pos += deqSegments.front().delta;
        if (glLength(posLast - deqSegments.front().pos) < 1.f) continue;

        posLast = deqSegments.front().pos;

        deqSegments.front().count--;
        if (deqSegments.front().count < 0) deqSegments.pop_front();

        ///////

        const auto moveVect = posLast - posStart; // window 2d cords
        const float angleM = std::atan2(moveVect.y, moveVect.x);
        const float handleDir = std::fabs(angleM - startNorm_Angle) > float(M_PI / 2) ? -1.f : +1.f;
        const float handleScale = std::pow(1. + patchSize, -3.); // todo: was inverse cubic

        if (patchSize < std::numeric_limits<float>::min()) {
            fnPaint(searchCxt.collisionTri, 1.f, handleScale * handleDir * glLength(moveVect) / startNorm_Len);
        } else {
            for (const auto& triadj : adjDeque) { // todo: added const &
                fnPaint(triadj.triID, triadj.effect, handleScale * handleDir * glLength(moveVect) / startNorm_Len);
            }
        }
    }
}

void AppTriBrusher::Stroke( PaintFn fnPaint, uint batchSize )
{
    int pulled = 0;

    // update and stroke the triangle-patch-set based upon 3d cursor movement across the geometry
    while( --batchSize )
    {
        if( !adjDeque.empty() )
        {
            auto triadj = adjDeque.front();
            adjDeque.pop_front();
            pulled++;

            // prevent selection of this tri later in current adj-set
            if( !cheatUnsafeSelection ) paintMarkings[ triadj.triID ] = adjSerial;

#if defined(CHECK_DEADZONES)
            AppLog::Info( __FILENAME__, "%s painting: triID %4X", __func__, triadj.triID );
#endif // CHECK_DEADZONES

            fnPaint(triadj.triID, triadj.effect, 0);
            continue; // keep pulling
        }

        if( deqSegments.empty()) break;

        deqSegments.front().pos += deqSegments.front().delta;
        posLast = deqSegments.front().pos;

        deqSegments.front().count--;
        if( deqSegments.front().count < 0 ) deqSegments.pop_front();

        ///////

        // find next valid tri
        auto posCursor = fnUnproject( posLast );
        auto incidentVec = glm::normalize( posCursor - posCamera );
        auto trialCxt = searchCxt;
        pCollisionBody->IdentifyTri( trialCxt, posCamera, incidentVec );

#if defined(CHECK_DEADZONES)
        if( patchSize < std::numeric_limits<float>::min())
            AppLog::Warn(__FILENAME__,"%s: %d - %04X (%04X)", __func__,trialCxt.collisionTri,trialCxt.collisionBin,trialCxt.lastValidBin);
#endif // CHECK_DEADZONES

        // use continue to keep parsing deqSegments in the hopes of finding a valid tri
        if( !trialCxt.IsValid() ) continue; // likely brushing off-object
        if( trialCxt.collisionTri == searchCxt.collisionTri ) continue; // same tri
        if( paintMarkings[trialCxt.collisionTri] == adjSerial ) continue; // just-painted tri

        searchCxt = trialCxt; // found a new tri, store it

        if( patchSize < std::numeric_limits<float>::min())
        {
            adjDeque.push_back( { searchCxt.collisionTri, 1 } );
        }
        else
        {
            adjSerial++; // find next adj-set
            AdjTriVisitor(
                adjDeque,
                searchCxt.collisionTri, pTriangular->GetIndTriAdjTris(),
                adjSerial, adjMarkings,
                fnTriEffector
            );
        }

#ifdef CHATTY
        AppLog::Info(__FILENAME__, "queued %d", adjDeque.size());
#endif
    }

#ifdef CHATTY
    if (pulled > 0) AppLog::Info(__FILENAME__, "%d pulled", pulled);
#endif
}

