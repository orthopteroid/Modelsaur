#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <csignal>

#include <unistd.h>
#include <math.h>

#ifdef __ANDROID_API__

#else // __ANDROID_API__

#include <bits/signum.h>

#endif // __ANDROID_API__

#include "GL9.hpp"

#include "TriTools.hpp"
#include "RSphere.hpp"
#include "AppLog.hpp"

//#define HIREZ

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

void RSphere::Save(const char* szFilename)
{
    std::string f = std::string(szFilename) + ".ply";
    FILE* pFile = fopen(f.c_str(), "w");
    if(!pFile) return;

    fprintf(
        pFile,
        "ply\n"
        "format ascii 1.0\n"
        "comment Created by Blob\n"
        "element vertex %u\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property float nx\n"
        "property float ny\n"
        "property float nz\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "element face %u\n"
        "property list uchar uint vertex_indices\n"
        "end_header\n",
        posVerts.size(),
        indTriVerts.size()
    );
    for( int i=0; i<posVerts.size(); i++ ) {
        auto v = posVerts[i];
        auto n = normVerts[i];
        auto c = colorVerts[i];
        fprintf( pFile, "%f %f %f %f %f %f %d %d %d\n", v.x, v.y, v.z, n.x, n.y, n.z, int(255 * c.x), int(255 * c.y), int(255 * c.z) );
    }
    for( auto i : indTriVerts ) fprintf( pFile, "3 %d %d %d\n", i.x, i.y, i.z );

    fclose( pFile );
}
