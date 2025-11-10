#ifndef _APPTIME_HPP_
#define _APPTIME_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

struct AppScopeTime
{
    struct timespec spec0;
    struct timespec spec1;
    float& elapsed;
    AppScopeTime(float& elapsed_);
    ~AppScopeTime();
};

struct AppAlarm
{
    long elapsed = 0;
    bool triggered = false;
    long interval = 1000;
    AppAlarm(long i = 1000) : interval(i) {}
    void Tick(unsigned long delta)
    {
        triggered = false;
        elapsed += delta;
        if(elapsed >= interval)
        {
            triggered = true;
            elapsed -= interval;
            if(elapsed >= interval) elapsed = 0;
        }
    }
};

// odd indicates triggered
template<long Interval>
void IntervalTick(long& state, long delta)
{
    state += delta;
    state &= ~1; // clear low bit
    if(state >= Interval)
    {
        state -= Interval;
        if(state >= Interval) state = 0;
        state |= 1; // set low bit
    }
}

// true indicates triggered
template<long Interval>
bool IntervalTick(long& state, long delta)
{
    state += delta;
    if(state >= Interval)
    {
        state -= Interval;
        if(state >= Interval) state = 0;
        return true;
    }
    return false;
}

std::string AppTimeCode32();

#endif //_APPTIME_HPP_
