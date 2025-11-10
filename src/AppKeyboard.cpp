// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <algorithm>
#include <iostream>
#include <cstring>

#include "AppKeyboard.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
//#define CHATTY

AppKeyboard::AppKeyboard()
{
    for(int i=0;i<256;i++) { keyboard[i] = Release; }
}

void AppKeyboard::Toggle(uint8_t key)
{
    keyboard[key] = (keyboard[key] & Press ? Release : Press) | Fresh;

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "toggle %c -> %s ", key, keyboard[key] & Press ? "FreshPress" : "FreshRelease");
#endif

    auto it = std::find(activekeys.begin(), activekeys.end(), key);
    if (it == activekeys.end()) activekeys.push_back(key);
}

void AppKeyboard::DoPress(uint8_t key)
{
    if(keyboard[key] != Release) return;

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "press %c", key);
#endif

    keyboard[key] = FreshPress;

    auto it = std::find(activekeys.begin(), activekeys.end(), key);
    if (it == activekeys.end()) activekeys.push_back(key);
}

void AppKeyboard::DoRelease(uint8_t key)
{
    if(keyboard[key] != Press) return;

#ifdef CHATTY
    AppLog::Info(__FILENAME__, "release %c", key);
#endif

    keyboard[key] = FreshRelease;

    auto it = std::find(activekeys.begin(), activekeys.end(), key);
    if (it == activekeys.end()) activekeys.push_back(key);
}

bool AppKeyboard::Check(const char* szKeys, KeyMask state) const
{
    if(!szKeys) return false;

    for(const char* p = szKeys; *p; p++)
    {
        if( Check(uint8_t(*p), state) )
            return true;
    }
    return false;
}

void AppKeyboard::Tick()
{
    auto it = activekeys.begin();
    while(it != activekeys.end())
    {
        auto k = *it;

        if( keyboard[k] & Fresh ) // fresh gets removed
        {
            keyboard[k] = keyboard[k] & ~Fresh; // remove fresh

#ifdef CHATTY
            AppLog::Info(__FILENAME__, "stale %c -> %s ", k, keyboard[k] & Press ? "Press" : "Release");
#endif
            it = activekeys.erase(it); // remove from active-list
        }

        ++it;
    }
}
