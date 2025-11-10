#ifndef _RHELIX_HPP_
#define _RHELIX_HPP_

#include <unistd.h>
#include <vector>
#include <map>
#include <functional>
#include <set>

#include "AppTypes.hpp"
#include "CRubus.hpp"

struct RHelix: public IDefineTri, public IRenormalizable
{
    std::vector<glm::vec3> posVerts;
    std::vector<glm::vec3> normVerts;
    std::vector<glm::vec3> colorVerts;
    std::vector<glm::vec3> normTris;

    GLuint boPos = 0, boColor = 0;

    RHelix();
    virtual ~RHelix() = default;

    void Reset();
    void Backup() {}
    void Save(const char* szFilename) {}

    void Bind();
    void Release();
    void Render();
    void RenderCollisionBody(glm::vec3 axisIn, glm::vec3 axisUp, glm::vec3 color);
    void RenderNormals();

    void BrushColor(triID_type triID, glm::vec3 const & color, float const blend = 1.f); // .5 is full blend
    void BrushPos(triID_type triID, glm::vec3 const &normDeform, float const & k);

    void UpdateColor(triID_type vertID);
    void UpdatePos(triID_type triID);
    void UpdateAllStates();

    // IDefineTri
    void TriIterNext(triID_type &triIter) final
    {
        // last 2 points are not part of any tri
        triIter = triID_type(triIter < posVerts.size() -2 -1 ? triIter+1 : TriIDEnd);
    }
    tri_type TriVertPos(triID_type triID) final
    {
        // tri strip winding juju
        triID_type p0=triID, p1=triID+1, p2=triID+2; // all to are ccw, all te are cw
        if(!(p0&1)) std::swap(p0,p1); // swap te to make all ccw

        return { posVerts[ p0 ], posVerts[ p1 ], posVerts[ p2 ] };
    }
    ind3_type TriVertInd(triID_type triID) final
    {
        // tri strip winding juju
        triID_type p0=triID, p1=triID+1, p2=triID+2; // all to are ccw, all te are cw
        if(!(p0&1)) std::swap(p0,p1); // swap te to make all ccw

        return { p0, p1, p2 };
    }
    void TriPosNorm(glm::vec3 &position, glm::vec3 &normal, triID_type const &triID) final
    {
        triID_type p0=triID, p1=triID+1, p2=triID+2; // all to are ccw, all te are cw

        position = {
            (posVerts[ p0 ].x + posVerts[ p1 ].x + posVerts[ p2 ].x) / 3,
            (posVerts[ p0 ].y + posVerts[ p1 ].y + posVerts[ p2 ].y) / 3,
            (posVerts[ p0 ].z + posVerts[ p1 ].z + posVerts[ p2 ].z) / 3,
        };
        normal = normTris[ triID ];
    }
    std::vector<ind3_type>& GetIndTris() final { static std::vector<ind3_type> ugly; return ugly; }
    std::vector<ind3_type>& GetIndTriAdjTris() final { static std::vector<ind3_type> ugly; return ugly; }
    std::vector<glm::vec3>& GetPosVerts() final { return posVerts; }

    // IRenormalizable
    uint TriInd(triID_type triID) final { return triID; }
    IDefineTri* GetIDefineTri() final { return this; }
    ind3_type AdjTriInd(triID_type t) final
    {
        if(t == 0) return { 1, TriIDEnd, TriIDEnd };
        else if(t == normTris.size() -1) return { (triID_type)(normTris.size() -2), TriIDEnd, TriIDEnd };
        else return { (triID_type)(t-1), (triID_type)(t+1), TriIDEnd };
    }
    bool HasDegenerates() final { return false; }
    std::vector<glm::vec3>& GetNormTris() final { return normTris; }
    std::vector<glm::vec3>& GetNormVerts() final { return normVerts; }
    //std::vector<glm::vec3>& GetPosVerts() final { return posVerts; }

    //private:
    CRubus rubus; // collision body

};

#endif //_RHELIX_HPP_
