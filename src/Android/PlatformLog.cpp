// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <stdarg.h>
#include <android/log.h>
#include <cstdio>

#include "AppLog.hpp"

namespace AppLog {

void Info(const char *szComponent, const char *sz, ...)
{
#ifdef DEBUG
    va_list args;
    //printf("%s: ", szComponent);
    va_start(args, sz);
    __android_log_vprint(ANDROID_LOG_INFO, szComponent, sz, args);
    va_end(args);
#endif
}

void Warn(const char *szComponent, const char *sz, ...)
{
    va_list args;
    //printf("%s: ", szComponent);
    va_start(args, sz);
    __android_log_vprint(ANDROID_LOG_WARN, szComponent, sz, args);
    va_end(args);
}

void Err(const char *szComponent, const char *sz, ...)
{
    va_list args;
    //printf("%s: ", szComponent);
    va_start(args, sz);
    __android_log_vprint(ANDROID_LOG_ERROR, szComponent, sz, args);
    va_end(args);
}

};

