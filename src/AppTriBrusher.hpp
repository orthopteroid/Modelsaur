#ifndef _APPTRIBRUSHER_HPP_
#define _APPTRIBRUSHER_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <vector>
#include <deque>
#include <cmath>
#include <limits>
#include <map>
#include <functional>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "AppTypes.hpp"
#include "TriTools.hpp"

// can stroke any IIdentifyTri class
struct AppTriBrusher
{
    struct Segment
    {
        glm::vec3 pos;
        glm::vec3 delta;
        int count;
    };
    std::deque<Segment> deqSegments;
    glm::vec3 vecLastEnd;

    IIdentifyTri* pCollisionBody = 0;
    IDefineTri* pTriangular = 0;

    glm::vec3 posLast;
    glm::vec3 posStart;

    float patchSize;
    serial_type adjSerial = 0x1234;
    serial_type strokeSerial = 0x7890;

    float startNorm_Angle; // 2d angle of start tri's normal
    float startNorm_Len; // start tri's normal 2d (pixel) length

    std::vector<serial_type> adjMarkings; // for adj tri selection
    std::deque<trieffect_type> adjDeque;
    trisearch_type searchCxt;
    std::vector<serial_type> paintMarkings; // prevent selection of tris painted in current stroke

    glm::vec3 posCamera;
    std::function<glm::vec3(glm::vec3)> fnProject;
    std::function<glm::vec3(glm::vec3)> fnUnproject;
    EffectorFn fnTriEffector;

    bool cheatUnsafeSelection = false;

    void Bind(IIdentifyTri* pIT, IDefineTri* pDT);
    void Release();

    // start for handled warping
    void Start(glm::vec3 const & p, glm::vec3 const & camera,
               std::function<glm::vec3(glm::vec3)> fnProj,
               std::function<glm::vec3(glm::vec3)> fnUnproj,
               EffectorFn fnTE,
               float ps);

    void Stop();
    void Continue(glm::vec3 const & p);
    void Stroke_handled( PaintFn fnPaint, uint batchSize );
    void Stroke( PaintFn fnPaint, uint batchSize );
};

#endif //_APPTRIBRUSHER_HPP_
