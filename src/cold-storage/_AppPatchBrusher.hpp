#ifndef _APPPATCHBRUSHER_HPP_
#define _APPPATCHBRUSHER_HPP_

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

// can stroke any IIdentifyTri class
struct AppPatchBrusher
{
    using PaintFn = std::function<void(triID_type, glm::vec3 &)>;

    struct Segment
    {
        glm::vec3 pos;
        glm::vec3 delta;
        int count;
    };
    std::deque<Segment> deqSegments;
    glm::vec3 vecLastEnd;

    IIdentifyTri* pCollisionBody = 0;
    glm::vec3 pos;
    triID_type lastTriangle;

    void Bind(IIdentifyTri* p);
    void Release();

    void Start(glm::vec3 const & p);
    void Stop();
    void Continue(glm::vec3 const & p);
    void Stroke(glm::vec3 &posCamera,
                std::function<glm::vec3(glm::vec3)> fnUnproject,
                PaintFn fnPaint,
                uint maxiter);
};

#endif //_APPPATCHBRUSHER_HPP_
