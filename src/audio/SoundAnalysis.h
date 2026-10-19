/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_SOUND_ANALYSIS_H
#define SNAP_SOUND_ANALYSIS_H


#include <Locker.h>
#include <MediaDefs.h>

#include <vector>

#include "AudioAnalyzer.h"


namespace snap {


// Where sound meets picture. Whichever sound input is on (the microphone, or
// music Snap plays) feeds its buffers in from its Media Kit thread; the
// render thread takes a copy of the latest AudioFeatures once a frame. The
// lock is held only to analyse one buffer or to copy the features.
class SoundAnalysis {
public:
								SoundAnalysis();

	// Feeding side: a buffer in any raw audio format the Media Kit uses
	// (float, 32, 16 or 8 bit integer; any number of channels).
			void				Feed(const void* data, size_t size,
									const media_raw_audio_format& format);
	// Forgets everything (the input changed or stopped).
			void				Reset();

	// Rendering side. When nothing was fed for a quarter of a second the
	// features fall to silence, so a stopped input does not freeze the
	// effects mid-beat.
			AudioFeatures		Snapshot();

private:
			BLocker				fLock;
			AudioAnalyzer		fAnalyzer;
			bigtime_t			fLastFeed;
			std::vector<float>	fMono;
};


// Mixes a buffer down to mono floats in -1..1. Exposed for the tests.
size_t MixDownToMono(const void* data, size_t size,
	const media_raw_audio_format& format, std::vector<float>& mono);


}	// namespace snap


#endif	// SNAP_SOUND_ANALYSIS_H
