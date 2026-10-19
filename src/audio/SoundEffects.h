/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_SOUND_EFFECTS_H
#define SNAP_SOUND_EFFECTS_H


class BSimpleGameSound;


namespace snap {


// The countdown's ticks and the shutter's click. Both are synthesized when
// Snap starts (no sound files to ship) and played with the Game Kit's
// BSimpleGameSound, the simplest way to fire off a short sound on Haiku: it
// keeps the samples in memory and mixes them in whenever StartPlaying() is
// called.
class SoundEffects {
public:
								SoundEffects();
								~SoundEffects();

			void				PlayTick(bool last);
			void				PlayShutter();

private:
			BSimpleGameSound*	fTick;
			BSimpleGameSound*	fLastTick;
			BSimpleGameSound*	fShutter;
};


}	// namespace snap


#endif	// SNAP_SOUND_EFFECTS_H
