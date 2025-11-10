// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <time.h>
#include <cstring>
#include <string>
#include <ctime>
#include <vector>
#include <cassert>

#include "AppLog.hpp"
#include "AppML.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

FILE* fmemopen(void * __restrict buf, size_t size, const char * __restrict mode);

static char* szHandle( uint32_t handle )
{
    const char* l4 = "abcdefghijklmnop";
    const char* n3 = "12345678";

    // To aide in human-readability the strategy is to make the right-most characters
    // the (likely) higher-entropic bits (ie the handle's least-significant bits).
    static char buf[ 6 ];
    buf[5] = '\0';
    buf[4] = n3[ handle &  0b111 ]; handle = handle >> 3;
    buf[3] = n3[ handle &  0b111 ]; handle = handle >> 3;
    buf[2] = l4[ handle & 0b1111 ]; handle = handle >> 4;
    buf[1] = l4[ handle & 0b1111 ]; handle = handle >> 4;
    buf[0] = l4[ handle & 0b1111 ]; //handle = handle >> 4;

    return buf;
}

///////////////////////
// for writing

uint32_t AppML::Register(const char *szURI)
{
    if( !pFile ) {
        AppLog::Err(__FILENAME__, "%s: file error", __func__ );
        return 0;
    }

    auto hURI = Hash(szURI);
    uriregistry_type::iterator i = uriRegistry.find( hURI );
    assert( i == uriRegistry.end() ); // collision
    uriRegistry[ hURI ] = std::string( szURI );

    fprintf( pFile, "@%s:%s\n", szHandle( hURI ), szURI );

    return hURI;
}

void AppML::Emit( uint32_t hURI, char* szData )
{
    if( !pFile ) {
        AppLog::Err(__FILENAME__, "%s: file error", __func__ );
        return;
    }

    if( hURI )
    {
        uriregistry_type::iterator i = uriRegistry.find( hURI );
        assert( i != uriRegistry.end() ); // unregistered

        fprintf( pFile, "%s:%s\n", szHandle( hURI ), szData );
    }
    else
    {
        fprintf( pFile, "%s\n", szData );
    }
}

////////////////////
// for reading

void AppML::Register( const char* szURI, handler_type handler )
{
    if( !szURI ) {
        AppLog::Err(__FILENAME__, "%s uri null", __func__ );
        return;
    }
    auto hURI = *szURI ? Hash( szURI ) : 0; // "" is null uri

    handlerRegistry[ hURI ] = handler;
}

// Namespaced data is returned line-by-line via registered handler.
// Unnamespaced (default) data is returned via handler namespace "".
// Comments, empty lines and incomplete ns decls are skipped.
// #<comment>\n
// @<ns>:<uri>\n
// <ns>:<data>\n
// <data>\n
void AppML::Parse()
{
    if( !pFile ) {
        AppLog::Err(__FILENAME__, "%s: file error", __func__ );
        return;
    }

    const uint32_t COMMENT    = 0b000000001;
    const uint32_t TOKEN_DECL = 0b000000010;
    const uint32_t URI        = 0b000000100;
    const uint32_t TOKEN      = 0b000001000;
    const uint32_t DATA       = 0b000010000;
    const uint32_t MUTABLE    = 0b100000000;
    uint32_t mode;

    using tokenmap_type = std::map<uint32_t /* hToken */, uint32_t /* hURI */>;
    tokenmap_type tokenMap;
    tokenmap_type::iterator tIter;
    handlermap_type::iterator hIter;

    std::string data;
    uint32_t hToken = 0, hURI = 0;
    mode = TOKEN | MUTABLE;

    fseek( pFile, 0, SEEK_END );
    int length = ftell( pFile );
    rewind( pFile );

    while(length)
    {
        length--;
        auto ci = fgetc( pFile );

        auto c = char(ci & 0xFF);
        switch( c )
        {
            case '#':
                if( !(mode & MUTABLE) ) { data += c; break; }
                mode = COMMENT;
                data.clear();
                hToken = hURI = 0;
                break;
            case '@':
                if( !(mode & MUTABLE) ) { data += c; break; }
                mode = TOKEN_DECL;
                data.clear();
                hToken = hURI = 0;
                break;
            case ':': // todo: allow ':' in data unit
                switch( mode & ~MUTABLE )
                {
                    case TOKEN_DECL:
                        hToken = Hash( data.c_str() );
                        data.clear();
                        mode = URI;
                        break;
                    case TOKEN:
                        hToken = Hash( data.c_str() );
                        tIter = tokenMap.find( hToken );
                        hURI = tIter == tokenMap.end() ? 0 : tIter->second;
                        data.clear();
                        mode = DATA;
                        break;
                    default:
                        if( mode & MUTABLE ) mode &= ~MUTABLE;
                        data += c;
                }
                break;
            case '\n':
                switch( mode & ~MUTABLE )
                {
                    case COMMENT:
                        break;
                    case URI:
                        tokenMap[ hToken ] = Hash( data.c_str() );
                        break;
                    case TOKEN:
                    case DATA:
                        if( data.size() == 0 ) break;
                        hIter = handlerRegistry.find( hURI );
                        if( hIter == handlerRegistry.end() ) break;
                        hIter->second( data );
                        break;
                    default:
                        assert( mode == TOKEN_DECL );
                }
                data.clear();
                hToken = hURI = 0;
                mode = TOKEN | MUTABLE;
                break;
            default:
                if( mode & MUTABLE ) mode &= ~MUTABLE;
                data += c;
        }
    }
    assert( length == 0 );

    hIter = handlerRegistry.find( hURI );
    if( data.size() > 0 )
        if (mode == DATA || mode == (TOKEN | MUTABLE))
            if( hIter != handlerRegistry.end() )
                hIter->second( data );
}

#ifdef DEBUG
void AppML::Test()
{
    const char* szTest =
        "#comment1\n"
            "@ns1:TheRainInSpain1234!\n"
            "#comment2\n"
            "something in default namespace\n"
            "\n" /* blank line is ignored? */
            "ns1:something in ns1\n"
            "ns1:final";

    FILE* pFile = fmemopen((void*)szTest, strlen(szTest), "r");

    AppML aml(pFile);
    aml.Register(
        "TheRainInSpain1234!",
        [] (const std::string& line)
        {
            assert( line.compare("something in ns1") == 0  ||
                    line.compare("final") == 0 );
        }
    );
    aml.Register(
        "",
        [] (const std::string& line)
        {
            assert( line.compare("something in default namespace") == 0 );
        }
    );
    aml.Parse();

    fclose( pFile );
}
#endif // DEBUG
