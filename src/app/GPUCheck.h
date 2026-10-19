/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_GPU_CHECK_H
#define SNAP_GPU_CHECK_H


#include <String.h>


namespace snap {


// What draws OpenGL on this computer, as a BGLView reports it.
struct GPUInfo {
	BString				renderer;	// GL_RENDERER, e.g. "AMD Radeon 760M ..."
	BString				vendor;
	BString				version;	// GL_VERSION, e.g. "4.6 (Compatibility ...)"
	int					major;
	int					minor;
	bool				hardware;	// not a software rasterizer

						GPUInfo();
	bool				MeetsSnapNeeds() const;
};


// Snap's effects need a GPU: in software (llvmpipe, softpipe) they would run
// at a few frames a second. Before the window opens, Snap makes a hidden
// window with a BGLView, asks the context who renders and which OpenGL
// version it speaks, and closes it again.
status_t ProbeOpenGL(GPUInfo& info);

// True for the names software renderers give themselves (also when Zink
// runs on top of the lavapipe software Vulkan driver).
bool IsSoftwareRenderer(const char* renderer);


}	// namespace snap


#endif	// SNAP_GPU_CHECK_H
