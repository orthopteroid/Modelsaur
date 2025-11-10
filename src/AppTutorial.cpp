// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <time.h>
#include <string>
#include <ctime>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

#include "AppTutorial.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define MARKED 'X'
#define UNMARKED '?'
#define UNMARKABLE '!'

///////////

namespace AppTutorial
{

std::list<item_type> items;

std::list<item_type>& Items() { return items; }

std::unique_ptr<char[]> SaveMarkings_01()
{
    std::unique_ptr<char[]> markings( new char[items.size() +1] );

    int j = 0;
    for(auto i: items) markings[ j++ ] = i.mark;
    markings[items.size()] = 0;

    return markings;
}

void RestoreMarkings_01( const char* sz )
{
    if( strlen(sz) != items.size() )
    {
        AppLog::Err(__FILENAME__, "format error" );
        return;
    }

    int j = 0;
    for(auto& i: items) i.mark = sz[ j++ ]; // note &
}

const char* TestMarkGet(char i)
{
    for(auto& j: items) // nb: auto&
        if(i == j.state)
        {
            if(j.mark == MARKED) return 0;
            if(j.mark == UNMARKED) j.mark = MARKED;
            return j.sz;
        }

    return 0;
}

}
