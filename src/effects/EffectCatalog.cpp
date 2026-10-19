/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "EffectCatalog.h"

#include <cstring>


namespace snap {


static const EffectInfo kEffects[] = {
	{ "crt", "CRT",
		"An old television: curved glass, scanlines, a shadow mask and glow.",
		"Noise makes the picture shake and fills it with static; a beat makes "
			"it roll." },
	{ "fire", "Set on Fire",
		"Flames rise from whatever moves and from your outline.",
		"Bass and loud sound feed the fire." },
	{ "metal", "Turning Metal",
		"You turn into polished chrome, from the top down.",
		"A beat sends a ripple across the metal." },
	{ "thermal", "Thermal Camera",
		"A heat camera's view; whatever moves glows hotter.",
		"The scene warms up with the noise in the room." },
	{ "popart", "Pop Art",
		"Four silkscreen prints in loud colours with halftone dots.",
		"The colours move along on every beat." },
	{ "normal", "Normal",
		"The camera picture as it is.",
		NULL },
	{ "neon", "Neon",
		"Your outlines as glowing neon tubes, with an equalizer of the sound.",
		"The equalizer shows the spectrum; treble makes the tubes flare." },
	{ "kaleidoscope", "Kaleidoscope",
		"Eight mirrored slices of the middle of the picture.",
		"It turns faster the louder it is and zooms with the bass." },
	{ "bulge", "Bass Bulge",
		"A fish-eye bulge in the middle of the picture.",
		"The bulge swells with the bass; a beat sends out a ripple." },
	{ "echo", "Echo",
		"Whatever moves leaves rainbow trails behind it.",
		"Trails last longer when it is loud; a beat changes their colour." },
	{ "timewarp", "Time Warp",
		"Each row of the picture is from a different moment: the top is now, "
			"the bottom a second and a half ago.",
		"Sound bends the border between now and then." },
	{ "textmode", "Text Mode",
		"The picture drawn in characters of the system's fixed font.",
		"A beat flashes the screen phosphor green." },
};


int32
CountEffects()
{
	return (int32)(sizeof(kEffects) / sizeof(kEffects[0]));
}


const EffectInfo&
EffectAt(int32 index)
{
	if (index < 0 || index >= CountEffects())
		index = NormalEffectIndex();
	return kEffects[index];
}


int32
FindEffect(const char* id)
{
	if (id == NULL)
		return -1;
	for (int32 i = 0; i < CountEffects(); i++) {
		if (strcmp(kEffects[i].id, id) == 0)
			return i;
	}
	return -1;
}


int32
NormalEffectIndex()
{
	static const int32 sIndex = FindEffect("normal");
	return sIndex;
}


}	// namespace snap
