#ifndef _APPLOG_HPP_
#define _APPLOG_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

namespace AppLog
{

#ifdef DEBUG
    void Info(const char* szComponent, const char* sz, ...);
#else
    inline void Info(const char* szComponent, const char* sz, ...) {}
#endif

void Warn(const char* szComponent, const char* sz, ...);

void Err(const char* szComponent, const char* sz, ...);

};

#endif // _APPLOG_HPP_
