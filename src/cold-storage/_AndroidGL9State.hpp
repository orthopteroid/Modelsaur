#ifndef _ANDROIDGL9STATE_HPP_
#define _ANDROIDGL9STATE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <android/native_window.h>
#include "AppPlatform.hpp"

struct AppPlatform::State
{
    ANativeWindow* window;
};

#endif // _ANDROIDGL9STATE_HPP_
