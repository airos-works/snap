/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_EFFECT_CATALOG_H
#define SNAP_EFFECT_CATALOG_H


#include <SupportDefs.h>


namespace snap {


// The effects Snap offers, in the order of the effect grid (four across,
// three down) and the Effects menu. The interface reads these names and
// descriptions; the renderer builds the matching Effect objects on its own
// thread (CreateEffect() in Effects.cpp).
struct EffectInfo {
	const char*			id;				// stable, stored in settings and photos
	const char*			name;			// shown to the user
	const char*			description;
	const char*			soundReaction;	// NULL when the effect ignores sound
};


int32				CountEffects();
const EffectInfo&	EffectAt(int32 index);
// The index of the effect with that id, or -1.
int32				FindEffect(const char* id);
// The plain camera picture.
int32				NormalEffectIndex();


}	// namespace snap


#endif	// SNAP_EFFECT_CATALOG_H
