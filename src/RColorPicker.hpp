#ifndef _RCOLORPICKER_HPP_
#define _RCOLORPICKER_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <list>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct RColorPicker
{
    bool visible = false;

    int platHeight, platWidth;
    GLuint txImage;
    float circleDiameter;
    glm::vec3 translate;
    glm::mat4 mxPlacement; // transforms unit-square image into platform-coords

    std::vector<glm::vec3> posVerts; // quad for max size, 0 at center, on near plane
    GLuint boVerts;

    std::vector<glm::vec2> posTexVerts;
    GLuint boTexVerts;

    glm::vec3 color;

    RColorPicker() = default;
    virtual ~RColorPicker() = default;

    void Bind(int w, int h, int z, GLuint tx);
    bool PickColor(glm::vec3 pos);
    void Render();
    void Release();
};

#endif //_RCOLORPICKER_HPP_
