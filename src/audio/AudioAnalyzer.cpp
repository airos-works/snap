/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "AudioAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>


namespace snap {


namespace {

// A peak falls to half in about 2.5 s (one analysis every ~11.6 ms at
// 44.1 kHz), so the gain follows a song getting quieter without pumping on
// every pause between words.
const float kPeakDecay = 0.9973f;

// The gain never grows past what these peaks allow: below them a sound is
// "quiet" rather than stretched to full scale. Linear amplitudes of the
// window's RMS and of the band magnitudes (a full-scale sine gives ~0.5).
const float kMinLevelPeak = 0.02f;		// about -34 dBFS
const float kMinBandPeak = 0.004f;

// The spectrum bars span 48 dB below the loudest recent band.
const float kSpectrumRangeDb = 48.0f;
const float kMinSpectrumPeakDb = -40.0f;
const float kSpectrumPeakFallDb = 0.06f;	// per analysis, ~5 dB/s

// Onsets: flux above kOnsetRatio times its mean over the last ~0.5 s, and at
// least kOnsetFloor (log-compressed magnitudes summed over the bins), at most
// one per kRefractoryFrames analyses (~170 ms).
const size_t kFluxHistory = 43;
const float kOnsetRatio = 1.6f;
const float kOnsetFloor = 6.0f;
const int kRefractoryFrames = 15;
const size_t kWarmUpFrames = 16;
const float kBeatDecay = 0.86f;

const float kSpectrumLowHz = 40.0f;
const float kSpectrumHighHz = 16000.0f;


float
Smooth(float current, float target)
{
	// Fast attack, slower release: hits show at once and fade out gently.
	const float rate = target > current ? 0.6f : 0.18f;
	return current + (target - current) * rate;
}

}	// namespace


AudioFeatures::AudioFeatures()
{
	Clear();
}


void
AudioFeatures::Clear()
{
	level = bass = mid = treble = beat = 0;
	beatCount = 0;
	std::fill(spectrum, spectrum + kSpectrumBands, 0.0f);
	std::fill(waveform, waveform + kWaveformPoints, 0.0f);
}


void
FastFourierTransform(float* real, float* imag, size_t count)
{
	// Bit-reversal permutation.
	for (size_t i = 1, j = 0; i < count; i++) {
		size_t bit = count >> 1;
		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j) {
			std::swap(real[i], real[j]);
			std::swap(imag[i], imag[j]);
		}
	}

	// Butterflies, doubling the transform length each pass.
	for (size_t length = 2; length <= count; length <<= 1) {
		const double angle = -2.0 * M_PI / length;
		const float stepReal = (float)cos(angle);
		const float stepImag = (float)sin(angle);
		for (size_t start = 0; start < count; start += length) {
			float twiddleReal = 1.0f;
			float twiddleImag = 0.0f;
			for (size_t k = 0; k < length / 2; k++) {
				const size_t even = start + k;
				const size_t odd = even + length / 2;
				const float oddReal = real[odd] * twiddleReal
					- imag[odd] * twiddleImag;
				const float oddImag = real[odd] * twiddleImag
					+ imag[odd] * twiddleReal;
				real[odd] = real[even] - oddReal;
				imag[odd] = imag[even] - oddImag;
				real[even] += oddReal;
				imag[even] += oddImag;

				const float nextReal = twiddleReal * stepReal
					- twiddleImag * stepImag;
				twiddleImag = twiddleReal * stepImag + twiddleImag * stepReal;
				twiddleReal = nextReal;
			}
		}
	}
}


AudioAnalyzer::AudioAnalyzer()
	:
	fSampleRate(44100.0f),
	fWindow(kWindowSize),
	fHistory(kWindowSize, 0.0f),
	fHistoryFill(0),
	fSinceAnalysis(0),
	fReal(kWindowSize),
	fImag(kWindowSize),
	fMagnitude(kWindowSize / 2, 0.0f),
	fPreviousMagnitude(kWindowSize / 2, 0.0f),
	fFluxHistory(kFluxHistory, 0.0f),
	fFluxIndex(0),
	fFluxCount(0),
	fFramesSinceBeat(kRefractoryFrames)
{
	for (size_t i = 0; i < kWindowSize; i++)
		fWindow[i] = 0.5f - 0.5f * (float)cos(2.0 * M_PI * i / (kWindowSize - 1));
	Reset();
}


