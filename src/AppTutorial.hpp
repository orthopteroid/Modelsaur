#ifndef _APPTURORIAL_HPP_
#define _APPTURORIAL_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <memory>
#include <list>
#include <string>
#include <functional>

namespace AppTutorial
{

    struct item_type
    {
        char mark;
        char state;
        const char* sz;
    };
    std::list<item_type>& Items();

    constexpr auto VERSION_01 = "tutorial " __DATE__ " " __TIME__;
    std::unique_ptr<char[]> SaveMarkings_01();
    void RestoreMarkings_01( const char* sz );

    const char* TestMarkGet(char i);
    inline void TestAndQueue(char i, std::function<void(const char*)> enqueuer)
    {
        auto sz = AppTutorial::TestMarkGet( i );
        if( sz ) enqueuer( sz );
    }

}

#endif //_APPTURORIAL_HPP_
