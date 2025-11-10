// Copyright 2025 orthopteroid@gmail.com, MIT License

#include <unistd.h>
#include <math.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <ctime>
#include <set>
#include <memory>
#include <string.h>
#include <sstream>

#include "GL9.hpp"

#include "AppTypes.hpp"
#include "TriTools.hpp"
#include "AppPlatform.hpp"
#include "AppTime.hpp"
#include "AppTutorial.hpp"
#include "AppLog.hpp"
#include "AppTriBrusher.hpp"
#include "AppNormalBrusher.hpp"
#include "AppKeyboard.hpp"
#include "AppTexture.hpp"
#include "AppFile.hpp"
#include "AppML.hpp"

#include "RSphere.hpp"
#include "RIcosahedron.hpp"
#include "RMenu.hpp"
#include "RText.hpp"
#include "RColorPicker.hpp"
#include "RSimpleTri.hpp"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define MODEL sphere

// https://en.wikipedia.org/wiki/Vertex_Buffer_Object#In_C.2C_using_OpenGL_2.1

// https://daler.github.io/blender-for-3d-printing/printing/export-stl.html

// glxinfo | grep version

//#define DEBUG_BRUSH_ENDBIN
//#define DEBUG_TWOFINGERMODE
//#define DEBUG_RENDER

#define ENABLE_SAVE_MODEL

#define RndColour ((float)rand() / (float)RAND_MAX)

const char* Platform_InternalPath();
const char* Platform_ExternalPath();

///////////////

// because of some ide issue...
inline float glLength(const glm::vec2& vec) { return sqrt( glm::dot(vec, vec) ); }
inline float glLength(const glm::vec3& vec) { return sqrt( glm::dot(vec, vec) ); }

// Ericson, p130
inline float glSegmentPointDistanceSq(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, bc = c - b;
    float e = glm::dot(ac, ab);
    if(e <= 0.f) return glm::dot(ac, ac);
    float f = glm::dot(ab, ab);
    if( e >= f ) return glm::dot(bc, bc);
    return glm::dot(ac, ac) - e * e / f;
}

struct Touch {
    // use a tri-state to process an ending-touch-position on the 'active' codepath
    // but ensure that that state reverts to 'false' on next tick.
    enum { False = 0, True, Ending } active = False;
    glm::vec3 pos;

    bool IsActive() { return active != False; }
};

struct TwoFinger
{
    float thetaSum = 0;
    float distSum = 0;
    float dZ, dR;

    void Reset()
    {
        thetaSum = distSum = 0;
    }

    // 2D angle between points
    float AngleOf(const glm::vec3 &pos0, const glm::vec3 &pos1)
    {
        return std::atan2( pos1.y - pos0.y, pos0.x - pos1.x ); // nb: y inversion
    }

    void CalcRotZoom(const glm::vec3 &t0, const glm::vec3 &t1, const glm::vec3 &lt0, const glm::vec3 &lt1 )
    {
        dR = AngleOf( t0, t1 ) - AngleOf( lt0, lt1 );
        if( dR > float( M_PI )) dR -= 2.f * float( M_PI ); // cw wrap
        else if( dR < -float( M_PI )) dR += 2.f * float( M_PI ); // ccw wrap

        dZ = glLength( t0 - t1 ) - glLength( lt0 - lt1 );
    }
};

// perf tune in conjunction with
// - model chicklet-size
// - model buffer-update strategy
// - target hardware
const uint kStrokeTune = 200;

// must be reset on app-entry as well as here
bool appQuit = false;
bool appPause = false;

AppPlatform platform;
AppKeyboard keyboard;

const int kMenuTimeout = 5000;
const int kDialogColumns = 30;

// Z is in units of relative depth: 0=near-plane, 1=far-plane
const float NearplaneZ( 0 );

const glm::vec3 AxisUp = { 0, 1, 0 };
const glm::vec3 AxisRight = { 1, 0, 0 };
const glm::vec3 PosCamera = { 0, 0, 10 };
const glm::vec3 PosLight = { 5, 5, 5 };

// view the origin from the camera
glm::vec3 posOrigin = glm::vec3( 0, 0, 0 );
glm::vec3 axisUp, axisRight, posCamera, posLight;
glm::mat4 mxView;
glm::mat4 mxProj;
glm::mat4 mxOrtho;
glm::vec4 boxViewport;

glm::vec2 sizDepthRange = { PosCamera.z * .5f, PosCamera.z * 2.f };
float degreeFOV = 30.f;
float whRatio = 1; // set in main after display binds

