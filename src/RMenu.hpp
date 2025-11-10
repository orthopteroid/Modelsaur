#ifndef _APPMENU_HPP_
#define _APPMENU_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <list>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct RMenu
{
    typedef enum { Cat, Row } place_type;

    struct item_type
    {
        uint8_t token;
        place_type placement;
        GLuint txImage;

        item_type(uint8_t t, place_type p, GLuint i) : token(t), placement(p), txImage(i) {}

        glm::vec3 translate;
        glm::mat4 mxPlacement; // transforms unit-square image into platform-coords
    };
    std::list<item_type> menu;

    uint8_t menuToken; // top-left sends token to toggle menu on/off

    // common props, reused for each tile when rendering
    float layoutDim;

    std::vector<glm::vec3> posVerts; // quad for max size, 0 at center, on near plane
    GLuint boVerts;

    std::vector<glm::vec2> posTexVerts;
    GLuint boTexVerts;

    bool visible = false;

    RMenu() = default;
    virtual ~RMenu() = default;

    void Bind(uint8_t mt, int platMinSq, float z);
    void Render();
    void Release();
    uint8_t* Translate(int32_t x, int32_t y);
    void Tick();
};

#endif //_APPMENU_HPP_
