/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "SoundEffects.h"

#include <MediaDefs.h>
#include <SimpleGameSound.h>

#include <cmath>
#include <random>
#include <vector>


namespace snap {


static const float kRate = 44100.0f;


namespace {


// A soft beep: a sine with a quick attack and an exponential fade.
std::vector<float>
Beep(float frequency, float seconds)
{
	std::vector<float> samples((size_t)(kRate * seconds));
	for (size_t i = 0; i < samples.size(); i++) {
		const float t = i / kRate;
		const float envelope = std::min(1.0f, t * 400.0f) * expf(-t * 18.0f);
		samples[i] = 0.35f * envelope * (sinf(2.0f * (float)M_PI * frequency * t)
			+ 0.25f * sinf(4.0f * (float)M_PI * frequency * t));
	}
	return samples;
}


// A camera shutter: the blades opening (a short burst of filtered noise with
// a knock under it), and closing again 80 ms later.
std::vector<float>
Shutter()
{
	std::vector<float> samples((size_t)(kRate * 0.25f), 0.0f);
	std::mt19937 random(42);
	std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
	for (float start : { 0.0f, 0.08f }) {
		float low = 0;
		const size_t first = (size_t)(start * kRate);
		for (size_t i = first; i < samples.size(); i++) {
			const float t = (i - first) / kRate;
			const float white = noise(random);
			low += (white - low) * 0.35f;	// takes the hiss off
			const float click = (white - low) * expf(-t * 260.0f);
			const float knock = sinf(2.0f * (float)M_PI * 140.0f * t)
				* expf(-t * 70.0f);
			samples[i] += 0.5f * click + 0.35f * knock;
		}
	}
	return samples;
}


BSimpleGameSound*
MakeSound(const std::vector<float>& samples)
{
	gs_audio_format format;
	format.frame_rate = kRate;
	format.channel_count = 1;
	format.format = gs_audio_format::B_GS_F;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = 0;
	// BSimpleGameSound copies the samples.
	BSimpleGameSound* sound = new BSimpleGameSound(samples.data(),
		samples.size(), &format);
	if (sound->InitCheck() != B_OK) {
		delete sound;
		return NULL;
	}
	return sound;
}


}	// namespace


SoundEffects::SoundEffects()
	:
	fTick(MakeSound(Beep(880.0f, 0.12f))),
	fLastTick(MakeSound(Beep(1320.0f, 0.18f))),
	fShutter(MakeSound(Shutter()))
{
}


SoundEffects::~SoundEffects()
{
	delete fTick;
	delete fLastTick;
	delete fShutter;
}


void
SoundEffects::PlayTick(bool last)
{
	BSimpleGameSound* sound = last ? fLastTick : fTick;
	if (sound != NULL)
		sound->StartPlaying();
}


void
SoundEffects::PlayShutter()
{
	if (fShutter != NULL)
		fShutter->StartPlaying();
}


}	// namespace snap
