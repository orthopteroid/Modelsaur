#ifndef _LINUXGL9STATE_HPP_
#define _LINUXGL9STATE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

#include "AppPlatform.hpp"

struct AppPlatform::State
{
	static constexpr int batchSize = 20;

	// window
	Display* xDisplayPtr = 0;
	Colormap xColormap;
	Window xWindow;
	Atom xWMDeleteWindowAtom;
	XVisualInfo *xVisualPtr;

    // opengl
    GLXContext glContext;
    GLXFBConfig glxFBConfig;
    uint frameTimer;
    uint frameTimerValue;

	// touchpad
	struct mtdev mtdevState;
	int mtEventSlot = 0; // ie. finger 0
	AppPlatform::Event::Kind mtEventSlotStateArr[ AppPlatform::Event::MaxTouch ] = {AppPlatform::Event::Kind::Nil,AppPlatform::Event::Kind::Nil}; // holds trackids
	struct input_event mtEventArr[batchSize];
	struct AxisInfo
	{
		float min, max, rez;
	} axisInfo[2];
	AppPlatform::Event touchEvent;

	// mouse
	bool mousePressed = false;
	AppPlatform::Event mouseEvent;

	// platform
	struct timespec timer;
	int windowFD, touchFD;
	fd_set fdReadSet;

	// time
	long tick_newMSec = 0;
	long tick_oldMSec = 0;
	long tick_deltaMSec = 0;
	long tick_deltaMSecAvg = 0;
	float tick_deltaSecAvg = 0;
};

#endif // _LINUXGL9STATE_HPP_
