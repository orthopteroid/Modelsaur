// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <csignal>

#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <linux/stat.h>
#include <asm/fcntl.h>
#include <fcntl.h>

#include "AppLog.hpp"
#include "AppFile.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

struct AppFile::Opaque
{
    char buffer[1024];
    int fint = 0;
};

AppFile::AppFile()
{
    pOpaque = new Opaque;
}
AppFile::~AppFile()
{
    Close();
    delete pOpaque;
    pOpaque = 0;
}

void AppFile::Open(const char* szFilename, file_type ft, file_mode fm)
{
#if false
    char *path;
    if(ft == Preferences)
        snprintf(path, 255, "/data/data/com.orthopteroid.blob/files");
    else
    {
        snprintf(path, 255, "%s/com.orthopteroid.blob", getenv("EXTERNAL_STORAGE"));
        mkdir(Platform_ExternalPath(), 0777);
    }
#else
    mkdir(Platform_ExternalPath(), 0777);
#endif

    snprintf(
            pOpaque->buffer, sizeof(pOpaque->buffer), "%s/%s",
            ft == Preferences ? Platform_InternalPath() : Platform_ExternalPath(),
            szFilename
    );
    int flags = fm == Write ? O_CREAT | O_WRONLY | O_TRUNC : O_RDONLY;
    pOpaque->fint = open(pOpaque->buffer, flags, 0644);
    if(pOpaque->fint < 0) AppLog::Warn(__FILENAME__, "open failed");
}
void AppFile::Close()
{
	if(pOpaque->fint !=0) close(pOpaque->fint);
    pOpaque->fint = 0;
}

void AppFile::Printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int n = vsnprintf(pOpaque->buffer, sizeof(pOpaque->buffer), format, args);
    if(n < 0) AppLog::Warn(__FILENAME__, "vsnprintf format error");
    if(n > sizeof(pOpaque->buffer)) AppLog::Warn(__FILENAME__, "vsnprintf buffer size error");
    va_end(args);
    int err = write(pOpaque->fint, pOpaque->buffer, strlen(pOpaque->buffer));
    if(err < 0) AppLog::Warn(__FILENAME__, "write failed");
}
