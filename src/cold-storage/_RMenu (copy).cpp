#include <cmath>
#include <algorithm>
#include <iostream>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef __ANDROID_API__

#define GLM_ENABLE_EXPERIMENTAL

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, __FILENAME__, __VA_ARGS__))
//#define LOGI(...) (0)

#else // __ANDROID_API__

#define GL_GLEXT_PROTOTYPES 1

#include <GL/gl.h>
#include <GL/glext.h>

#endif // __ANDROID_API__

#include <png.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AppTypes.hpp"
#include "RMenu.hpp"

struct glBufferUnbinder
{
    uint target, id;
    glBufferUnbinder(uint t, uint b) : target(t), id(b) { glBindBuffer(target, id); }
    ~glBufferUnbinder() { glBindBuffer(target, 0); }
};

struct glTextureUnbinder
{
    uint target, id;
    glTextureUnbinder(uint t, uint b) : target(t), id(b) {
#ifdef __ANDROID_API__
        glBindTexture(GL_TEXTURE_2D, id);
#else
        glActiveTexture(id); // todo: could be 0 or 0+GL_TEXTURE0 ?
        glEnable(GL_TEXTURE_2D);
        glBindTexture(target, id);
#endif
    }
    ~glTextureUnbinder() {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(target, 0);
    }
};

// because of some ide issue...
inline float glLength(glm::vec3 vec)
{
    return sqrt( glm::dot(vec, vec) );
}

GLuint loadTexture(const char* szFilename)
{
    FILE *fp = 0;
    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;
    png_infop end_info = nullptr;
    png_byte *image_data = nullptr;
    png_bytep *row_pointers = nullptr;
    GLuint texture = 0;

    png_byte header[8]; //header for testing if it is a png

    do // pathetic png-style error handling
    {
        if( !(fp = fopen( szFilename, "rb" )) ) break;
        if( !fread( header, 1, 8, fp ) ) break;

        //test if png & get pointers to data
        if( png_sig_cmp( header, 0, 8 ) ) break;
        if( !(png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL )) ) break;
        if( !(info_ptr = png_create_info_struct( png_ptr )) ) break;
        if( !(end_info = png_create_info_struct( png_ptr )) ) break;

        if( setjmp( png_jmpbuf( png_ptr ))) break;

        png_init_io( png_ptr, fp ); // init png reading
        png_set_sig_bytes( png_ptr, 8 ); // let libpng know you already read the first 8 bytes
        png_read_info( png_ptr, info_ptr ); // read all the info up to the image data

        // get info about png
        int bit_depth, color_type;
        int width, height;
        png_get_IHDR( png_ptr, info_ptr, (uint*)&width, (uint*)&height, &bit_depth, &color_type, NULL, NULL, NULL );

        png_read_update_info( png_ptr, info_ptr ); // Update the png info struct.

        uint rowbytes = png_get_rowbytes( png_ptr, info_ptr ); // get row size, allocate image and row buffers
        image_data = new png_byte[rowbytes * height];
        row_pointers = new png_bytep[height];

        // set the individual row_pointers to point at the correct offsets of image_data
        for( int i = 0; i < height; ++i ) row_pointers[height - 1 - i] = image_data + i * rowbytes;

        png_read_image( png_ptr, row_pointers ); // read the png into image_data through row_pointers

        // generate the OpenGL texture object
        glGenTextures( 1, &texture );
        glBindTexture( GL_TEXTURE_2D, texture );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *) image_data );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

    }  while(false);

    // clean up memory and close stuff
    if(png_ptr) png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    if(image_data) delete[] image_data;
    if(row_pointers) delete[] row_pointers;
    if(fp) fclose(fp);

    return texture;
}