void
AudioAnalyzer::SetSampleRate(float rate)
{
	if (rate > 1000.0f)
		fSampleRate = rate;
}


void
AudioAnalyzer::Reset()
{
	std::fill(fHistory.begin(), fHistory.end(), 0.0f);
	std::fill(fPreviousMagnitude.begin(), fPreviousMagnitude.end(), 0.0f);
	std::fill(fFluxHistory.begin(), fFluxHistory.end(), 0.0f);
	fHistoryFill = 0;
	fSinceAnalysis = 0;
	fFluxCount = 0;
	fLevelPeak = kMinLevelPeak;
	fBassPeak = fMidPeak = fTreblePeak = kMinBandPeak;
	fSpectrumPeak = kMinSpectrumPeakDb;
	fFramesSinceBeat = kRefractoryFrames;
	const uint32_t beats = fFeatures.beatCount;
	fFeatures.Clear();
	fFeatures.beatCount = beats;
}


bool
AudioAnalyzer::Process(const float* samples, size_t count)
{
	bool changed = false;
	while (count > 0) {
		// The history is a sliding window: the newest sample last. Moving it
		// by a hop at a time keeps the copy cheap (512 floats per analysis).
		const size_t room = kHopSize - fSinceAnalysis;
		const size_t take = std::min(room, count);
		std::memmove(fHistory.data(), fHistory.data() + take,
			(kWindowSize - take) * sizeof(float));
		std::memcpy(fHistory.data() + kWindowSize - take, samples,
			take * sizeof(float));
		samples += take;
		count -= take;
		fSinceAnalysis += take;
		fHistoryFill = std::min(kWindowSize, fHistoryFill + take);

		if (fSinceAnalysis == kHopSize) {
			fSinceAnalysis = 0;
			_AnalyzeWindow();
			changed = true;
		}
	}
	return changed;
}


