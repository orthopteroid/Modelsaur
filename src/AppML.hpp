#ifndef _APPML_HPP_
#define _APPML_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <string>
#include <map>
#include <functional>
#include <stdint.h>

struct AppML
{
    // Namespaced data is returned line-by-line via registered handler.
    // Unnamespaced (default) data is returned via handler namespace "".
    // Comments, empty lines and incomplete ns decls are skipped.
    // #<comment>\n
    // @<ns>:<uri>\n
    // <ns>:<data>\n
    // <data>\n

    static constexpr uint32_t Hash(const char *sz) { return (*sz) ? (Hash_1(*sz, Hash(sz + 1))) : (0); }

    AppML( FILE* pFile_) : pFile(pFile_) {}
    virtual ~AppML() = default;

    FILE* pFile;

    using uriregistry_type = std::map<uint32_t /* uriHash */, std::string /* uri */>;
    uriregistry_type uriRegistry;

    using handler_type = std::function<void(const std::string& /* line */)>;
    using handlermap_type = std::map<uint32_t /* uriHash */, handler_type /* handler */>;
    handlermap_type handlerRegistry;

    // for writing
    uint32_t Register(const char *szURI);
    void Emit( uint32_t hURI, char* szData );

    // for reading
    void Register( const char* szURI, handler_type handler );
    void Parse();

#ifdef DEBUG
    static void Test();
#endif // DEBUG

private:
    static constexpr char Hash_4(char c) { return uint8_t((c >> 5) +1); } // no ctrl, printables start at +1
    static constexpr uint32_t Hash_3(char c, uint32_t h) { return (h << 4) + Hash_4(c); }
    static constexpr uint32_t Hash_2(char c, uint32_t h) { return (Hash_3(c, h) ^ ((Hash_3(c, h) & 0xF0000000) >> 23)); }
    static constexpr uint32_t Hash_1(char c, uint32_t h) { return (Hash_2(c, h) & 0x0FFFFFFF); }
};

#endif //_APPML_HPP_
