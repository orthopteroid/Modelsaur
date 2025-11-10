# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.6)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(MY_ROOT ${CMAKE_SOURCE_DIR}/../..)

#################

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

#add_library( glue_lib STATIC
#    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
#)

#################
# ZOMG - props to OpenRCT2 on github!

include(ExternalProject)
ExternalProject_Add(png_ext
    PREFIX "${CMAKE_BINARY_DIR}/png_prefix"
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libpng-1.6.34/

    CMAKE_CACHE_ARGS
      -DCMAKE_MAKE_PROGRAM:STRING=${CMAKE_MAKE_PROGRAM}
      -DANDROID_ABI:STRING=${ANDROID_ABI}
      -DANDROID_NDK:PATH=${ANDROID_NDK}
      -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}
      -DANDROID_TOOLCHAIN:STRING=gcc
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
      -DCMAKE_BUILD_TYPE:STRING=Release
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/contrib
      -DPNG_SHARED:BOOL=OFF
      -DPNG_STATIC:BOOL=ON
      -DPNG_TESTS:BOOL=OFF

    BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/contrib/lib/${CMAKE_STATIC_LIBRARY_PREFIX}png${CMAKE_STATIC_LIBRARY_SUFFIX}

    LOG_DOWNLOAD 1
    LOG_UPDATE 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_TEST 1
    LOG_INSTALL 1
)
add_library(png_lib STATIC IMPORTED)
set_target_properties(png_lib PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/contrib/lib/${CMAKE_STATIC_LIBRARY_PREFIX}png${CMAKE_STATIC_LIBRARY_SUFFIX}
)
add_dependencies(png_lib png_ext)

#################

add_library( gif_lib STATIC
    ${MY_ROOT}/giflib-5.1.4/giflib/dgif_lib.c
    ${MY_ROOT}/giflib-5.1.4/giflib/egif_lib.c
    ${MY_ROOT}/giflib-5.1.4/giflib/gifalloc.c
    ${MY_ROOT}/giflib-5.1.4/giflib/gif_err.c
    ${MY_ROOT}/giflib-5.1.4/giflib/gif_font.c
    ${MY_ROOT}/giflib-5.1.4/giflib/gif_hash.c
    ${MY_ROOT}/giflib-5.1.4/giflib/openbsd-reallocarray.c
    ${MY_ROOT}/giflib-5.1.4/giflib/quantize.c
)

target_compile_definitions( gif_lib PRIVATE S_IREAD=0400 S_IWRITE=0200 )

set_target_properties( gif_lib PROPERTIES LINKER_LANGUAGE C )

#################

add_library( blob_app SHARED
    ${MY_ROOT}/src/App.cpp
    ${MY_ROOT}/src/AppKeyboard.cpp
    ${MY_ROOT}/src/AppNormalBrusher.cpp
    ${MY_ROOT}/src/AppTexture.cpp
    ${MY_ROOT}/src/AppTime.cpp
    ${MY_ROOT}/src/AppML.cpp
    ${MY_ROOT}/src/AppTutorial.cpp
    ${MY_ROOT}/src/AppTriBrusher.cpp
    ${MY_ROOT}/src/TriTools.cpp
    ${MY_ROOT}/src/CRubus.cpp
    ${MY_ROOT}/src/RIcosahedron.cpp
    ${MY_ROOT}/src/RMenu.cpp
    ${MY_ROOT}/src/RText.cpp
    ${MY_ROOT}/src/RSphere.cpp
    ${MY_ROOT}/src/RTetrahedron.cpp
    ${MY_ROOT}/src/RSimpleTri.cpp
    ${MY_ROOT}/src/RColorPicker.cpp
    ${MY_ROOT}/src/GL9Common.cpp

    ${MY_ROOT}/src/Android/PlatformFile.cpp
    ${MY_ROOT}/src/Android/PlatformLog.cpp
    ${MY_ROOT}/src/Android/Platform.cpp
    ${MY_ROOT}/src/Android/fmemopen.cpp
    ${MY_ROOT}/src/Android/GL9GLES2.cpp
    ${MY_ROOT}/src/Android/android_native_app_glue.c
)

set_target_properties( blob_app PROPERTIES LINKER_LANGUAGE CXX )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( log_lib log )

find_package( ZLIB REQUIRED )

####################

# inspired by:
# https://beesbuzz.biz/blog/e/2014/07/31-embedding_binary_resources_with_cmake_and_c11.php
# resource names must be unique
FUNCTION(ADD_RESOURCES out_var)
    MAKE_DIRECTORY( ${PROJECT_BINARY_DIR}/rez )
    SET(out_list)
    FOREACH(in_arg ${ARGN})
        GET_FILENAME_COMPONENT(in_p ${in_arg} DIRECTORY)
        GET_FILENAME_COMPONENT(in_f ${in_arg} NAME)
        SET( out_pf ${PROJECT_BINARY_DIR}/rez/${in_f}.o )
        MESSAGE("Packaging resource " ${out_pf})
        EXECUTE_PROCESS(
            COMMAND ${ANDROID_TOOLCHAIN_PREFIX}ld --format=binary -relocatable --output=${out_pf} ${in_f}
            WORKING_DIRECTORY ${in_p}
        )
        SET(out_list "${out_list};${out_pf}")
    ENDFOREACH()
    SET(${out_var} "${out_list}" PARENT_SCOPE)
ENDFUNCTION()

ADD_RESOURCES( rez_files
    ${MY_ROOT}/png/menu.png
    ${MY_ROOT}/png/menu_colorpicker.png
    ${MY_ROOT}/png/menu_littletool.png
    ${MY_ROOT}/png/menu_bigtool.png
    ${MY_ROOT}/png/menu_gianttool.png
    ${MY_ROOT}/png/menu_deflate.png
    ${MY_ROOT}/png/menu_inflate.png
    ${MY_ROOT}/png/menu_rot_z.png
    ${MY_ROOT}/png/menu_rot_y.png
    ${MY_ROOT}/png/menu_undo.png
    ${MY_ROOT}/png/menu_handle.png
    ${MY_ROOT}/png/menu_save.png
    ${MY_ROOT}/png/menu_trash.png
    ${MY_ROOT}/png/menu_privacy.png
    ${MY_ROOT}/png/menu_cheat.png

    ${MY_ROOT}/png/dialog_colorpicker.png
)

############

target_link_libraries( blob_app
    android
    gif_lib
    png_lib
    EGL
    GLESv2
    ${rez_files}
    log
    ${ZLIB_LIBRARIES}
)

target_include_directories( blob_app PRIVATE
    ${MY_ROOT}/src
    ${MY_ROOT}/src/Android
	${MY_ROOT}/glm-0.9.7.6
    ${MY_ROOT}/giflib-5.1.4
    ${ZLIB_INCLUDE_DIRS}
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_SOURCE_DIR}/libpng-1.6.34
)
