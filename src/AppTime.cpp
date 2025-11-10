// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <time.h>
#include <string>
#include <ctime>
#include <stdint.h>

#include "AppTime.hpp"

AppScopeTime::AppScopeTime(float& elapsed_) : elapsed(elapsed_)
{
    clock_gettime(CLOCK_REALTIME, &spec0);
}
AppScopeTime::~AppScopeTime()
{
    clock_gettime(CLOCK_REALTIME, &spec1);
    spec1.tv_sec -= spec0.tv_sec;
    spec1.tv_nsec -= spec0.tv_nsec;
    elapsed = .75f * elapsed + .25f * ((float)spec1.tv_sec + (float)spec1.tv_nsec / 1E+9f);
}

std::string AppTimeCode32()
{
    const char* u4 = "ABCDEFGHIJKLMNOP";
    const char* n3 = "12345678";

    // >>2 makes 30 bits makes dt of 4 secs
    auto num = uint32_t(std::time(nullptr)) >> 2;

    // build right-to-left
    std::string code;
    code.insert( code.begin(), n3[ num & 0b111 ] ); num = num >> 3;
    code.insert( code.begin(), n3[ num & 0b111 ] ); num = num >> 3;
    code.insert( code.begin(), n3[ num & 0b111 ] ); num = num >> 3;
    code.insert( code.begin(), n3[ num & 0b111 ] ); num = num >> 3; // 12 bits ~ 4096 ticks ~ 16384 secs ~ 4.5 hours
    code.insert( code.begin(), u4[ num & 0b1111 ] ); num = num >> 4;
    code.insert( code.begin(), u4[ num & 0b1111 ] ); num = num >> 4;
    code.insert( code.begin(), u4[ num & 0b1111 ] ); num = num >> 4;
    code.insert( code.begin(), u4[ num & 0b1111 ] ); num = num >> 4;
    return code;
}