void cameraReset() {
    axisUp = AxisUp;
    axisRight = AxisRight;
    posCamera = PosCamera;
    posLight = PosLight;
    mxView = glm::lookAt( posCamera, posOrigin, axisUp );
    degreeFOV = 30.f;
    mxProj = glm::perspective( glm::radians( degreeFOV ), whRatio, sizDepthRange.x, sizDepthRange.y );
};

glm::vec3 touchLastPos[AppPlatform::Event::MaxTouch];
Touch touch[AppPlatform::Event::MaxTouch];

TwoFinger twoFinger;
RSimpleTri cursor[2];
RSphere sphere;
RMenu menu;
std::deque< std::unique_ptr<RText> > dialogStack;
RColorPicker colorPicker;
AppTriBrusher triBrusher;
AppNormalBrusher normalBrusher;

glm::vec3 paintColor;
glm::vec3 backColor = {0,0,0};
const glm::vec3 menuColor = {1,1,1};
const glm::vec3 cursorColor = {1,1,1};

enum : uint8_t {
    tokenScroll = 'q',
    tokenStroke = 's',
    tokenCloseTextDialog = '%',
    tokenPickAndCloseDialog = ';'
};

float uiDur = 0;
uint32_t uiInactiveElapsedMSec = 0;
bool uiActive = false;
int32_t platWidth, platHeight;

enum tool_type { NoTool, TriTool, SmallTool, BigTool };
tool_type toolType = NoTool;

enum tool_mode { ColorMode, InflateMode, DeflateMode, HandleMode };
tool_mode toolMode = ColorMode;

uint32_t donationKey = 0;

//////////////////////////////

glm::vec3 fnUnproject(glm::vec3 pos2d)
{
    glm::vec3 posRevY( pos2d.x, platHeight - pos2d.y, pos2d.z );
    auto projectedPoint = glm::unProject( posRevY, mxView, mxProj, boxViewport ); // on near plane
    return projectedPoint;
}

glm::vec3 fnProject(glm::vec3 pos3d)
{
    auto projectedPoint = glm::project( pos3d, mxView, mxProj, boxViewport ); // on near plane
    return projectedPoint;
}

void fnPaintColor(triID_type triID, float patchEffect, float handleEffect)
{
//    MODEL.BrushColor( triID, paintColor, 1.0f );
    MODEL.BrushColor( triID, paintColor, patchEffect ); // hack?
}

void fnPaintInflate(triID_type triID, float patchEffect, float handleEffect)
{
    MODEL.BrushPos( triID, +MODEL.normTris[triID], .05f * patchEffect );
    normalBrusher.Continue( triID );
}

void fnPaintDeflate(triID_type triID, float patchEffect, float handleEffect)
{
    MODEL.BrushPos( triID, -MODEL.normTris[triID], .05f * patchEffect );
    normalBrusher.Continue( triID );
}

void fnPaintHandle(triID_type triID, float patchEffect, float handleEffect)
{
    MODEL.BrushPos( triID, MODEL.normTris[triID], .01f * handleEffect * patchEffect );
    normalBrusher.Continue( triID );
}

struct PaintEffectorState
{
    serial_type serial = 0;
    ind3_type indTri_coll;
    glm::vec3 center_coll;
    glm::vec3 root;
};
static PaintEffectorState pfe;

std::pair<bool, float> fnPaintEffector(triID_type t)
{
    const float a_third( 1.f / 3.f );
    const std::vector<glm::vec3>& posVerts = MODEL.posVerts;

    if( pfe.serial != triBrusher.adjSerial )
    {
        pfe.serial = triBrusher.adjSerial;
        pfe.indTri_coll = MODEL.indTriVerts[triBrusher.searchCxt.collisionTri];
        pfe.center_coll = ( posVerts[pfe.indTri_coll.x] + posVerts[pfe.indTri_coll.y] + posVerts[pfe.indTri_coll.z] ) * a_third;
        pfe.root = pfe.center_coll - MODEL.normTris[triBrusher.searchCxt.collisionTri] * triBrusher.patchSize;
    }

    if( triBrusher.searchCxt.collisionTri == t )
        return std::make_pair<bool, float>(true, .5f); // hack to prevent nipple

    const ind3_type indTri = MODEL.indTriVerts[ t ];
    const glm::vec3 center = ( posVerts[ indTri.x ] + posVerts[ indTri.y ] + posVerts[ indTri.z ] ) * a_third;

    const float dist = std::sqrt( glSegmentPointDistanceSq( pfe.root, pfe.center_coll, center ) );

    const float effect = 1.f - dist / triBrusher.patchSize;
    return std::make_pair<bool, float>(dist <= triBrusher.patchSize, effect +0); // duh, +0 to make r-value
};

