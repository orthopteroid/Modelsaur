#ifndef _SIMPLETRI_HPP_
#define _SIMPLETRI_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <glm/vec3.hpp>

struct RSimpleTri
{
    glm::vec3 scale, position;
    GLuint boPoints = 0;
    bool visible = false;

    RSimpleTri();
    void Bind(int platMinSq);
    void Update(glm::vec3 pos, bool vis);
    void Release();
    void Render();
};

#endif //_SIMPLETRI_HPP_
