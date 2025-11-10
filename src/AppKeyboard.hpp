#ifndef _APPKEYBOARD_HPP_
#define _APPKEYBOARD_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <cstdint>
#include <list>

struct AppKeyboard
{
    enum KeyMask: uint8_t {
        Fresh = 4, Press = 2, Release = 1,
        FreshPress = Fresh | Press,
        FreshRelease = Fresh | Release,
    };

    std::list<uint8_t> activekeys;
    KeyMask keyboard[256];

    AppKeyboard();

    bool Check(uint8_t key, KeyMask state) const { return (keyboard[key] & state) == state; }
    bool Check(const char* szKeys, KeyMask state) const;

    void Toggle(uint8_t key);
    void DoPress(uint8_t key);
    void DoRelease(uint8_t key);
    void DoKey(uint8_t key, bool press)
    {
        if(press)
            DoPress(key);
        else
            DoRelease(key);
    }
    void Tick();
};

inline AppKeyboard::KeyMask operator|(const AppKeyboard::KeyMask& a, const AppKeyboard::KeyMask& b)
{
    return AppKeyboard::KeyMask( uint8_t(a) | uint8_t(b) );
}

inline AppKeyboard::KeyMask operator&(const AppKeyboard::KeyMask& a, const AppKeyboard::KeyMask& b)
{
    return AppKeyboard::KeyMask( uint8_t(a) & uint8_t(b) );
}

inline AppKeyboard::KeyMask operator~(const AppKeyboard::KeyMask& a)
{
    return AppKeyboard::KeyMask( ~uint8_t(a) );
}

#endif //_APPKEYBOARD_HPP_
