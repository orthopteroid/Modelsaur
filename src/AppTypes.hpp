#ifndef _APPTYPES_HPP_
#define _APPTYPES_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <functional>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <deque>

typedef uint16_t serial_type;
typedef uint16_t vertID_type;
typedef uint16_t binID_type;
typedef uint16_t triID_type;
typedef uint32_t binID_triID_key;

const triID_type TriIDEnd = std::numeric_limits<triID_type>::max();
const triID_type TriIDBegin = 0;

const binID_type BinIDEnd = std::numeric_limits<binID_type>::max();

//////////////////

struct sph_type
{
    glm::vec3 center;
    float radius;
};

struct trieffect_type
{
    triID_type triID;
    float effect;
};

inline bool operator<(const trieffect_type l, const trieffect_type r) { return l.triID < r.triID; }

struct ind3_type: public glm::tvec3<uint16_t>
{
    ind3_type() {}
    ind3_type(uint16_t a, uint16_t b, uint16_t c) : glm::tvec3<uint16_t>(a,b,c) {}
    using base_type = uint16_t;
    static const unsigned int base_typeid = 0x1403; //GL_UNSIGNED_SHORT;
};

struct trisearch_type
{
    triID_type collisionTri;
    binID_type collisionBin;
    binID_type lastValidBin;

    trisearch_type() { Invalidate(); }
    void Invalidate()
    {
        collisionTri = TriIDEnd;
        collisionBin = BinIDEnd;
        lastValidBin = 0; // must never be invalid
    }
    bool IsValid() { return collisionTri != TriIDEnd && collisionBin != BinIDEnd; }
};

//////////////////

using PaintFn = std::function<void(triID_type, float /* patchEffect */, float /* handleEffect */)>;

using EffectorFn = std::function< std::pair<bool /* include */, float /* patchEffect */>(triID_type)>;

//////////////////

// a collision alg requires this to iterate on tris and get their data
struct IDefineTri
{
    virtual void TriPosNorm(glm::vec3 &position, glm::vec3 &normal, triID_type const &tri) = 0;

    virtual std::vector<ind3_type>& GetIndTris() = 0;
    virtual std::vector<ind3_type>& GetIndTriAdjTris() = 0;
    virtual void GetTriVerts(glm::vec3 &v0, glm::vec3 &v1, glm::vec3 &v2, triID_type tri) = 0;
};

// a collision alg requires this to find ray-intersecting tris
struct IIdentifyTri
{
    virtual void IdentifyTri(trisearch_type& cxt_out, glm::vec3 const &position, glm::vec3 const &direction) = 0;
};

// for triangle renormalization
struct IRenormalizable
{
    virtual IDefineTri* GetIDefineTri() = 0;

    virtual uint TriInd(triID_type t) = 0;
    virtual ind3_type TriVertInd(triID_type t) = 0;
    virtual ind3_type AdjTriInd(triID_type t) = 0; // on a surface there should be 6 adjacent tris... we only pick 3 here!
    virtual bool HasDegenerates() = 0;

    virtual std::vector<glm::vec3>& GetNormTris() = 0;
    virtual std::vector<glm::vec3>& GetNormVerts() = 0;
    virtual std::vector<glm::vec3>& GetPosVerts() = 0;
};

///////////////

struct Resource {
    const char* mName;
    const uint8_t *mStart;
    const uint8_t *mEnd;
    size_t mSize;

    Resource(const char* name, const uint8_t *start, const uint8_t *end)
        : mName(name), mStart(start), mEnd(end), mSize(end - start)
    {}
};
#define xstr(a) str(a)
#define str(a) #a
#define ACCESS_RESOURCE(x) ([]() {                                    \
        extern const char _binary_##x##_start, _binary_##x##_end;   \
        return Resource(xstr(x), (uint8_t*)&_binary_##x##_start, (uint8_t*)&_binary_##x##_end);  \
    })()


///////////////////

#ifdef __ANDROID_API__

typedef unsigned int GLenum;
typedef float_t GLfloat;
typedef unsigned int GLbitfield;
typedef int GLsizei;
typedef void GLvoid;
typedef int GLint;

#define GL_CLIENT_ALL_ATTRIB_BITS 0x0000

// ogl1.4 fixed-pipeline emulation
void glMatrixMode(GLenum mode);
void glPushMatrix();
void glPopMatrix();
void glLoadMatrixf(const GLfloat *f);
void glLoadIdentity();
void glMultMatrixf(const GLfloat *f);
void glEnableClientState(GLenum cap);
void glDisableClientState(GLenum cap);
void glPushClientAttrib(GLbitfield mask);
void glPopClientAttrib();
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
//void glNormalPointer(GLenum type, GLsizei stride, const GLvoid * pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);

#endif

#endif //_APPTYPES_HPP_
