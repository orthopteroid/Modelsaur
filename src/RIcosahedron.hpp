#ifndef _APPICOSAHEDRON_HPP_
#define _APPICOSAHEDRON_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <glm/vec3.hpp>

#include "AppTime.hpp"

struct RIcosahedron
{
    glm::vec3 position;
    GLuint boPoints = 0, boIndicies = 0;
    glm::vec3 color;
    AppAlarm colorSwitcher;
    bool solid = false;

    RIcosahedron();
    void Bind();
    void Release();
    void Tick(uint32_t delta);
    void Render();
};

#endif //_APPICOSAHEDRON_HPP_
