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

#include "AppFile.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

AppFile::AppFile(const char* szFilename, file_type ft, file_mode fm)
{
    // ensure path exists
    std::string buffer( ft == Preferences ? Platform_InternalPath() : Platform_ExternalPath() );
    buffer.push_back('/');
    buffer += szFilename;
    AppLog::Info(__FILENAME__, "%s opening %s for %s", __func__, buffer.c_str(), fm == WriteMode ? "write" : "read" );

    int flags = fm == WriteMode ? O_CREAT | O_WRONLY | O_TRUNC : O_RDONLY;
    int iFile = open(buffer.c_str(), flags, 0660);
    if(iFile > 0) close(iFile);

    pFile = fopen( buffer.c_str(), fm == WriteMode ? "w" : "r" );
    if(!pFile) AppLog::Err(__FILENAME__, "%s FILE null", __func__);
}
AppFile::~AppFile()
{
	if(pFile) fclose(pFile);
    pFile = 0;
}

void AppFile::Printf(const char* format, ...)
{
    if(!pFile) AppLog::Err(__FILENAME__, "%s FILE null", __func__);

    va_list args;
    va_start(args, format);
    int code = vfprintf(pFile, format, args); // api21
    if(code < 0) AppLog::Warn(__FILENAME__, "vfprintf format error %d", code);
    va_end(args);
}

void AppFile::Scanf(const char* format, ...)
{
    if(!pFile) AppLog::Err(__FILENAME__, "%s FILE null", __func__);

    va_list args;
    va_start(args, format);
    int code = vfscanf(pFile, format, args); // api21
    if(code < 0) AppLog::Warn(__FILENAME__,"vfscanf error %d", code);
    va_end(args);
}

