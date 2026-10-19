/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */

// Snap: a photo booth for air/OS. See README.md for what it does and
// docs/DESIGN.md for how; the classes are in src/app (application, window),
// src/capture (camera), src/audio (sound), src/render and src/effects
// (OpenGL), src/photos and src/ui.


#include "app/SnapApp.h"


int
main()
{
	snap::SnapApp app;
	app.Run();
	return 0;
}
