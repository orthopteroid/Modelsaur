#ifndef OPENGL1_TETRAHEDRON_H
#define OPENGL1_TETRAHEDRON_H

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <glm/vec3.hpp>

#include "AppTime.hpp"

struct RTetrahedron
{
    glm::vec3 position;
    GLuint boPoints = 0;
    glm::vec3 color;
    AppAlarm colorSwitcher;

    RTetrahedron();
    void Bind();
    void Release();
    void Tick(long delta);
    void Render();
};


#endif //OPENGL1_TETRAHEDRON_H
