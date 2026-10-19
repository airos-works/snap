/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_SHADER_LIBRARY_H
#define SNAP_SHADER_LIBRARY_H


#include <String.h>


namespace snap {


class ShaderProgram;


// Where the GLSL comes from. The shaders are part of the application file:
// the build imports each src/effects/shaders/*.glsl as a 'GLSL' resource
// named after the file (resources/Shaders.rdef), and they are read back with
// BResources at run time. Tracker and the "listres" command show them.
//
// While working on the effects, set SNAP_SHADER_DIR to the shaders folder of
// a source tree: Snap then reads the files from there instead, so a shader
// can be changed and tried without building Snap again (switch effects to
// rebuild them).
class ShaderLibrary {
public:
								ShaderLibrary();

			status_t			Init(BString& error);

	// Builds a program from the shared vertex shader and the named fragment
	// shader, with the prelude (common.glsl) in front of it.
			status_t			Build(ShaderProgram& program,
									const char* fragmentName, BString& error);

			status_t			Source(const char* name, BString& source);

private:
			BString				fDirectory;
			BString				fVertexSource;
			BString				fPrelude;
};


}	// namespace snap


#endif	// SNAP_SHADER_LIBRARY_H
