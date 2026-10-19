/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_APP_H
#define SNAP_APP_H


#include <Application.h>

#include "GPUCheck.h"
#include "Settings.h"


namespace snap {


extern const char* kAppSignature;


class SnapWindow;


// The application: checks that the computer can draw the effects, then
// opens the window. A camera serves one program at a time, so Snap is
// B_SINGLE_LAUNCH (see Snap.rdef): launching it again brings this one up.
class SnapApp : public BApplication {
public:
								SnapApp();

	virtual	void				ReadyToRun();
	virtual	void				AboutRequested();
	virtual	void				RefsReceived(BMessage* message);
	virtual	BHandler*			ResolveSpecifier(BMessage* message,
									int32 index, BMessage* specifier,
									int32 what, const char* property);

private:
			bool				_CheckGraphics();

			Settings			fSettings;
			GPUInfo				fGPU;
			SnapWindow*			fWindow;
};


}	// namespace snap


#endif	// SNAP_APP_H