/////////////////

void AppDialog( char d )
{
    AppTutorial::TestAndQueue(
        d,
        [&] (const char* sz) {
            dialogStack.push_back( RText::Factory( kDialogColumns, sz ) );
            dialogStack.back()->Bind(platWidth, platHeight);
        }
    );
}

/////////////////

#define SAVE 0
#define LOAD 1
bool AppSerialize(int op)
{
    AppFile prefs( "config", AppFile::Preferences, op == SAVE ? AppFile::WriteMode : AppFile::ReadMode );
    if( !prefs.pFile )
        return false;

    AppML aml( prefs.pFile );

    if(op == SAVE)
    {
        aml.Emit(aml.Register(AppTutorial::VERSION_01), AppTutorial::SaveMarkings_01().get() );
    }
    else
    {
        aml.Register(
            AppTutorial::VERSION_01,
            [](const std::string& line) { AppTutorial::RestoreMarkings_01( line.c_str() ); }
        );
        aml.Parse();
    }
    return true;
}

void AppRenderToFile()
{
    std::string filename = AppTimeCode32();

    std::string message( "Saved image" );

#if defined(ENABLE_SAVE_MODEL)
    message += " and 3D model files";

    MODEL.SavePLY( filename.c_str());
    MODEL.SaveSTL( filename.c_str());
#endif // ENABLE_SAVE_MODEL

    message.append(" with name ");
    message.append( filename );
    message.append(" to your storage folder.");

    const int w = 150, h = 100;

    // png
    gl9Viewport( 0, 0, w, h ); // set for export
    gl9ClearColor3fv(glm::value_ptr(backColor));
    gl9UseProgram( GL9_WORLD );
    gl9MatrixMode( GL_PROJECTION );
    gl9LoadMatrixf( glm::value_ptr( mxProj ));
    gl9MatrixMode( GL_MODELVIEW );
    gl9LoadMatrixf( glm::value_ptr( mxView ));
    gl9LoadLightf( glm::value_ptr( posLight ) );
    gl9RenderPNG( filename.c_str(), w, h, [&]() { MODEL.Render(); } );

    // rotating gif
    {
        auto axisUp_ = axisUp;
        auto posCamera_ = posCamera;
        auto posLight_ = posLight;
        auto mxView_ = mxView;

        const auto nFrames = 360 / 4; // every 4'
        const float rotRate = float( 2. * M_PI ) / float( nFrames );

        gl9RenderGIF(
            filename.c_str(),
            w, h,
            [&]() {
                gl9MatrixMode( GL_MODELVIEW );
                gl9LoadMatrixf( glm::value_ptr( mxView_ ));
                gl9LoadLightf( glm::value_ptr( posLight_ ) );

                MODEL.Render();

                const glm::quat quat = glm::angleAxis( rotRate, axisUp );
                axisUp_ = glm::normalize( quat * axisUp_ * glm::conjugate( quat )); // renorm axis
                posCamera_ = quat * posCamera_ * glm::conjugate( quat );
                posLight_ = quat * posLight_ * glm::conjugate( quat );
                mxView_ = glm::lookAt( posCamera_, posOrigin, axisUp_ );
            },
            MODEL.colorVerts,
            nFrames, float(nFrames) / 4.f // 4 secs
        );

    }

    gl9Viewport( 0, 0, platWidth, platHeight ); // reset

    ///////////////////////////

    dialogStack.push_back( RText::Factory( kDialogColumns, message ) );
    dialogStack.back()->Bind(platWidth, platHeight);
};

//////////////////////////////

