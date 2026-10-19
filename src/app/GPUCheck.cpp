/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "GPUCheck.h"

#include <GLView.h>
#include <Window.h>

#include <GL/gl.h>

#include <cstdio>


namespace snap {


// GLSL 1.40, the language of Snap's shaders, comes with OpenGL 3.1.
static const int kNeededMajor = 3;
static const int kNeededMinor = 1;


GPUInfo::GPUInfo()
	:
	major(0),
	minor(0),
	hardware(false)
{
}


bool
GPUInfo::MeetsSnapNeeds() const
{
	return hardware && (major > kNeededMajor
		|| (major == kNeededMajor && minor >= kNeededMinor));
}


bool
IsSoftwareRenderer(const char* renderer)
{
	static const char* kSoftware[] = { "llvmpipe", "softpipe", "lavapipe",
		"swrast", "Software Rasterizer", NULL };
	if (renderer == NULL || renderer[0] == '\0')
		return true;
	const BString name(renderer);
	for (int i = 0; kSoftware[i] != NULL; i++) {
		if (name.IFindFirst(kSoftware[i]) >= 0)
			return true;
	}
	return false;
}


static BString
gl_string(GLenum name)
{
	const GLubyte* value = glGetString(name);
	return value != NULL ? BString((const char*)value) : BString();
}


status_t
ProbeOpenGL(GPUInfo& info)
{
	// A small window off screen that is never shown: the BGLView makes its
	// context when it is attached to the window.
	BWindow* window = new BWindow(BRect(-200, -200, -137, -137), "Snap GPU check",
		B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AVOID_FOCUS | B_NOT_MOVABLE | B_NOT_RESIZABLE);
	window->Lock();
	BGLView* view = new BGLView(window->Bounds(), "probe", B_FOLLOW_ALL_SIDES,
		0, BGL_RGB | BGL_DOUBLE);
	window->AddChild(view);

	view->LockGL();
	info.renderer = gl_string(GL_RENDERER);
	info.vendor = gl_string(GL_VENDOR);
	info.version = gl_string(GL_VERSION);
	view->UnlockGL();

	// Quit() deletes a locked window that never ran, and its views.
	window->Quit();

	if (info.version.Length() == 0)
		return B_NOT_SUPPORTED;
	if (sscanf(info.version.String(), "%d.%d", &info.major, &info.minor) != 2)
		info.major = info.minor = 0;
	info.hardware = !IsSoftwareRenderer(info.renderer.String());
	return B_OK;
}


}	// namespace snap
