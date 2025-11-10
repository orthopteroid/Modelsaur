#ifndef _APPTEXTURE_HPP_
#define _APPTEXTURE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include "AppTypes.hpp"

namespace AppTexture
{
    GLuint LoadFile(const char* szFilename);
    GLuint LoadResource(Resource rez);
};

#endif // _APPTEXTURE_HPP_