#ifndef OPENGL1_APPPLATFORM_H
#define OPENGL1_APPPLATFORM_H

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <functional>

struct AppPlatform
{
    struct Event
    {
        const static int MaxTouch = 2;

        enum Kind: uint8_t { // todo: was uint16_t
            Nil,
            Adornment, Key, Touch, Tick, // todo: check Tick might be linux only?
            Close, Refresh, Pause, Resume, // adornments
            Begin, End, Move // touch
        };

        Kind kind;
        union U_
        {
            struct Adornment_
            {
                Kind adKind;
            } adornment;
            struct Key_
            {
                char key;
                bool press;
            } key;
            struct Touch_
            {
                Kind toKind;
                int8_t id; // todo: was int32_t
                int16_t x, y; // todo: was int32_t
            } touch; // touch. also used under touch-emulation (mouse) mode
        } u;
    };

    uint32_t deltaMSec = 0;
    float deltaSecAvg = 0.f;

    void Bind(
        std::function<void(void)> rebindFn_, std::function<void(void)> releaseFn_,
        const char* szWindowname, const char* szDevName
    );
    void Release();
    void Tick(std::function<void(const Event &)> fnEvent);
};

#endif //OPENGL1_APPPLATFORM_H
