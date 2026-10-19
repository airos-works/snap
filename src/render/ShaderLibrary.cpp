/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "ShaderLibrary.h"

#include <Application.h>
#include <File.h>
#include <Path.h>
#include <Resources.h>

#include <cstdlib>

#include "GLUtilities.h"


namespace snap {


// The resource type of a shader's source: plain text, but not
// zero-terminated, so it does not pass for a B_STRING_TYPE.
static const type_code kShaderResourceType = 'GLSL';

// Every shader is compiled as GLSL 1.40, i.e. OpenGL 3.1, which is what the
// slowest GPU air/OS drives (Panfrost on the ROCK 5) offers through BGLView.
static const char* kVersionLine = "#version 140\n";


ShaderLibrary::ShaderLibrary()
{
	const char* directory = getenv("SNAP_SHADER_DIR");
	if (directory != NULL && directory[0] != '\0')
		fDirectory = directory;
}


status_t
ShaderLibrary::Init(BString& error)
{
	status_t status = Source("quad.vert", fVertexSource);
	if (status == B_OK)
		status = Source("common.glsl", fPrelude);
	if (status != B_OK)
		error << "The shaders are missing from the application: " << strerror(status);
	return status;
}


status_t
ShaderLibrary::Source(const char* name, BString& source)
{
	if (fDirectory.Length() > 0) {
		BPath path(fDirectory.String(), name);
		BFile file(path.Path(), B_READ_ONLY);
		off_t size;
		status_t status = file.InitCheck();
		if (status == B_OK)
			status = file.GetSize(&size);
		if (status != B_OK)
			return status;
		char* buffer = source.LockBuffer(size + 1);
		const ssize_t bytesRead = file.Read(buffer, size);
		source.UnlockBuffer(bytesRead > 0 ? bytesRead : 0);
		return bytesRead == size ? B_OK : B_IO_ERROR;
	}

	// The application's own resources. LoadResource() keeps the data cached
	// in the BResources object; it is copied into the string here.
	BResources* resources = be_app->AppResources();
	if (resources == NULL)
		return B_ERROR;
	size_t size = 0;
	const void* data = resources->LoadResource(kShaderResourceType, name, &size);
	if (data == NULL)
		return B_ENTRY_NOT_FOUND;
	source.SetTo((const char*)data, size);
	return B_OK;
}


status_t
ShaderLibrary::Build(ShaderProgram& program, const char* fragmentName,
	BString& error)
{
	BString effect;
	status_t status = Source(fragmentName, effect);
	if (status != B_OK) {
		error << fragmentName << ": " << strerror(status);
		return status;
	}

	// "#line 1 N" restarts the line numbers, so that compile errors point at
	// lines of the right file (0: vertex shader, 1: prelude, 2: effect).
	BString vertex(kVersionLine);
	vertex << "#line 1 0\n" << fVertexSource;
	BString fragment(kVersionLine);
	fragment << "#line 1 1\n" << fPrelude << "\n#line 1 2\n" << effect;

	BString log;
	status = program.Build(fragmentName, vertex.String(), fragment.String(), log);
	if (status != B_OK)
		error << fragmentName << " (file 1 = common.glsl, 2 = " << fragmentName
			<< "): " << log;
	return status;
}


}	// namespace snap
