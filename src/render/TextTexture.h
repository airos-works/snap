/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_TEXT_TEXTURE_H
#define SNAP_TEXT_TEXTURE_H


#include <Font.h>
#include <String.h>

#include "GLUtilities.h"


namespace snap {


// Text on the stage, drawn by app_server: the string is drawn with a BFont
// into a BBitmap (white on black), and the bitmap becomes a texture whose red
// channel is how much of each pixel the glyphs cover (ui-text.glsl). This
// gets the system's fonts, hinting and antialiasing into OpenGL for free.
//
// Only B_RGB32 bitmaps are used: app_server leaves the alpha channel of
// offscreen B_RGBA32 bitmaps undefined, coverage never needs it.
class TextTexture {
public:
								TextTexture();

	// Draws the text again only if the text or the font size changed.
			status_t			SetTo(const char* text, const BFont& font);

			const BString&		Text() const { return fText; }
			float				Width() const { return fWidth; }
			float				Height() const { return fHeight; }
			const Texture&		GetTexture() const { return fTexture; }
			bool				IsValid() const { return fTexture.IsValid(); }

private:
			BString				fText;
			float				fFontSize;
			float				fWidth;
			float				fHeight;
			Texture				fTexture;
};


// Text Mode's ramp of characters: glyphs of the fixed font side by side in
// one texture, sorted from the least ink to the most, so that a brighter
// picture can pick a "heavier" character. Returns the number of glyphs.
int32 MakeGlyphRamp(Texture& texture);


}	// namespace snap


#endif	// SNAP_TEXT_TEXTURE_H
