// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include <deque>
#include <map>
#include <list>

#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
//#include <linux/input.h>
//#include <mtdev.h>
//#include <mtdev-plumbing.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

//#include <X11/X.h>
//#include <X11/Xlib.h>
//#include <X11/XKBlib.h>

#define PNG_SETJMP_SUPPORTED

#include <png.h>
#include <pngconf.h>

#define GLX_GLXEXT_PROTOTYPES

#include <giflib/gif_lib.h>

#include <GLES2/gl2.h> // for glGenRenderbuffers and glGenFramebuffers, etc

#include "GL9.hpp"

#include "AppPlatform.hpp"
#include "AppLog.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

////////////////////////

struct OSB565
{
    GLuint rbo, dbo, fbo;
    int w, h;
    void* pixels;

    OSB565(int w_, int h_, void* pixels_)
    {
        w = w_; h = h_;
        pixels = pixels_;

        glGenRenderbuffers(1,&rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, w, h);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        glGenRenderbuffers(1,&dbo);
        glBindRenderbuffer(GL_RENDERBUFFER, dbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        glGenFramebuffers(1,&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }

    virtual ~OSB565()
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteFramebuffers(1,&fbo);
        glDeleteRenderbuffers(1,&rbo);
        glDeleteRenderbuffers(1,&dbo);
    }

    void BeginFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable( GL_DEPTH_TEST );

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }

    void EndFrame()
    {
        glFinish();

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        ::glReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels );

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif
    }
};

///////////////////////

