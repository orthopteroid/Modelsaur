// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <cmath>
#include <algorithm>
#include <iostream>
#include <string.h>
#include <limits.h>

#ifdef __ANDROID_API__

#define PNG_SETJMP_SUPPORTED

#include <png.h>
#include <pngstruct.h>
#include <pngconf.h>

typedef uint32_t png_uint_32;

#else // __ANDROID_API__

#include <png.h>
#include <pngstruct.h>

#endif // __ANDROID_API__

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "AppLog.hpp"
#include "AppTexture.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

namespace AppTexture {

    GLuint LoadFile(const char* szFilename)
    {
        FILE *fp = 0;

        int bit_depth, color_type;
        uint32_t width, height;
        png_structp png_ptr = nullptr;
        png_infop info_ptr = nullptr;
        png_infop end_info = nullptr;
        png_byte *image_data = nullptr;
        png_bytep *row_pointers = nullptr;
        GLuint texture = 0;

        png_byte header[8]; //header for testing if it is a png

        bool fail = true;
        do // png-style error handling
        {
            if( !(fp = fopen( szFilename, "rb" )) ) break;
            if( !fread( header, 1, 8, fp ) ) break;

            // test if png & get pointers to data
            if( !(png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL )) ) break;
            if( !(info_ptr = png_create_info_struct( png_ptr )) ) break;
            if( !(end_info = png_create_info_struct( png_ptr )) ) break;
            //if( setjmp( png_jmpbuf( png_ptr ))) break;
            if( png_sig_cmp( header, 0, 8 ) ) break;

            // LoadFile specific
            png_init_io( png_ptr, fp ); // init png reading
            png_set_sig_bytes( png_ptr, 8 ); // let libpng know you already read the first 8 bytes

            png_read_info( png_ptr, info_ptr ); // read all the info up to the image data
            png_get_IHDR( png_ptr, info_ptr, (png_uint_32*)&width, (png_uint_32*)&height, &bit_depth, &color_type, NULL, NULL, NULL );
            png_read_update_info( png_ptr, info_ptr ); // Update the png info struct.

            uint rowbytes = png_get_rowbytes( png_ptr, info_ptr ); // get row size, allocate image and row buffers
            image_data = new png_byte[rowbytes * height];
            row_pointers = new png_bytep[height];

            // set the individual row_pointers to point at the correct offsets of image_data
            for( uint32_t i = 0; i < height; ++i ) row_pointers[height - 1 - i] = image_data + i * rowbytes;

            png_read_image( png_ptr, row_pointers ); // read the png into image_data through row_pointers

            // generate the OpenGL texture object
            gl9GenTextures( 1, &texture );
            gl9BindTexture( GL_TEXTURE_2D, texture );
            gl9TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *) image_data );
            gl9TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

            fail = false;
        }  while(false);

        if(fail)
            AppLog::Warn(__FILENAME__, "Load Failed %s", szFilename);
        else
            AppLog::Info(__FILENAME__, "Loaded %s as texture %d", szFilename, texture);

        // clean up memory and close stuff
        if(png_ptr) png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        if(image_data) delete[] image_data;
        if(row_pointers) delete[] row_pointers;
        if(fp) fclose(fp);

        return texture;
    }

    /////////////////////
    // http://pulsarengine.com/2009/01/reading-png-images-from-memory/

    typedef struct {
        const uint8_t* ptr;
        size_t len;
        size_t off;
    } user_file_t;

    static void png_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
    {
        user_file_t* f = (user_file_t*)png_ptr->io_ptr;

        if (length > f->len) {
            png_error(png_ptr, "Read Error");
            return;
        }
        memcpy(data, &f->ptr[f->off], length);
        f->len -= length;
        f->off += length;
    }

    GLuint LoadResource(Resource rez)
    {
        user_file_t f = { .ptr = rez.mStart, .len = rez.mSize, .off = 0UL, };

        int bit_depth, color_type;
        uint32_t width, height;
        png_structp png_ptr = nullptr;
        png_infop info_ptr = nullptr;
        png_infop end_info = nullptr;
        png_byte *image_data = nullptr;
        png_bytep *row_pointers = nullptr;
        GLuint texture = 0;

        bool fail = true;
        do // png-style error handling
        {
            if( !(png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL )) ) break;
            if( !(info_ptr = png_create_info_struct( png_ptr )) ) break;
            if( !(end_info = png_create_info_struct( png_ptr )) ) break;
            //if( setjmp( png_jmpbuf( png_ptr )) ) break;
            if( png_sig_cmp( (png_bytep)f.ptr, 0, 8 ) ) break; // state-free header check

            // LoadResource specific
            png_set_read_fn(png_ptr, (void *) &f, png_user_read_data); // set read function

            png_read_info( png_ptr, info_ptr ); // read all the info up to the image data
            png_get_IHDR( png_ptr, info_ptr, (png_uint_32*)&width, (png_uint_32*)&height, &bit_depth, &color_type, NULL, NULL, NULL );
            png_read_update_info( png_ptr, info_ptr ); // Update the png info struct.

            uint rowbytes = png_get_rowbytes( png_ptr, info_ptr ); // get row size, allocate image and row buffers
            image_data = new png_byte[rowbytes * height];
            row_pointers = new png_bytep[height];

            // set the individual row_pointers to point at the correct offsets of image_data
            for( uint32_t i = 0; i < height; ++i ) row_pointers[height - 1 - i] = image_data + i * rowbytes;

            png_read_image( png_ptr, row_pointers ); // read the png into image_data through row_pointers

            // generate the OpenGL texture object
            gl9GenTextures( 1, &texture );
            gl9TextureUnbinder texObject( GL_TEXTURE_2D, texture );
            gl9TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *) image_data );
            gl9TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

            fail = false;
        }  while(false);

        if(fail)
            AppLog::Warn(__FILENAME__, "Load Failed %s", rez.mName);
        else
            AppLog::Info(__FILENAME__, "Loaded %s as texture %d", rez.mName, texture);

        // clean up memory and close stuff
        if(png_ptr) png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        if(image_data) delete[] image_data;
        if(row_pointers) delete[] row_pointers;

        return texture;
    }

};