void AppLogic(uint32_t deltaMSec)
{
    AppScopeTime st( uiDur );

    menu.visible = keyboard.Check( 'm', AppKeyboard::Press );
    appQuit = appQuit | keyboard.Check( 27, AppKeyboard::Press );

    // anything fresh toggles the dialog
    if(keyboard.Check( 'w', AppKeyboard::Fresh )) colorPicker.visible = !colorPicker.visible;

    if( keyboard.Check( 'Z', AppKeyboard::Fresh ) ) { MODEL.Reset(); MODEL.UpdateAllStates(); MODEL.rubus.Reset(); cameraReset(); }
    if( keyboard.Check( 0x08, AppKeyboard::Fresh ) ) { MODEL.Reset(true); MODEL.UpdateAllStates(); MODEL.rubus.Reset(); }

    if( keyboard.Check( 'B', AppKeyboard::Fresh ) ) { backColor = paintColor; } // todo: add ui button

    /////////////////// modal text dialogs

    if(keyboard.activekeys.size() > 0)
    {
        char token = keyboard.activekeys.front();
        if( dialogStack.size() > 0 && keyboard.Check( tokenCloseTextDialog, AppKeyboard::Fresh ) )
        {
            dialogStack.pop_front(); // uniqueptr dtor
            if( !AppSerialize( SAVE ) )
                AppDialog( '^' );
        }
        else if( keyboard.Check( token, AppKeyboard::Fresh ) )
        {
            // handle any key-specific dialogs
            AppDialog( token );
        }
    }

    /////////////////// cheats

    if( keyboard.Check( '!', AppKeyboard::Fresh ) ) { RSphere::CheatSphereOnly = false; }

    /////////////////// button actions

    if( keyboard.Check( 'S', AppKeyboard::Fresh ) ) AppRenderToFile();

    /////////////////// move the camera

    // only move when there is no stroking
    if(keyboard.Check( tokenStroke, AppKeyboard::Release ))
    {
        glm::vec3 deltaRot = {0, 0, 0};
        float deltaZoom = 0;

        // looking down axis to origin... angle decreases are cw, angle increases are ccw
        const float rotStep = float( deltaMSec ) * .001f * float( 2 * M_PI ) / 10.f; // tuned for phone...
        if( keyboard.Check( 'i', AppKeyboard::Press )) deltaRot.y = -rotStep;
        if( keyboard.Check( 'k', AppKeyboard::Press )) deltaRot.y = +rotStep;
        if( keyboard.Check( 'j', AppKeyboard::Press )) deltaRot.x = -rotStep;
        if( keyboard.Check( 'l', AppKeyboard::Press )) deltaRot.x = +rotStep;
        if( keyboard.Check( 'I', AppKeyboard::Press )) deltaRot.z = -rotStep;
        if( keyboard.Check( 'K', AppKeyboard::Press )) deltaRot.z = +rotStep;

        const glm::vec3 _180dPerScreen = float( M_PI ) / glm::vec3( platWidth, platHeight, 1.f );
        if( touch[0].IsActive() && touch[1].IsActive())
        {
            twoFinger.CalcRotZoom(touch[0].pos, touch[1].pos, touchLastPos[0], touchLastPos[1]);

            deltaRot.z = twoFinger.dR;
            deltaZoom = std::fabs( twoFinger.dZ ) <= 100 * std::numeric_limits<float>::min() ? 0 : ( twoFinger.dZ < 0 ? 1.02f : .98f );

            // undo the change when out of range
            if( degreeFOV * deltaZoom < 10.f || 50.f < degreeFOV * deltaZoom ) deltaZoom = 0;
        } else if( touch[0].IsActive() && !touch[1].IsActive())
        {
            // single finger: only finger 0 is active
            if( keyboard.Check( tokenScroll, AppKeyboard::Press ))
            {
                deltaRot = ( touch[0].pos - touchLastPos[0] ) * _180dPerScreen; // quat rotate using x,y
                deltaRot.z = 0;
            }
        }

        if( glLength( deltaRot ) > std::numeric_limits<float>::min())
        {
            // determine delta-rotation quat around current up and right axis
            // update the axis and camera orientations with the delta-rotation quat
            // I think -ve is required as the camera, not the object, is moving. (?)
            const glm::vec3 axisOut = glm::normalize( posOrigin - posCamera );
            const glm::quat quat = glm::angleAxis( -deltaRot.y, axisRight ) * glm::angleAxis( -deltaRot.x, axisUp ) *
                                   glm::angleAxis( deltaRot.z, axisOut );
            axisRight = glm::normalize( quat * axisRight * glm::conjugate( quat )); // renorm axis
            axisUp = glm::normalize( quat * axisUp * glm::conjugate( quat )); // renorm axis
            posCamera = quat * posCamera * glm::conjugate( quat );
            posLight = quat * posLight * glm::conjugate( quat );
            mxView = glm::lookAt( posCamera, posOrigin, axisUp );
        }

        if( std::fabs( deltaZoom ) > std::numeric_limits<float>::min())
        {
            degreeFOV *= deltaZoom;
            mxProj = glm::perspective( glm::radians( degreeFOV ), whRatio, sizDepthRange.x, sizDepthRange.y );
        }
    }

    /////////////////// apply a modelling tool

    if( keyboard.Check( 't', AppKeyboard::Fresh )) { toolMode = HandleMode; }
    else if( keyboard.Check( 'e', AppKeyboard::Fresh )) { toolMode = InflateMode; }
    else if( keyboard.Check( 'r', AppKeyboard::Fresh )) { toolMode = DeflateMode; }
    else if( keyboard.Check( 'w', AppKeyboard::Fresh )) { toolMode = ColorMode; }
    else if( keyboard.Check( 'W', AppKeyboard::Fresh )) { toolType = TriTool; }
    else if( keyboard.Check( 'E', AppKeyboard::Fresh )) { toolType = SmallTool; }
    else if( keyboard.Check( 'R', AppKeyboard::Fresh )) { toolType = BigTool; }

    if(keyboard.Check( tokenStroke, AppKeyboard::Fresh ))
    {
        if( keyboard.Check( tokenStroke, AppKeyboard::Press ))
        {
            float patchSize = 0; // TriTool
            if( toolType == SmallTool )     patchSize = 3.1415f / 8; // radians
            else if( toolType == BigTool )  patchSize = 3.1415f / 4; // radians

            triBrusher.Start( touch[0].pos, posCamera, &fnProject, &fnUnproject, &fnPaintEffector, patchSize );

            MODEL.Backup();
            std::generate( MODEL.normEffectVerts.begin(), MODEL.normEffectVerts.end(), []() { return 0.f; } );
        }
        else // if( keyboard.Check( tokenStroke, AppKeyboard::Release ))
        {
            triBrusher.Stop(); // stop on release as slow hardware causes problems

            // detailed normal adjustment...
            if( toolMode != ColorMode )
            {
                normalBrusher.ReStrokeObject();
                MODEL.rubus.Reset();
                MODEL.UpdatePosFinalize();
                MODEL.UpdateNormalFinalize();
            } else {
                MODEL.UpdateColorFinalize();
            }
        }
    }
    else if(keyboard.Check( tokenStroke, AppKeyboard::Press ))
    {
        triBrusher.Continue( touch[0].pos ); // finger 0 draws // todo: add camera and camera slerp?
    }

    if( keyboard.Check( tokenStroke, AppKeyboard::Release ) == false ) // anything other than released
    {
        switch( toolMode )
        {
            case ColorMode:
                triBrusher.Stroke( fnPaintColor, kStrokeTune );
                MODEL.UpdateColorTick();
                break;
            case InflateMode:
                triBrusher.Stroke( fnPaintInflate, kStrokeTune );
                normalBrusher.Stroke( kStrokeTune ); // rough normal asjustment...
                MODEL.UpdatePosTick();
                break;
            case DeflateMode:
                triBrusher.Stroke( fnPaintDeflate, kStrokeTune );
                normalBrusher.Stroke( kStrokeTune ); // rough normal asjustment...
                MODEL.UpdatePosTick();
                break;
            case HandleMode:
                triBrusher.Stroke_handled( fnPaintHandle, kStrokeTune );
                normalBrusher.Stroke( kStrokeTune ); // rough normal asjustment...
                MODEL.UpdatePosTick();
                break;
            default:;
        }
    }

    /////////////////// debug options

#if DEBUG
//    platform.showDepthBuffer = keyboard.Check( 'd', AppKeyboard::Press );

    glm::vec2 depthDelta;
    if( keyboard.Check( '1', AppKeyboard::Press ) ) depthDelta.x=-1.f;
    if( keyboard.Check( '2', AppKeyboard::Press ) ) depthDelta.x=+1.f;
    if( keyboard.Check( '3', AppKeyboard::Press ) ) depthDelta.y=-1.f;
    if( keyboard.Check( '4', AppKeyboard::Press ) ) depthDelta.y=+1.f;
    if( glLength( depthDelta ) > .1f ) // todo: test
//    if( glLength( depthDelta ) > std::numeric_limits<float>::denorm_min() ) // todo: test
    {
        sizDepthRange = sizDepthRange + depthDelta;
        printf("%f %f\n",sizDepthRange.x,sizDepthRange.y);
        mxProj = glm::perspective( glm::radians( degreeFOV ), whRatio, sizDepthRange.x, sizDepthRange.y );
    }
#endif

};

