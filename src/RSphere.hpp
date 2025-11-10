#ifndef _RSPHERE_HPP_
#define _RSPHERE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <vector>
#include <map>
#include <functional>
#include <set>

#include "AppTypes.hpp"
#include "CRubus.hpp"

struct RSphere: public IDefineTri, public IRenormalizable
{
    std::vector<glm::vec3> posVerts;
    std::vector<glm::vec3> posVerts_backup;
    std::vector<ind3_type> indTriVerts;
    std::vector<ind3_type> indTriAdjTris;
    std::vector<glm::vec3> normTris, normVerts;

    std::vector<float> normEffectVerts;

    std::vector<glm::vec3> colorVerts;
    std::vector<glm::vec3> colorVerts_backup;
    std::set<triID_type> triUpdateSet;

    template<class T>
    void BufferSubData_Chicklet(
        const GLenum& target,
        const std::vector<T>& vec,
        const std::set<uint16_t>& markings,
        const uint16_t& chickletBitSize
    );

    GLuint boPos = 0;
    GLuint boIndicies = 0;
    GLuint boNormals = 0;
    GLuint boColor = 0;

    uint bytesUpdated = 0;

    static bool CheatSphereOnly;

    uint GetDivisions() const;

    RSphere() = default;
    virtual ~RSphere() = default;

    void Reset(bool fromBackup = false);
    void Backup();
    void SavePLY(const char *szFilename);
    void SaveSTL(const char* szFilename);

    void Bind();
    void Release();
    void Render();
    void RenderCollisionBody(glm::vec3 axisIn, glm::vec3 axisUp, glm::vec3 color);
    void RenderNormals();

    void BrushPos(triID_type triID, glm::vec3 const &normDeform, float const & k);
    void UpdatePos(triID_type triID);
    void UpdatePosTick();
    void UpdatePosFinalize();

    void BrushZ(triID_type triID, float const &k);
    void UpdateNormalFinalize();

    void BrushColor(triID_type triID, glm::vec3 const & color, float const blend = 1.f); // .5f is full blend
    void UpdateColor(triID_type vertID);
    void UpdateColorTick();
    void UpdateColorFinalize();

    void UpdateAllStates();

    // IDefineTri
    void TriPosNorm(glm::vec3 &position, glm::vec3 &normal, triID_type const &triID) final
    {
        ind3_type tri = indTriVerts[ triID ];
        position = {
            (posVerts[ tri.x ].x + posVerts[ tri.x ].x + posVerts[ tri.x ].x) / 3,
            (posVerts[ tri.y ].y + posVerts[ tri.y ].y + posVerts[ tri.y ].y) / 3,
            (posVerts[ tri.z ].z + posVerts[ tri.z ].z + posVerts[ tri.z ].z) / 3,
        };
        normal = normTris[ triID ];
    }
    std::vector<ind3_type>& GetIndTris() final { return indTriVerts; }
    std::vector<ind3_type>& GetIndTriAdjTris() final { return indTriAdjTris; }
    void GetTriVerts(glm::vec3 &v0, glm::vec3 &v1, glm::vec3 &v2, triID_type tri) final
    {
        v0 = posVerts[ indTriVerts[ tri ].x ];
        v1 = posVerts[ indTriVerts[ tri ].y ];
        v2 = posVerts[ indTriVerts[ tri ].z ];
    }

    // IRenormalizable
    uint TriInd(triID_type triID) final { return triID; }
    ind3_type TriVertInd(triID_type triID) final { return indTriVerts[ triID ]; }
    IDefineTri* GetIDefineTri() final { return this; }
    ind3_type AdjTriInd(triID_type t) final
    {
        auto offset = triID_type(GetDivisions());
        if(t == 0) return { 1, TriIDEnd, TriIDEnd }; // end
        else if(t == normTris.size() -1) return { triID_type(normTris.size()-2), TriIDEnd, TriIDEnd }; // end
        else if(offset < t && t < normTris.size() -1 -offset) return { triID_type(t-1), triID_type(t+1), triID_type(t+offset) }; // middle
        else return { triID_type(t-1), triID_type(t+1), TriIDEnd }; // near an end
    }
    bool HasDegenerates() final { return false; }
    std::vector<glm::vec3>& GetNormTris() final { return normTris; }
    std::vector<glm::vec3>& GetNormVerts() final { return normVerts; }
    std::vector<glm::vec3>& GetPosVerts() final { return posVerts; }

    //private:
    CRubus rubus; // collision body

};


#endif //_RSPHERE_HPP_
