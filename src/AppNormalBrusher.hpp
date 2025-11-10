#ifndef _APPNORMALBRUSHER_HPP_
#define _APPNORMALBRUSHER_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <deque>

#include "AppTypes.hpp"

// can renormalize verts and tris for any IRenormalizable class
struct AppNormalBrusher
{
    std::deque<triID_type> deqSegments;

    IRenormalizable* pRenormalizable = 0;
    triID_type lastTriangle;

    void Bind(IRenormalizable* p);
    void Release();

    void Start();
    void Stop();
    void Continue(triID_type triID);
    void Stroke(uint maxiter);

    void ReStrokeObject();
};

#endif //_APPNORMALBRUSHER_HPP_
