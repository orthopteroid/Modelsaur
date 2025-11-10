#ifndef _TRITOOLS_HPP_
#define _TRITOOLS_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <vector>
#include <map>
#include <functional>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "AppTypes.hpp"

binID_type BinMake(glm::vec3 const & pos, uint8_t dim);

inline binID_type BinAdjust(binID_type bin, int8_t dx, int8_t dy, uint8_t dim)
{
    uint8_t x = uint8_t(bin >> 8), y = uint8_t(bin & 0xFF);
    uint8_t ux = (uint8_t(x + dx) % dim);
    uint8_t uy = (uint8_t(y + dy) % dim);
    binID_type newBin = ux << 8 | uy;
    return newBin;
}

inline uint BinLinearDist(uint8_t a, uint8_t b, uint8_t dim)
{
    int a0 = a, b0 = b;
    if(b<a) { std::swap(a0,b0); }
    uint d = std::min( b0 - a0, (a0 + dim) - b0 ); // shorter of distances around ring
    return d;
}

void TriVertNormals(
        std::vector<glm::vec3>& normVerts,
        std::vector<glm::vec3>& normTris,
        const std::vector<ind3_type>& indTriVerts,
        const std::vector<glm::vec3>& posVerts
);

void IndTriAdjTris(
        std::vector<ind3_type>& indTriAdjTris,
        const std::vector<ind3_type>& indTriVerts
);

void AdjTriVisitor(
    std::deque<trieffect_type>& deq_out,
    triID_type triStart,
    const std::vector<ind3_type>& indTriAdjTris,
    const serial_type serial,
    std::vector<serial_type>& serialMarkings,
    EffectorFn& fnTriEffector
);

#endif //_TRITOOLS_HPP_
