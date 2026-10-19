/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_GL_UTILITIES_H
#define SNAP_GL_UTILITIES_H


// Small RAII wrappers for the OpenGL objects Snap uses. Every air/OS OpenGL
// stack (Mesa's Haiku renderer on NVIDIA, libglvnd + EGL on AMD and ARM)
// exports the whole OpenGL 3.x API from libGL, so the functions are called
// directly; no loader is needed.
//
// All of these must be used on the thread that holds the BGLView's context
// (between LockGL() and UnlockGL()).

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include <String.h>

#include <map>


namespace snap {


// A linked vertex + fragment shader program. Uniform locations are looked up
// once and cached by name; setting a uniform the program does not use (the
// GLSL compiler removes unused ones) is silently ignored.
class ShaderProgram {
public:
								ShaderProgram();
								~ShaderProgram();

			status_t			Build(const char* name, const char* vertexSource,
									const char* fragmentSource, BString& log);
			bool				IsValid() const { return fProgram != 0; }
			const char*			Name() const { return fName.String(); }

			void				Use() const;

			GLint				Location(const char* uniform);
			void				Set(const char* uniform, int value);
			void				Set(const char* uniform, float value);
			void				Set(const char* uniform, float x, float y);
			void				Set(const char* uniform, float x, float y,
									float z);
			void				Set(const char* uniform, float x, float y,
									float z, float w);

private:
	static	GLuint				_Compile(GLenum type, const char* source,
									BString& log);

			GLuint				fProgram;
			BString				fName;
			std::map<BString, GLint> fLocations;

								ShaderProgram(const ShaderProgram&) = delete;
			ShaderProgram&		operator=(const ShaderProgram&) = delete;
};


// A 2D texture with linear filtering and edges clamped.
class Texture {
public:
								Texture();
								~Texture();

	// (Re)allocates storage when the size or format changed. Returns true
	// when it did, so callers know any content is gone.
			bool				Allocate(int width, int height,
									GLenum internalFormat, bool mipmaps = false);
			bool				Matches(int width, int height,
									GLenum internalFormat, bool mipmaps) const;
			void				Release();
	// Uploads B_RGB32 pixels (B, G, R, X bytes), rows top to bottom, and
	// refreshes the mipmaps if the texture has them.
			void				UploadBGRA(const void* pixels, int bytesPerRow);
	// Uploads one float per texel into a GL_R32F texture.
			void				UploadFloats(const float* values);

			void				Bind(int unit) const;
			GLuint				Id() const { return fTexture; }
			int					Width() const { return fWidth; }
			int					Height() const { return fHeight; }
			bool				IsValid() const { return fTexture != 0; }
			bool				HasMipmaps() const { return fMipmaps; }

private:
			GLuint				fTexture;
			int					fWidth;
			int					fHeight;
			GLenum				fInternalFormat;
			bool				fMipmaps;

								Texture(const Texture&) = delete;
			Texture&			operator=(const Texture&) = delete;
};


// An offscreen frame buffer drawing into one Texture: where effects draw, and
// what they read back the next frame when they need memory. Its texture can
// also be filled from the CPU, which is how camera pictures arrive.
class RenderTarget {
public:
								RenderTarget();
								~RenderTarget();

	// Returns true when the target was (re)created; its content is then
	// cleared to transparent black.
			bool				Resize(int width, int height,
									GLenum internalFormat = GL_RGBA8,
									bool mipmaps = false);
			void				Release();

	// Makes this the drawing target and sets the viewport to all of it.
			void				Bind() const;
			void				BindTexture(int unit) const
									{ fTexture.Bind(unit); }
	// Refreshes the smaller mipmap levels after drawing or uploading.
			void				GenerateMipmaps() const;

			Texture&			ColorTexture() { return fTexture; }
			GLuint				Framebuffer() const { return fFramebuffer; }
			GLuint				TextureId() const { return fTexture.Id(); }
			int					Width() const { return fTexture.Width(); }
			int					Height() const { return fTexture.Height(); }
			bool				IsValid() const { return fFramebuffer != 0; }

private:
			GLuint				fFramebuffer;
			Texture				fTexture;

								RenderTarget(const RenderTarget&) = delete;
			RenderTarget&		operator=(const RenderTarget&) = delete;
};


// Two targets used in turn: read last frame's result, write this frame's.
// Feedback effects (fire, echo) keep their state this way.
class PingPongTarget {
public:
								PingPongTarget() : fCurrent(0) {}

			bool				Resize(int width, int height,
									GLenum internalFormat = GL_RGBA8);
			void				Swap() { fCurrent ^= 1; }

			RenderTarget&		Previous() { return fTargets[fCurrent ^ 1]; }
			RenderTarget&		Next() { return fTargets[fCurrent]; }

private:
			RenderTarget		fTargets[2];
			int					fCurrent;
};


// Restores the frame buffer binding and viewport it found, so code that
// renders offscreen can be called from anywhere in a frame.
class FramebufferScope {
public:
								FramebufferScope();
								~FramebufferScope();

private:
			GLint				fFramebuffer;
			GLint				fViewport[4];
};


// Reports and clears OpenGL errors; returns true when there were any.
bool CheckGLError(const char* where);


}	// namespace snap


#endif	// SNAP_GL_UTILITIES_H
