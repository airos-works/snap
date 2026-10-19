/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "GLUtilities.h"

#include <cstdio>
#include <vector>


namespace snap {


// #pragma mark - ShaderProgram


ShaderProgram::ShaderProgram()
	:
	fProgram(0)
{
}


ShaderProgram::~ShaderProgram()
{
	if (fProgram != 0)
		glDeleteProgram(fProgram);
}


GLuint
ShaderProgram::_Compile(GLenum type, const char* source, BString& log)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_TRUE) {
		GLint length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		std::vector<char> text(length > 1 ? length : 1, '\0');
		glGetShaderInfoLog(shader, (GLsizei)text.size(), NULL, text.data());
		log << (type == GL_VERTEX_SHADER ? "vertex shader: " : "fragment shader: ")
			<< text.data();
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}


status_t
ShaderProgram::Build(const char* name, const char* vertexSource,
	const char* fragmentSource, BString& log)
{
	fName = name;
	GLuint vertex = _Compile(GL_VERTEX_SHADER, vertexSource, log);
	GLuint fragment = _Compile(GL_FRAGMENT_SHADER, fragmentSource, log);
	if (vertex == 0 || fragment == 0) {
		if (vertex != 0)
			glDeleteShader(vertex);
		if (fragment != 0)
			glDeleteShader(fragment);
		return B_ERROR;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glBindFragDataLocation(program, 0, "fragColor");
	glLinkProgram(program);
	// The program keeps what it needs; the shader objects can go.
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		GLint length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		std::vector<char> text(length > 1 ? length : 1, '\0');
		glGetProgramInfoLog(program, (GLsizei)text.size(), NULL, text.data());
		log << "link: " << text.data();
		glDeleteProgram(program);
		return B_ERROR;
	}

	if (fProgram != 0)
		glDeleteProgram(fProgram);
	fProgram = program;
	fLocations.clear();
	return B_OK;
}


void
ShaderProgram::Use() const
{
	glUseProgram(fProgram);
}


GLint
ShaderProgram::Location(const char* uniform)
{
	std::map<BString, GLint>::iterator found = fLocations.find(uniform);
	if (found != fLocations.end())
		return found->second;
	const GLint location = glGetUniformLocation(fProgram, uniform);
	fLocations[uniform] = location;
	return location;
}


void
ShaderProgram::Set(const char* uniform, int value)
{
	glUniform1i(Location(uniform), value);
}


void
ShaderProgram::Set(const char* uniform, float value)
{
	glUniform1f(Location(uniform), value);
}


void
ShaderProgram::Set(const char* uniform, float x, float y)
{
	glUniform2f(Location(uniform), x, y);
}


void
ShaderProgram::Set(const char* uniform, float x, float y, float z)
{
	glUniform3f(Location(uniform), x, y, z);
}


void
ShaderProgram::Set(const char* uniform, float x, float y, float z, float w)
{
	glUniform4f(Location(uniform), x, y, z, w);
}


// #pragma mark - Texture


Texture::Texture()
	:
	fTexture(0),
	fWidth(0),
	fHeight(0),
	fInternalFormat(0),
	fMipmaps(false)
{
}


Texture::~Texture()
{
	Release();
}


void
Texture::Release()
{
	if (fTexture != 0)
		glDeleteTextures(1, &fTexture);
	fTexture = 0;
	fWidth = fHeight = 0;
}


bool
Texture::Matches(int width, int height, GLenum internalFormat,
	bool mipmaps) const
{
	return fTexture != 0 && width == fWidth && height == fHeight
		&& internalFormat == fInternalFormat && mipmaps == fMipmaps;
}


bool
Texture::Allocate(int width, int height, GLenum internalFormat, bool mipmaps)
{
	if (Matches(width, height, internalFormat, mipmaps))
		return false;

	if (fTexture == 0)
		glGenTextures(1, &fTexture);
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const bool isFloat = internalFormat == GL_R32F
		|| internalFormat == GL_R16F || internalFormat == GL_RGBA16F;
	const GLenum format = internalFormat == GL_R32F || internalFormat == GL_R16F
		|| internalFormat == GL_R8 ? GL_RED : GL_BGRA;
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
		isFloat ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL);

	fWidth = width;
	fHeight = height;
	fInternalFormat = internalFormat;
	fMipmaps = mipmaps;
	return true;
}


void
Texture::UploadBGRA(const void* pixels, int bytesPerRow)
{
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerRow / 4);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fWidth, fHeight, GL_BGRA,
		GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	if (fMipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);
}


void
Texture::UploadFloats(const float* values)
{
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fWidth, fHeight, GL_RED,
		GL_FLOAT, values);
}


void
Texture::Bind(int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, fTexture);
}


// #pragma mark - RenderTarget


RenderTarget::RenderTarget()
	:
	fFramebuffer(0)
{
}


RenderTarget::~RenderTarget()
{
	Release();
}


void
RenderTarget::Release()
{
	if (fFramebuffer != 0)
		glDeleteFramebuffers(1, &fFramebuffer);
	fFramebuffer = 0;
	fTexture.Release();
}


bool
RenderTarget::Resize(int width, int height, GLenum internalFormat,
	bool mipmaps)
{
	if (fFramebuffer != 0
		&& fTexture.Matches(width, height, internalFormat, mipmaps)) {
		return false;
	}
	Release();
	fTexture.Allocate(width, height, internalFormat, mipmaps);

	FramebufferScope scope;
	glGenFramebuffers(1, &fFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, fFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, fTexture.Id(), 0);
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Snap: frame buffer %dx%d incomplete (0x%x)\n", width,
			height, status);
		Release();
		return false;
	}
	glViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	if (mipmaps)
		GenerateMipmaps();
	return true;
}


void
RenderTarget::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fFramebuffer);
	glViewport(0, 0, fTexture.Width(), fTexture.Height());
}


void
RenderTarget::GenerateMipmaps() const
{
	if (!fTexture.HasMipmaps())
		return;
	glBindTexture(GL_TEXTURE_2D, fTexture.Id());
	glGenerateMipmap(GL_TEXTURE_2D);
}


// #pragma mark - PingPongTarget


bool
PingPongTarget::Resize(int width, int height, GLenum internalFormat)
{
	bool changed = fTargets[0].Resize(width, height, internalFormat);
	changed |= fTargets[1].Resize(width, height, internalFormat);
	return changed;
}


// #pragma mark - FramebufferScope


FramebufferScope::FramebufferScope()
{
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fFramebuffer);
	glGetIntegerv(GL_VIEWPORT, fViewport);
}


FramebufferScope::~FramebufferScope()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fFramebuffer);
	glViewport(fViewport[0], fViewport[1], fViewport[2], fViewport[3]);
}


// #pragma mark -


bool
CheckGLError(const char* where)
{
	bool any = false;
	for (GLenum error = glGetError(); error != GL_NO_ERROR;
			error = glGetError()) {
		fprintf(stderr, "Snap: OpenGL error 0x%x in %s\n", error, where);
		any = true;
	}
	return any;
}


}	// namespace snap