void
AudioAnalyzer::_AnalyzeWindow()
{
	AudioFeatures& features = fFeatures;

	// Loudness from the plain samples, and the oscilloscope trace.
	double sumSquares = 0;
	for (size_t i = 0; i < kWindowSize; i++)
		sumSquares += fHistory[i] * fHistory[i];
	const float rms = (float)sqrt(sumSquares / kWindowSize);
	const size_t step = kWindowSize / AudioFeatures::kWaveformPoints;
	for (int i = 0; i < AudioFeatures::kWaveformPoints; i++)
		features.waveform[i] = std::clamp(fHistory[i * step] / std::max(fLevelPeak * 1.4f, 0.05f),
			-1.0f, 1.0f);

	// The spectrum.
	for (size_t i = 0; i < kWindowSize; i++) {
		fReal[i] = fHistory[i] * fWindow[i];
		fImag[i] = 0.0f;
	}
	FastFourierTransform(fReal.data(), fImag.data(), kWindowSize);
	const float scale = 2.0f / kWindowSize;
	for (size_t i = 0; i < kWindowSize / 2; i++) {
		fMagnitude[i] = scale
			* (float)sqrt(fReal[i] * fReal[i] + fImag[i] * fImag[i]);
	}

	features.level = Smooth(features.level,
		sqrtf(_Normalize(rms, fLevelPeak)));
	features.bass = Smooth(features.bass,
		_Normalize(_BandEnergy(20, 250), fBassPeak));
	features.mid = Smooth(features.mid,
		_Normalize(_BandEnergy(250, 2000), fMidPeak));
	features.treble = Smooth(features.treble,
		_Normalize(_BandEnergy(2000, 12000), fTreblePeak));

	// Spectrum bars in decibels below the loudest recent band.
	const float binHz = fSampleRate / kWindowSize;
	const float highHz = std::min(kSpectrumHighHz, fSampleRate * 0.5f);
	const float ratio = highHz / kSpectrumLowHz;
	float loudestDb = -120.0f;
	float bandDb[AudioFeatures::kSpectrumBands];
	for (int band = 0; band < AudioFeatures::kSpectrumBands; band++) {
		const float low = kSpectrumLowHz
			* powf(ratio, (float)band / AudioFeatures::kSpectrumBands);
		const float high = kSpectrumLowHz
			* powf(ratio, (float)(band + 1) / AudioFeatures::kSpectrumBands);
		size_t first = (size_t)(low / binHz);
		size_t last = std::max(first, (size_t)(high / binHz));
		last = std::min(last, kWindowSize / 2 - 1);
		first = std::min(first, last);
		float peak = 0;
		for (size_t bin = first; bin <= last; bin++)
			peak = std::max(peak, fMagnitude[bin]);
		bandDb[band] = 20.0f * log10f(peak + 1e-7f);
		loudestDb = std::max(loudestDb, bandDb[band]);
	}
	fSpectrumPeak = std::max({ loudestDb, fSpectrumPeak - kSpectrumPeakFallDb,
		kMinSpectrumPeakDb });
	for (int band = 0; band < AudioFeatures::kSpectrumBands; band++) {
		const float value = std::clamp(
			(bandDb[band] - (fSpectrumPeak - kSpectrumRangeDb))
				/ kSpectrumRangeDb, 0.0f, 1.0f);
		features.spectrum[band] = Smooth(features.spectrum[band], value);
	}

	// Onsets: the summed rise of log-compressed magnitudes (spectral flux),
	// compared with its own recent mean.
	float flux = 0;
	for (size_t bin = 1; bin < kWindowSize / 2; bin++) {
		const float now = log1pf(1000.0f * fMagnitude[bin]);
		const float before = log1pf(1000.0f * fPreviousMagnitude[bin]);
		if (now > before)
			flux += now - before;
	}
	std::swap(fMagnitude, fPreviousMagnitude);

	// The mean of the history filled so far (all of it after ~0.5 s).
	const size_t known = std::min(fFluxCount, kFluxHistory);
	float mean = 0;
	for (size_t i = 0; i < known; i++)
		mean += fFluxHistory[i];
	mean = known > 0 ? mean / known : 0;
	fFluxHistory[fFluxIndex] = flux;
	fFluxIndex = (fFluxIndex + 1) % kFluxHistory;
	fFluxCount++;

	// The first analyses only learn what "usual" is: the step from the empty
	// history to the room's noise is not a beat.
	fFramesSinceBeat++;
	features.beat *= kBeatDecay;
	if (fFluxCount > kWarmUpFrames && flux > kOnsetFloor
		&& flux > mean * kOnsetRatio && fFramesSinceBeat >= kRefractoryFrames) {
		features.beat = 1.0f;
		features.beatCount++;
		fFramesSinceBeat = 0;
	}
}


float
AudioAnalyzer::_BandEnergy(float lowHz, float highHz) const
{
	const float binHz = fSampleRate / kWindowSize;
	size_t first = std::max<size_t>(1, (size_t)(lowHz / binHz));
	size_t last = std::min(kWindowSize / 2 - 1, (size_t)(highHz / binHz));
	double sum = 0;
	for (size_t bin = first; bin <= last; bin++)
		sum += fMagnitude[bin] * fMagnitude[bin];
	return (float)sqrt(sum);
}


float
AudioAnalyzer::_Normalize(float value, float& peak) const
{
	const float floor = &peak == &fLevelPeak ? kMinLevelPeak : kMinBandPeak;
	peak = std::max({ value, peak * kPeakDecay, floor });
	return std::clamp(value / peak, 0.0f, 1.0f);
}


}	// namespace snap
