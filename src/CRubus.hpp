#ifndef _CRUBUS_HPP_
#define _CRUBUS_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <vector>
#include <map>
#include <functional>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <deque>

#include "AppTypes.hpp"

// The Rubus is the Genera of the Blackberry...
struct CRubus : public IIdentifyTri
{
    // types for mark/sweep collision resolution
    struct sph_markable : public sph_type
    {
        serial_type serial;
    };

    struct triID_markable
    {
        triID_markable( triID_type t ): triID(t), serial(0) {}
        triID_type triID;
        serial_type serial;
    };

    uint8_t dimension = 1; // works, very inefficiently...

    // ray identifies sphere whcich identifies bin which identifies tri
    std::map<binID_type, sph_markable> binSpheres;
    std::multimap<binID_type, triID_markable> binTris;

    // search t_state
    IDefineTri* pTriagonalnomial;
    serial_type serial = 0x1234; // for mark-and-sweep algos

    CRubus();
    virtual ~CRubus() = default;

    void Bind(IDefineTri* p);
    void Release();

    void Reset();

    void Inflate(sph_markable& sph, binID_type bin, std::multimap<binID_type, glm::vec3> const & binVecs);
    void Inflate(triID_type triID, const glm::vec3& v0, const glm::vec3 & v1, const glm::vec3 & v2);

    // IIdentifyTri
    void IdentifyTri(trisearch_type& cxt_out, glm::vec3 const &position_, glm::vec3 const &direction_) final;
};

#endif //_CRUBUS_HPP_
