/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "SnapApp.h"

#include <Alert.h>
#include <Entry.h>

#include <cstdio>
#include <cstdlib>

#include "SnapWindow.h"


namespace snap {


const char* kAppSignature = "application/x-vnd.airOS-Snap";


SnapApp::SnapApp()
	:
	BApplication(kAppSignature),
	fLaunchRefs(B_REFS_RECEIVED)
{
	fSettings.Load();
}


void
SnapApp::ReadyToRun()
{
	if (!_CheckGraphics()) {
		Quit();
		return;
	}
	SnapWindow* window = new SnapWindow(fSettings, fGPU.renderer);
	window->Show();
	fWindow = BMessenger(window);
	// Files Snap was opened with arrived before there was a window.
	if (!fLaunchRefs.IsEmpty())
		fWindow.SendMessage(&fLaunchRefs);
}


bool
SnapApp::_CheckGraphics()
{
	const status_t status = ProbeOpenGL(fGPU);
	fprintf(stderr, "Snap: OpenGL %s by \"%s\"\n", fGPU.version.String(),
		fGPU.renderer.String());
	if (status == B_OK && fGPU.MeetsSnapNeeds())
		return true;

	// For working on Snap in a virtual machine, which has no GPU.
	const char* allow = getenv("SNAP_ALLOW_SOFTWARE_RENDERING");
	if (status == B_OK && allow != NULL && strcmp(allow, "1") == 0) {
		fprintf(stderr, "Snap: software rendering allowed by "
			"SNAP_ALLOW_SOFTWARE_RENDERING\n");
		return true;
	}

	BString text("Snap needs a graphics card with 3D acceleration.\n\n");
	if (status != B_OK) {
		text << "This computer has no OpenGL driver that works.";
	} else if (!fGPU.hardware) {
		text << "This computer draws OpenGL in software (\"" << fGPU.renderer
			<< "\"), which is far too slow for Snap's live effects.";
	} else {
		text << "The graphics driver (\"" << fGPU.renderer << "\") offers "
			"OpenGL " << fGPU.major << "." << fGPU.minor << "; Snap's "
			"effects need OpenGL 3.1 or newer.";
	}
	BAlert* alert = new BAlert("Snap", text.String(), "Quit", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
	return false;
}


void
SnapApp::AboutRequested()
{
	BString text;
	text << "Snap\n\n"
		"A photo booth for air/OS: the camera through live effects drawn by "
		"the graphics card, some of which dance to the sound around you or "
		"to music.\n\n"
		"Graphics: " << fGPU.renderer << "\n"
		"OpenGL " << fGPU.version << "\n\n"
		"Photos are saved in your Pictures/Snap folder.\n\n"
		"\xC2\xA9 2026 air/OS contributors. MIT license.";
	BAlert* alert = new BAlert("About Snap", text.String(), "OK");
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go(NULL);
}


void
SnapApp::RefsReceived(BMessage* message)
{
	// Sound files opened with Snap play for the effects; the window does it.
	if (fWindow.IsValid())
		fWindow.SendMessage(message);
	else {
		entry_ref ref;
		for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
			fLaunchRefs.AddRef("refs", &ref);
	}
}


BHandler*
SnapApp::ResolveSpecifier(BMessage* message, int32 index, BMessage* specifier,
	int32 what, const char* property)
{
	// Snap's properties live in the window; `hey Snap get Effect` works
	// without naming it. A handler of another looper cannot be returned
	// here, so the message is passed on the way BApplication passes on
	// "Window" specifiers: sent to the window's looper (which then replies
	// to the original sender), and NULL returned.
	if (fWindow.IsValid() && SnapWindow::HasProperty(property)) {
		fWindow.SendMessage(message);
		return NULL;
	}
	return BApplication::ResolveSpecifier(message, index, specifier, what,
		property);
}


}	// namespace snap