void AppRender()
{
    gl9ClearColor3fv(glm::value_ptr(backColor));
    gl9BeginFrame();

    // the model might penetrate the picker, so don't draw model when picker is open
    if(!colorPicker.visible)
    {
        gl9UseProgram( GL9_WORLD );
        gl9MatrixMode( GL_PROJECTION );
        gl9LoadMatrixf( glm::value_ptr( mxProj ));
        gl9MatrixMode( GL_MODELVIEW );
        gl9LoadMatrixf( glm::value_ptr( mxView ));
        gl9LoadLightf( glm::value_ptr( posLight ) );

        MODEL.Render();

        if( keyboard.Check( 'c', AppKeyboard::Press ))
        {
            glm::vec3 axisIn = glm::normalize( posOrigin - posCamera );
            MODEL.RenderCollisionBody( axisIn, axisUp, {1,1,1} );
        }
        if( keyboard.Check( 'n', AppKeyboard::Press ))
        {
            MODEL.RenderNormals();
        }
    }

    if( uiInactiveElapsedMSec < kMenuTimeout || colorPicker.visible )
    {
        gl9UseProgram( GL9_MENU );
        gl9MatrixMode( GL_PROJECTION );
        gl9LoadMatrixf( glm::value_ptr( mxOrtho ));
        gl9MatrixMode( GL_MODELVIEW );
        gl9LoadIdentity();

        gl9Color3fv( glm::value_ptr( menuColor ));

        gl9Disable( GL_DEPTH_TEST ); // draw on top
        menu.Render();
        gl9Enable( GL_DEPTH_TEST );

        if( colorPicker.visible )
        {
            colorPicker.Render();

            // sample buffer after glFinish
            if( keyboard.Check( tokenPickAndCloseDialog, AppKeyboard::Fresh ) )
            {
                if( colorPicker.PickColor(touch[0].pos) )
                {
                    glFinish();
                    paintColor = colorPicker.color;
                }
                keyboard.Toggle( 'w' ); // close picker
            }
        }

        gl9UseProgram( GL9_POINTER );
        gl9MatrixMode( GL_PROJECTION );
        gl9LoadMatrixf( glm::value_ptr( mxOrtho ));
        gl9MatrixMode( GL_MODELVIEW );
        gl9LoadIdentity();

        gl9Color3fv( glm::value_ptr( cursorColor ));

        gl9Disable( GL_DEPTH_TEST ); // draw on top
        if( cursor[0].visible ) cursor[0].Render();
        if( cursor[1].visible ) cursor[1].Render();
        gl9Enable( GL_DEPTH_TEST );
    }

    if( dialogStack.size() > 0 )
    {
        gl9UseProgram( GL9_POINTER );
        gl9MatrixMode( GL_PROJECTION );
        gl9LoadMatrixf( glm::value_ptr( mxOrtho ));
        gl9MatrixMode( GL_MODELVIEW );
        gl9LoadIdentity();

        gl9Color3fv( glm::value_ptr( cursorColor ));

        gl9Disable( GL_DEPTH_TEST ); // draw on top
        dialogStack.front()->Render();
        gl9Enable( GL_DEPTH_TEST );
    }

    gl9EndFrame();
};

