# Copyright 2025 orthopteroid@gmail.com, MIT License

CMAKE_MINIMUM_REQUIRED( VERSION 3.8 )

SET( CMAKE_VERBOSE_MAKEFILE ON )

LIST( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake-modules" )

SET( CMAKE_CXX_STANDARD 11 )

SET( MY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. )

SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
IF( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    SET( CMAKE_EXECUTABLE_SUFFIX_CXX "-debug" )
ENDIF()

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Wall" )
SET( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g" )
SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-exceptions -s" )

# inspired by:
# https://beesbuzz.biz/blog/e/2014/07/31-embedding_binary_resources_with_cmake_and_c11.php
# resource names must be unique
FUNCTION( ADD_RESOURCES out_var )
    SET( out_p ${CMAKE_BINARY_DIR}/rez )
    SET( out_list )
    MAKE_DIRECTORY( ${out_p} )
    FOREACH( in_arg ${ARGN} )
        GET_FILENAME_COMPONENT( in_p ${in_arg} DIRECTORY )
        GET_FILENAME_COMPONENT( in_f ${in_arg} NAME )
        SET( out_pf ${out_p}/${in_f}.o )
        MESSAGE( "Packaging " ${in_p}/${in_f} " -> " ${out_pf} )
        EXECUTE_PROCESS(
            COMMAND /usr/bin/ld --format=binary -r --output=${out_pf} ${in_f}
            WORKING_DIRECTORY ${in_p}
        )
        SET( out_list "${out_list};${out_pf}" )
    ENDFOREACH()
    SET( ${out_var} "${out_list}" PARENT_SCOPE )
ENDFUNCTION()

ADD_RESOURCES( MY_REZ
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

    ${MY_ROOT}/png/menu_collision.png
    ${MY_ROOT}/png/menu_normals.png

    ${MY_ROOT}/png/dialog_colorpicker.png
)

SET( MY_SRC
    ${MY_ROOT}/src/App.cpp
    ${MY_ROOT}/src/AppKeyboard.cpp
    ${MY_ROOT}/src/AppNormalBrusher.cpp
    ${MY_ROOT}/src/AppTriBrusher.cpp
    ${MY_ROOT}/src/AppTexture.cpp
    ${MY_ROOT}/src/AppTime.cpp
    ${MY_ROOT}/src/AppML.cpp
    ${MY_ROOT}/src/AppTutorial.cpp
    ${MY_ROOT}/src/CRubus.cpp
    ${MY_ROOT}/src/RIcosahedron.cpp
    ${MY_ROOT}/src/RMenu.cpp
    ${MY_ROOT}/src/RColorPicker.cpp
    ${MY_ROOT}/src/RSphere.cpp
    ${MY_ROOT}/src/RText.cpp
    ${MY_ROOT}/src/RTetrahedron.cpp
    ${MY_ROOT}/src/RSimpleTri.cpp
    ${MY_ROOT}/src/TriTools.cpp
    
    ${MY_ROOT}/src/GL9Common.cpp
)

############

PROJECT( appOGL1 CXX )
FIND_PACKAGE( OpenGL REQUIRED )
FIND_PACKAGE( GLUT REQUIRED )
FIND_PACKAGE( X11 REQUIRED )
INCLUDE_DIRECTORIES(
    ${MY_ROOT}/src
    ${MY_ROOT}/glm-0.9.7.6
    ${MY_ROOT}/giflib-5.1.4
    ${MY_ROOT}/libpng-1.6.26
    ${MY_ROOT}/src/Linux
    ${OPENGL_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
ADD_EXECUTABLE( appOGL1
    ${MY_SRC}

    ${MY_ROOT}/src/Linux/Platform.cpp
    ${MY_ROOT}/src/Linux/GL9OGL1.cpp
    ${MY_ROOT}/src/Linux/PlatformLog.cpp
    ${MY_ROOT}/src/Linux/PlatformFile.cpp
)
TARGET_COMPILE_OPTIONS( appOGL1 PRIVATE -DOGL1 -DPLATFORM=Linux ) # nb: special ogl1 build
TARGET_LINK_LIBRARIES( appOGL1
    ${MY_REZ}
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARY}
    ${X11_LIBRARIES}
    libmtdev.so
    libpng.so
    libgif.so
)

############

PROJECT( appGLES2 CXX )
FIND_PACKAGE( OpenGL REQUIRED )
FIND_PACKAGE( GLUT REQUIRED )
FIND_PACKAGE( X11 REQUIRED )
INCLUDE_DIRECTORIES(
    ${MY_ROOT}/src
    ${MY_ROOT}/src/Linux
    ${MY_ROOT}/glm-0.9.7.6
    ${MY_ROOT}/giflib-5.1.4
    ${OPENGL_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
ADD_EXECUTABLE( appGLES2
    ${MY_SRC}

    ${MY_ROOT}/src/Linux/Platform.cpp
    ${MY_ROOT}/src/Linux/GL9GLES2.cpp
    ${MY_ROOT}/src/Linux/PlatformLog.cpp
    ${MY_ROOT}/src/Linux/PlatformFile.cpp
)
TARGET_COMPILE_OPTIONS( appGLES2 PRIVATE -DPLATFORM=Linux )
TARGET_LINK_LIBRARIES( appGLES2
    ${MY_REZ}
    ${GLUT_LIBRARY}
    ${X11_LIBRARIES}
    libmtdev.so
    libpng.so
    libgif.so
    libEGL.so
    libGLESv2.so
)
