// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <stdarg.h>
#include <stdlib.h>
#include <cstdio>

#include "AppLog.hpp"

namespace AppLog {

#ifdef DEBUG
void Info(const char *szComponent, const char *sz, ...)
{
    va_list args;
    printf("%s: ", szComponent);
    va_start(args, sz);
    vprintf(sz, args);
    va_end(args);
    fputc('\n',stdout);

    fflush(stdout); // nb: slows down the thread, but probably important
}
#endif

void Warn(const char *szComponent, const char *sz, ...)
{
    va_list args;
    printf("%s: ", szComponent);
    va_start(args, sz);
    vprintf(sz, args);
    va_end(args);
    fputc('\n',stdout);

    fflush(stdout); // nb: slows down the thread, but probably important
}

void Err(const char *szComponent, const char *sz, ...)
{
    va_list args;
    printf("%s: ", szComponent);
    va_start(args, sz);
    vprintf(sz, args);
    va_end(args);
    fputc('\n',stdout);

    fflush(stdout); // nb: slows down the thread, but probably important
}

};