///////////////

void AppEvent(const AppPlatform::Event &event)
{
    uiActive = true;

    // see comment in Touch decl on how tristating works
    if(touch[0].active == Touch::Ending) touch[0].active = Touch::False;
    if(touch[1].active == Touch::Ending) touch[1].active = Touch::False;

    switch( event.kind )
    {
        case AppPlatform::Event::Adornment:
            switch( event.u.adornment.adKind )
            {
                case AppPlatform::Event::Kind::Close:
                    appQuit = true;
                    break;
                case AppPlatform::Event::Kind::Pause:
                    if( !AppSerialize( SAVE ) )
                        AppDialog( '^' );
                    appPause = true;
                    break;
                case AppPlatform::Event::Kind::Resume:
                    AppSerialize( LOAD ); // ignore failure
                    appPause = false;
                    break;
                default: ;
            }
            break;
        case AppPlatform::Event::Key:
            keyboard.DoKey( uint8_t(event.u.key.key), event.u.key.press );
            break;
        case AppPlatform::Event::Touch:
            switch( event.u.touch.toKind )
            {
                case AppPlatform::Event::Kind::Begin:
                {
                    touch[event.u.touch.id].active = Touch::True;
                    touch[event.u.touch.id].pos = glm::vec3( (float) event.u.touch.x, (float) event.u.touch.y, NearplaneZ );
                    touchLastPos[event.u.touch.id] = touch[event.u.touch.id].pos;

                    // modal text shown - clear dialog
                    if( dialogStack.size() > 0 )
                    {
                        keyboard.Toggle(tokenCloseTextDialog);
                        break;
                    }

                    // only finger 0 deals with commands
                    if(event.u.touch.id==0) // finger 0 can do anything...
                    {
                        if( colorPicker.visible )
                        {
                            keyboard.Toggle( tokenPickAndCloseDialog );
                        }
                        else
                        {
                            uint8_t *pToken = menu.Translate( event.u.touch.x, event.u.touch.y );
                            if( menu.visible )
                            {
                                if( pToken ) keyboard.Toggle( *pToken ); // select item
                                keyboard.DoRelease( menu.menuToken ); // close menu
                            }
                            else
                            {
                                if( pToken )
                                    keyboard.DoPress( menu.menuToken ); // open menu
                                else
                                {
                                    // touch on object?
                                    auto posCursor = fnUnproject( touch[0].pos );
                                    auto incidentVec = glm::normalize( posCursor - posCamera );
                                    trisearch_type cxt;
                                    MODEL.rubus.IdentifyTri( cxt, posCamera, incidentVec );
                                    if( cxt.collisionTri == TriIDEnd )
                                    {
                                        triBrusher.Stop(); // review: put logic in start()?
                                        normalBrusher.Stop(); // review: put logic in start()?
                                        keyboard.DoPress( tokenScroll );
                                    }
                                    else
                                    {
                                        keyboard.DoPress( tokenStroke );
                                    }
                                }
                            }
                        }
                    }

                    if(event.u.touch.id==1)
                    {
                        // allow 2-finger control when too close to reach free-space
                        if(touch[0].active)
                        {
                            keyboard.DoRelease( tokenScroll );
                            keyboard.DoRelease( tokenStroke );
                        }
                        twoFinger.Reset();
                    }
                    break;
                }
                case AppPlatform::Event::Kind::Move:
                    touch[event.u.touch.id].pos = glm::vec3((float) event.u.touch.x, (float) event.u.touch.y, NearplaneZ );

                    break;
                case AppPlatform::Event::Kind::End:
                    touch[event.u.touch.id].active = Touch::Ending;
                    touch[event.u.touch.id].pos = glm::vec3((float) event.u.touch.x, (float) event.u.touch.y, NearplaneZ );

                    // only finger 0 deals with commands
                    if(event.u.touch.id==0)
                    {
                        keyboard.DoRelease( tokenScroll );
                        keyboard.DoRelease( tokenStroke );
                    }

                    twoFinger.Reset();
                    break;
                default: ;
            }
            break;
        default: ;
    }
};

