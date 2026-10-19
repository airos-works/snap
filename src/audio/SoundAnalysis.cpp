/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "SoundAnalysis.h"

#include <Autolock.h>
#include <OS.h>


namespace snap {


static const bigtime_t kSilenceAfter = 250000;


size_t
MixDownToMono(const void* data, size_t size,
	const media_raw_audio_format& format, std::vector<float>& mono)
{
	const uint32 channels = format.channel_count > 0 ? format.channel_count : 1;
	const size_t sampleSize = format.format & media_raw_audio_format::B_AUDIO_SIZE_MASK;
	if (sampleSize == 0)
		return 0;
	const size_t frames = size / (sampleSize * channels);
	mono.resize(frames);

	for (size_t frame = 0; frame < frames; frame++) {
		float sum = 0;
		for (uint32 channel = 0; channel < channels; channel++) {
			const size_t index = frame * channels + channel;
			switch (format.format) {
				case media_raw_audio_format::B_AUDIO_FLOAT:
					sum += ((const float*)data)[index];
					break;
				case media_raw_audio_format::B_AUDIO_INT:
					sum += ((const int32*)data)[index] / 2147483648.0f;
					break;
				case media_raw_audio_format::B_AUDIO_SHORT:
					sum += ((const int16*)data)[index] / 32768.0f;
					break;
				case media_raw_audio_format::B_AUDIO_CHAR:
					sum += ((const int8*)data)[index] / 128.0f;
					break;
				case media_raw_audio_format::B_AUDIO_UCHAR:
					sum += (((const uint8*)data)[index] - 128) / 128.0f;
					break;
			}
		}
		mono[frame] = sum / channels;
	}
	return frames;
}


SoundAnalysis::SoundAnalysis()
	:
	fLock("sound analysis"),
	fLastFeed(0)
{
}


void
SoundAnalysis::Feed(const void* data, size_t size,
	const media_raw_audio_format& format)
{
	BAutolock _(fLock);
	const size_t frames = MixDownToMono(data, size, format, fMono);
	if (frames == 0)
		return;
	fAnalyzer.SetSampleRate(format.frame_rate);
	fAnalyzer.Process(fMono.data(), frames);
	fLastFeed = system_time();
}


void
SoundAnalysis::Reset()
{
	BAutolock _(fLock);
	fAnalyzer.Reset();
	fLastFeed = 0;
}


AudioFeatures
SoundAnalysis::Snapshot()
{
	BAutolock _(fLock);
	if (system_time() - fLastFeed > kSilenceAfter) {
		AudioFeatures silence;
		silence.beatCount = fAnalyzer.Features().beatCount;
		return silence;
	}
	return fAnalyzer.Features();
}


}	// namespace snap