void gl9RenderPNG(const char *szFilename, int w, int h, std::function<void(void)> fnRender)
{
    struct pxFormat888 {
        uint8_t r, g, b;
    };
    struct pxFormat565 {
        union {
            uint16_t u;
            struct {
                uint16_t b : 5;
                uint16_t g : 6;
                uint16_t r : 5;
            };
        };
        inline operator pxFormat888() // interger math conversion
        {
            pxFormat888 px;
            px.r = uint8_t(uint(255 * uint(r)) >> 5);
            px.g = uint8_t(uint(255 * uint(g)) >> 6);
            px.b = uint8_t(uint(255 * uint(b)) >> 5);
            return px;
        }
    };

    //////////////

    std::string filename = std::string(Platform_ExternalPath());
    filename.append("/").append(szFilename).append(".png");

    std::unique_ptr<pxFormat565[]> pxData565( new pxFormat565[w * h] );
    std::unique_ptr<pxFormat888[]> pxData888( new pxFormat888[w * h] );
    std::unique_ptr<png_bytep[]> rows( new png_bytep[h] );

    for (int y = 0; y < h; ++y) rows[y] = (png_bytep)&pxData888[ ((h-1) - y) * w ];

    OSB565 osb(w, h, pxData565.get());
    osb.BeginFrame();

    fnRender();

#ifdef DEBUG
    { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

    osb.EndFrame();

    for(int i=0; i<w*h; i++) pxData888[ i ] = pxData565[ i ]; // convert frame

    FILE *fp = 0;
    png_structp png_ptr = 0;
    png_infop info_ptr = 0;

    AppLog::Info(__FILENAME__, "%s opening %s for write", __func__, filename.c_str() );
    do{
        if( !(fp = fopen(filename.c_str(), "wb")) ) break;
        if( !(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) ) break;
        if( !(info_ptr = png_create_info_struct(png_ptr)) ) break;

        png_init_io(png_ptr, fp);
        png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png_ptr, info_ptr);
        png_set_packing(png_ptr);
        png_write_image(png_ptr, rows.get());
        png_write_end(png_ptr, info_ptr);
    } while(false);

    if(png_ptr) png_free(png_ptr, 0);
    if(info_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
    if(fp) fclose(fp);
}

void gl9RenderGIF( const char *szFilename, int w, int h, std::function<void(void)> fnRender, const std::vector<glm::vec3> &colorVerts, int frames, float fps )
{

    struct GifFile
    {
        int handle;
        GifFileType* f;

        GifFile(const std::string& path)
        {
            handle = open( path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE );
            f = (handle != -1) ? EGifOpenFileHandle(handle, nullptr) : nullptr;
        }
        virtual ~GifFile()
        {
            if( f != nullptr ) EGifCloseFile(f, nullptr);
            if( handle != -1 ) close(handle); // todo: check
        }
        bool Bad() const { return f == nullptr; }
        operator GifFileType*() { return f; }
    };

    struct pxFormat565 {
        union {
            uint16_t u;
            struct {
                uint16_t b : 5;
                uint16_t g : 6;
                uint16_t r : 5;
            };
        };
        inline uint8_t index233() const
        {
            return uint8_t(
                (( b << 3 ) & 0b11100000 ) | // put top 3 bits of 5 at top of index
                (( g >> 1 ) & 0b00011100 ) | // put top 3 bits of 6 across nibble of index
                ( r >> 3 ) // put top 2 bits of 5 at bottom of index
            );
        }
        inline static GifColorType palette233(uint16_t i)
        {
            // index is bgr332/rgb233
            GifColorType col =
                {
                    uint8_t((255 >> 2) * uint8_t(i & 0b00000011)), // bottom 2 bits are red * 4 steps of red
                    uint8_t((255 >> 3) * uint8_t((i >> 2) & 0b00000111)), // 3 bits across nibble are green * 7 steps green
                    uint8_t((255 >> 3) * uint8_t(i >> 5)) // top 3 bits are blue * 7 steps blue
                };
            return col;
        }
    };

    //////////////

    std::string filename = std::string(Platform_ExternalPath());
    filename.append("/").append(szFilename).append(".gif");

    GifFile gifFile( filename.c_str() );
    if( gifFile.Bad() ) return;

    auto frameDelay = std::max<int>(1,int(100.f / fps));

    std::unique_ptr<pxFormat565[]> pxData565( new pxFormat565[w * h] );
    std::unique_ptr<uint8_t[]> pxDataIdx( new uint8_t[w * h] );
    std::unique_ptr<GifColorType[]> palette( new GifColorType[256] );

    int gifIdxTrans = 0 /* NO_TRANSPARENT_COLOR */; // black is transparent

    for(uint16_t i=0; i<256; i++) palette[ i ] = pxFormat565::palette233( i );

    ColorMapObject colorMap = { 256, 8, false, palette.get() };

    OSB565 osb(w, h, pxData565.get());

    EGifSetGifVersion(gifFile, true); // GIF89, for animation and transparency support
    int err = EGifPutScreenDesc( gifFile, w, h, colorMap.BitsPerPixel, gifIdxTrans, &colorMap );
    assert(err != GIF_ERROR);

    bool multiframe = frames > 1;
    while(frames--)
    {
        osb.BeginFrame();

        fnRender();

#ifdef DEBUG
        { auto err = glGetError(); assert(err == GL_NO_ERROR); }
#endif

        osb.EndFrame();

        for(int i=0; i<w*h; i++) pxDataIdx[ i ] = pxData565[i].index233(); // convert

        if( multiframe ) // Netscape loop extension
        {
            EGifPutExtensionLeader(gifFile, APPLICATION_EXT_FUNC_CODE);
            EGifPutExtensionBlock(gifFile, 11, "NETSCAPE2.0");
            EGifPutExtensionBlock(gifFile, 3, "\x01" "\x00" "\x00");
            EGifPutExtensionTrailer(gifFile);
        }

        GifByteType extArr[4];
        GraphicsControlBlock gcb = { DISPOSE_BACKGROUND, false, frameDelay, gifIdxTrans };
        EGifGCBToExtension( &gcb, extArr );
        EGifPutExtension( gifFile, GRAPHICS_EXT_FUNC_CODE, 4, extArr );

        err = EGifPutImageDesc( gifFile, 0, 0, w, h, false, nullptr );
        assert(err != GIF_ERROR);

        for (int y = 0; y < h; y++)
            EGifPutLine(gifFile, &pxDataIdx[ ((h-1) - y) * w ], w);
    }
}