/////////////////

void app_rebind()
{
    gl9Bind(platWidth, platHeight);

    AppTutorial::Items() = {
        {'?', '*', "Welcome to MODELSAUR!\n\n"
                   "The top left tool menu has deforming and coloring tools and can save and undo your work." },
        {'?', 's', "Good work! You are learning to draw! Reorient the model by one-finger drawing on the"
                       " black edges or two-finger pinch-to-zoom or dial-to-rotate." },
        {'?', 'w', "You selected the COLORPICKER.\n\n"
                       "Pick a color for drawing on the surface of your model." },
        {'?', 'e', "You have selected the INFLATE tool.\n\n"
                   "This tool lifts a portion of the surface the size of the current BRUSH size." },
        {'?', 'r', "You have selected the DEFLATE tool.\n\n"
                   "This tool depresses a portion of the surface the size of the current BRUSH size." },
        {'?', 't', "You have selected the DEFORM tool.\n\n"
                       "This tool allows brush-sized deformation in the direction of the surface normal." },
        {'?', 'W', "You have selected the LITTLE brush.\n\n"
                   "This brush can be used for geometry manipulation or vertex coloring." },
        {'?', 'E', "You have selected the BIG brush.\n\n"
                   "This brush can be used for geometry manipulation or vertex coloring." },
        {'?', 'R', "You have selected the GIANT brush.\n\n"
                   "This brush can be used for geometry manipulation or vertex coloring." },
        {'?', 'j', "This button rotates your model around the UP axis of your screen." },
        {'?', 'I', "This button rotates your model around the OUT axis of your screen." },
        {'?', 'S', "This button saves your work to the storage on your device." },
        {'?','\b', "This is the UNDO button.\n\nThis will undo your most recent coloring or geometry change." },
        {'?', 'Z', "This is the TRASH button.\n\nIt will reset the colors and geometry of your model." },
        {'!', '!', "You found an easteregg!\n\nThe TRASH button now resets the geometry to be"
                       " objects other than just spheres." },
        {'!', '^', "Unable to access filesystem.\n\nPlease grant storage permissions for the"
                       " app to save your work." },
        {'!', '?', "Privacy Policy\n\n"
                   "Modelsaur respects your privacy and does not collect any personal information about your installation or use of any Modelsaur applications."
                   " You retain the copyright of any works you create using Modelsaur applications and any work you create resides on your device for you to share or keep private as you wish." }
    };

    menu.menu = {
            { 'm', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_png ) ) },
            { 'w', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_colorpicker_png ) ) },
            { 'e', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_inflate_png ) ) },
            { 'r', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_deflate_png ) ) },
            { 't', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_handle_png ) ) },
            { 'W', RMenu::Row, AppTexture::LoadResource( ACCESS_RESOURCE( menu_littletool_png ) ) },
            { 'E', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_bigtool_png ) ) },
            { 'R', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_gianttool_png ) ) },
            { 'j', RMenu::Row, AppTexture::LoadResource( ACCESS_RESOURCE( menu_rot_y_png ) ) },
            { 'I', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_rot_z_png ) ) },
            { 'S', RMenu::Row, AppTexture::LoadResource( ACCESS_RESOURCE( menu_save_png ) ) },
            { 0x08, RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_undo_png ) ) },
            { 'Z', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_trash_png ) ) },
            { '?', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_privacy_png ) ) },
            { '!', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_cheat_png ) ) },
