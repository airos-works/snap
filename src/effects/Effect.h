/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_EFFECT_H
#define SNAP_EFFECT_H


#include <String.h>

#include "audio/AudioAnalyzer.h"
#include "effects/EffectCatalog.h"


namespace snap {


class CameraHistory;
class RenderTarget;
class ShaderLibrary;
class ShaderProgram;
class Texture;


// The texture units the renderer binds before any effect draws; effects
// bind their own textures from kEffectTextureUnit on.
enum {
	kCameraTextureUnit		= 0,
	kPreviousTextureUnit	= 1,
	kSoundTextureUnit		= 2,
	kEffectTextureUnit		= 3
};


// Everything an effect may use to draw one frame.
struct EffectContext {
	int32				cameraWidth;
	int32				cameraHeight;
	float				time;			// seconds since Snap started
	float				delta;			// seconds since the last frame
	float				effectTime;		// seconds since this effect appeared
	bool				mirror;

	AudioFeatures		sound;
	float				beatAge;		// seconds since the last beat
	float				audioTime;		// runs faster when it is loud

	// Past camera pictures, kept while an effect that NeedsHistory() shows.
	CameraHistory*		history;
	// The character ramp for Text Mode: glyphs from dark to bright.
	const Texture*		glyphs;
	int32				glyphCount;

	// Sets the uniforms of common.glsl on a program in use, for an output of
	// the given size.
			void		Apply(ShaderProgram& program, int width,
							int height) const;
};


// Draws the rectangle set with the uRect uniform (all of the target by
// default, see quad.vert).
void DrawRectangle();


// One look Snap can give the camera picture. An effect lives on the render
// thread: it builds its shader programs in Prepare() and draws into the
// output it is given in Render(), at whatever size that output has (a small
// cell of the grid, or the full photo).
class Effect {
public:
	explicit					Effect(const EffectInfo& info);
	virtual						~Effect();

			const EffectInfo&	Info() const { return fInfo; }

	virtual	status_t			Prepare(ShaderLibrary& shaders,
									BString& error) = 0;
	virtual	void				Render(const EffectContext& context,
									RenderTarget& output) = 0;

	// Forgets what the effect remembers from earlier frames; called when it
	// appears again after not being shown.
	virtual	void				Reset() {}

	virtual	bool				NeedsHistory() const { return false; }

private:
			const EffectInfo&	fInfo;
};


// Makes the Effect for EffectAt(index).
Effect* CreateEffect(int32 index);


}	// namespace snap


#endif	// SNAP_EFFECT_H