void RMenu::Bind(uint8_t mt, int w, int h, float z)
{
    menuToken = mt;

    int maxExtentX = 0, maxExtentY = 0;

    for(item_type& i : menu)
    {
#if true
        i.txImage = loadTexture( std::string( i.szImage ).append( ".png" ).c_str() );
#else
        i.txImage = loadTexture( i.szImage );
        Resource menu = LOAD_RESOURCE(menu_png);
#endif
    }

#if true
    {
        Resource menu = LOAD_RESOURCE(menu_png);
        LOGI("menu size %d", menu.size());
        Resource colour = LOAD_RESOURCE(colour_png);
        LOGI("colour size %d", colour.size());
        Resource deflate = LOAD_RESOURCE(deflate_png);
        LOGI("deflate size %d", deflate.size());
        Resource inflate = LOAD_RESOURCE(inflate_png);
        LOGI("inflate size %d", inflate.size());
        Resource collision = LOAD_RESOURCE(collision_png);
        LOGI("collision size %d", collision.size());
        Resource normals = LOAD_RESOURCE(normals_png);
        LOGI("normals size %d", normals.size());
    }
#endif

    posTexVerts = { {0,0}, {0,1}, {1,1}, {1,0}, }; // CW (?)
    glGenBuffers( 1, &boTexVerts );
    glBindBuffer( GL_ARRAY_BUFFER, boTexVerts );
    glBufferData( GL_ARRAY_BUFFER, posTexVerts.size() * sizeof(glm::vec2), posTexVerts.data(), GL_STATIC_DRAW );

    posVerts = { { -.5, -.5, z }, { -.5, +.5, z }, { +.5, +.5, z }, { +.5, -.5, z }, }; // center @ 0,0,z
    glGenBuffers( 1, &boVerts );
    glBindBuffer( GL_ARRAY_BUFFER, boVerts );
    glBufferData( GL_ARRAY_BUFFER, posVerts.size() * sizeof(glm::vec3), posVerts.data(), GL_STATIC_DRAW );

    // determine ui range
    {
        int cx=-1, cy=0;
        for(item_type& i : menu)
        {
            if(i.placement == RMenu::Cat) cx++; else { cx=0; cy++; }
            maxExtentY = std::max( maxExtentY, cy );
            maxExtentX = std::max( maxExtentX, cx );
        }
        layoutDim = std::min( float(w) / float(maxExtentX +1), float(h) / float(maxExtentY +1) );
    }

    // set ui positions
    {
        int cx=-1, cy=0;
        for(item_type& i : menu)
        {
            if(i.placement == RMenu::Cat) cx++; else { cx=0; cy++; }
            ///
            i.translate = { layoutDim * (.5f + float(cx)), layoutDim * (.5f + float(cy)), 0 }; // also used as center
            i.mxPlacement = glm::scale( glm::translate( glm::mat4(), i.translate ), { layoutDim, -layoutDim, 1 } ); // -1 to flip vertical
            ///
        }
    }

    mxCorner = glm::scale( glm::translate( glm::mat4(), {cornerSize/2,cornerSize/2,0} ), { cornerSize,-cornerSize,1 } ); // -1 to flip vertical
}

void RMenu::Release()
{
    for(item_type& i : menu)
    {
        if(i.txImage) { glDeleteBuffers(1, &i.txImage); i.txImage=0; }
    }
}

void RMenu::Render()
{
    glPushMatrix();
//    glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS ); // saves pixel storage modes and vertex array state // todo: ogles
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glEnable( GL_BLEND ); // because items have alpha
    {
        // for all images, reuse the texture and vert coordinates
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        glBufferUnbinder texVerts( GL_ARRAY_BUFFER, boTexVerts );
        glTexCoordPointer( 2, GL_FLOAT, 0, nullptr );

        glEnableClientState( GL_VERTEX_ARRAY );
        glBufferUnbinder objectVerts( GL_ARRAY_BUFFER, boVerts );
        glVertexPointer( 3, GL_FLOAT, 0, nullptr );

        // for each image, place it, activate the texture unit and draw
        for(item_type &i : menu)
        {
            if(showMenu) {
                glLoadMatrixf( glm::value_ptr( i.mxPlacement ));
                glTextureUnbinder texObject( GL_TEXTURE_2D, i.txImage ); // todo: GL_TEXTURE_2D becomes uniformID?
//                glDrawArrays( GL_QUADS, 0, 4 ); // todo: ogles
            } else {
                if(menuToken != i.token) continue;
                glLoadMatrixf( glm::value_ptr( mxCorner ));
                glTextureUnbinder texObject( GL_TEXTURE_2D, i.txImage ); // todo: GL_TEXTURE_2D becomes uniformID?
//                glDrawArrays( GL_QUADS, 0, 4 ); // todo: ogles
                break;
            }
        }
    }
    glPopClientAttrib();
//    glPopAttrib(); // todo: ogles
    glPopMatrix();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

uint8_t* RMenu::Translate(int32_t x, int32_t y)
{
    if(x < cornerSize && y < cornerSize) return &menuToken;
    if(!showMenu) return nullptr;

    float innerCircleRadius = layoutDim /2;
    for(item_type& i : menu)
    {
        float dist = glLength( { i.translate.x - float(x), i.translate.y - float(y), 0 } );
        if(dist<innerCircleRadius) {
            return &i.token; // TODO: held? how to release?
        }
    }
    return nullptr;
}

void RMenu::Tick() {}