#if defined(OGL1)
            { 'n', RMenu::Row, AppTexture::LoadResource( ACCESS_RESOURCE( menu_normals_png ) ) },
            { 'c', RMenu::Cat, AppTexture::LoadResource( ACCESS_RESOURCE( menu_collision_png ) ) },
#endif // OGL1
    };

    menu.Bind('m', std::min(platWidth, platHeight), NearplaneZ);

    colorPicker.Bind(
        platWidth, platHeight, NearplaneZ,
        AppTexture::LoadResource( ACCESS_RESOURCE( dialog_colorpicker_png ) )
    );
    cursor[0].Bind(std::min(platWidth, platHeight));
    cursor[1].Bind(std::min(platWidth, platHeight));
    sphere.Bind();
    triBrusher.Bind(&MODEL.rubus, &MODEL);
    normalBrusher.Bind(&MODEL);
    normalBrusher.ReStrokeObject(); // right after binding

    if( dialogStack.size() > 0 )
        dialogStack.front()->Bind( platWidth, platHeight );
}

void app_release()
{
    if( dialogStack.size() > 0 )
        dialogStack.front()->Release();

    normalBrusher.Release();
    triBrusher.Release();
    sphere.Release();
    cursor[0].Release();
    cursor[1].Release();
    menu.Release();
    colorPicker.Release();

    gl9Release();
}

static bool main_init = true;
void app_main()
{
    if(main_init)
    {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        srand((unsigned int) spec.tv_nsec);

#ifdef DEBUG
        AppML::Test();
#endif
    }

    platform.Bind( app_rebind, app_release, "Modelsaur", "Wacom Intuos PT S 2 Finger" );

    if(main_init)
    {
        cameraReset();
        paintColor = glm::vec3(RndColour, RndColour, RndColour);
        AppDialog( '*' );
    }
    main_init = false; // init complete

    whRatio = float(platWidth) / float(platHeight); // todo: check on linux

    mxProj = glm::perspective( glm::radians( degreeFOV ), whRatio, sizDepthRange.x, sizDepthRange.y );
    mxOrtho = glm::ortho( 0.f, float(platWidth), float(platHeight), 0.0f );
    boxViewport = { 0, 0, platWidth, platHeight };

    AppAlarm notification;
    notification.interval = 2000;

    bool firstTick = true;
    appQuit = false;
    appPause = false;
    while(true)
    {
        // sensitive ordering here
        keyboard.Tick();

        // update old positions prior to setting new ones
        touchLastPos[0] = touch[0].pos;
        touchLastPos[1] = touch[1].pos;

        uiActive = false;
        platform.Tick( AppEvent );
        if(appQuit) break; // when told to quit, no tickie-tickie!
        if(appPause) continue; // keep doing important stuff

        // update cursor new positions
        cursor[0].Update(touch[0].pos, touch[0].active);
        cursor[1].Update(touch[1].pos, touch[1].active);

        // startup events generated here
        if(firstTick)
        {
            firstTick = false;
            toolMode = ColorMode;
            toolType = SmallTool;
        }

        AppLogic( platform.deltaMSec );

        uiInactiveElapsedMSec = uiActive ? 0 : uiInactiveElapsedMSec + platform.deltaMSec;

        AppRender();

        notification.Tick(platform.deltaMSec);
        if( notification.triggered )
        {
#ifdef DEBUG
            float fps = 1 / platform.deltaSecAvg;
            static float fps_ = 1;
            bool update = (MODEL.bytesUpdated > 0) | (std::abs(fps - fps_) > 1);
            if( update )
            {
                auto bytesPerSec = float(MODEL.bytesUpdated) / (float(notification.interval) / float(1000));
                AppLog::Info( __FILENAME__, "platform %f fps, %8.2f kBps", fps, bytesPerSec / float(1000) );
                fps_ = fps;
                MODEL.bytesUpdated = 0;
            }
#endif // DEBUG
        }
    }

    if( dialogStack.size() > 0 )
        dialogStack.front()->Release();

    platform.Release();
}
