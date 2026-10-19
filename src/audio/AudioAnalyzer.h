/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */
#ifndef SNAP_AUDIO_ANALYZER_H
#define SNAP_AUDIO_ANALYZER_H


#include <cstddef>
#include <cstdint>
#include <vector>


namespace snap {


// What the effects know about the sound around them, refreshed about 90 times
// a second. Every value is normalised to 0..1 by an automatic gain control, so
// a quiet room and a loud party both drive the effects over their full range.
struct AudioFeatures {
	static const int	kSpectrumBands = 32;
	static const int	kWaveformPoints = 128;

	float				level;		// overall loudness
	float				bass;		// 20 - 250 Hz
	float				mid;		// 250 - 2000 Hz
	float				treble;		// 2 - 12 kHz
	float				beat;		// 1 at an onset, decaying to 0
	uint32_t			beatCount;	// onsets seen so far
	float				spectrum[kSpectrumBands];	// log-spaced bands
	float				waveform[kWaveformPoints];	// -1..1, newest last

						AudioFeatures();
			void		Clear();
};


// Turns a stream of mono samples into AudioFeatures: a Hann-windowed FFT
// every 512 samples, band energies from the spectrum, onsets from the rise of
// spectral energy (spectral flux) above its recent average.
//
// The analyzer is plain C++ with no locking and no Haiku dependencies, so it
// is unit-tested on the build host (tests/AudioAnalyzerTests.cpp). AudioTap
// owns one, feeds it from the Media Kit's thread and hands copies of
// Features() to the renderer under its own lock.
class AudioAnalyzer {
public:
	static const size_t	kWindowSize = 1024;
	static const size_t	kHopSize = 512;

								AudioAnalyzer();

			void				SetSampleRate(float rate);

	// Feeds mono samples in -1..1. Analyses as many windows as the samples
	// complete; returns true when Features() changed.
			bool				Process(const float* samples, size_t count);

	// Lets the features fall back to silence, for when the input stops.
			void				Reset();

			const AudioFeatures& Features() const { return fFeatures; }

private:
			void				_AnalyzeWindow();
			float				_BandEnergy(float lowHz, float highHz) const;
			float				_Normalize(float value, float& peak) const;

			float				fSampleRate;
			std::vector<float>	fWindow;		// Hann coefficients
			std::vector<float>	fHistory;		// last kWindowSize samples
			size_t				fHistoryFill;
			size_t				fSinceAnalysis;
			std::vector<float>	fReal;
			std::vector<float>	fImag;
			std::vector<float>	fMagnitude;		// kWindowSize / 2 bins
			std::vector<float>	fPreviousMagnitude;

			// Automatic gain: a slowly falling peak per feature.
			float				fLevelPeak;
			float				fBassPeak;
			float				fMidPeak;
			float				fTreblePeak;
			float				fSpectrumPeak;

			// Onset detection: the recent spectral flux and a refractory
			// period, so one clap is one beat.
			std::vector<float>	fFluxHistory;
			size_t				fFluxIndex;
			size_t				fFluxCount;		// analyses since Reset()
			int					fFramesSinceBeat;

			AudioFeatures		fFeatures;
};


// The in-place radix-2 FFT the analyzer uses; real and imag hold
// count = 2^n values. Exposed for the unit tests.
void FastFourierTransform(float* real, float* imag, size_t count);


}	// namespace snap


#endif	// SNAP_AUDIO_ANALYZER_H
