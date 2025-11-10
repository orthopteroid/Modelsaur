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
#include <linux/stat.h>
#include <asm/fcntl.h>
#include <sys/stat.h>
#include <cstdarg>
#include <cstring>

#include "AppLog.hpp"
#include "AppFile.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

AppFile::AppFile(const char* szFilename, file_type ft, file_mode fm)
{
    std::string buffer( ft == Preferences ? Platform_InternalPath() : Platform_ExternalPath() );
    buffer.push_back('/');
    buffer += szFilename;
    AppLog::Info(__FILENAME__, "%s opening %s for %s", __func__, buffer.c_str(), fm == WriteMode ? "write" : "read" );
    
    pFile = fopen( buffer.c_str(), fm == WriteMode ? "w" : "r" );
    if(!pFile) AppLog::Warn(__FILENAME__, "fopen failed");
}

AppFile::~AppFile()
{
    if(pFile) fclose( pFile );
    pFile = 0;
}

void AppFile::Printf(const char* format, ...)
{
    if(!pFile) { AppLog::Warn(__FILENAME__, "scanf failed"); return; }

    va_list args;
    va_start(args, format);
    int code = vfprintf(pFile, format, args);
    if(code < 0) AppLog::Warn(__FILENAME__,"vfprintf error %d", code);
    va_end(args);
}

void AppFile::Scanf(const char* format, ...)
{
    if(!pFile) { AppLog::Warn(__FILENAME__, "scanf failed"); return; }

    va_list args;
    va_start(args, format);
    int code = vfscanf(pFile, format, args);
    if(code < 0) AppLog::Warn(__FILENAME__,"vfscanf error %d", code);
    va_end(args);
}
