#ifndef _APPTEXT_HPP_
#define _APPTEXT_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <list>
#include <functional>
#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct RText
{
    glm::vec3 position = {0,0,0};
    GLshort quad[12];

    std::string text;
    short platWidth, platHeight, cols;

    bool bound = false;
    GLuint boVerts;
    short textLength = 0;

    RText(short cols_, std::string text_)
    {
        cols = cols_;
        text = text_;
        textLength = text.length();
    }
    virtual ~RText()
    {
        Release();
    }

    static std::unique_ptr<RText> Factory( short cols, std::string text_ )
    {
        return std::unique_ptr<RText>(new RText(cols, text_));
    }

    void Bind(short platWidth_, short platHeight_);
    void Release();

    void Render();
};

#endif //_APPTEXT_HPP_
