/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */

// Host tests for the platform independent audio analysis: run with
// `make check-host` on Linux, or `make check` on air/OS.


#include "audio/AudioAnalyzer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>


using snap::AudioAnalyzer;
using snap::AudioFeatures;


static int sFailures = 0;


#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
				#condition); \
			sFailures++; \
		} \
	} while (false)


static const float kRate = 48000.0f;


static std::vector<float>
Sine(float hz, float amplitude, float seconds)
{
	std::vector<float> samples((size_t)(kRate * seconds));
	for (size_t i = 0; i < samples.size(); i++)
		samples[i] = amplitude * sinf(2.0f * (float)M_PI * hz * i / kRate);
	return samples;
}


// Feeds in small, uneven chunks, as the Media Kit's buffers would.
static void
Feed(AudioAnalyzer& analyzer, const std::vector<float>& samples)
{
	size_t offset = 0;
	size_t chunk = 300;
	while (offset < samples.size()) {
		const size_t count = std::min(chunk, samples.size() - offset);
		analyzer.Process(samples.data() + offset, count);
		offset += count;
		chunk = chunk == 300 ? 457 : 300;
	}
}


static void
TestTransformOfImpulse()
{
	// An impulse transforms to a flat spectrum.
	const size_t count = 64;
	std::vector<float> real(count, 0.0f), imag(count, 0.0f);
	real[0] = 1.0f;
	snap::FastFourierTransform(real.data(), imag.data(), count);
	for (size_t i = 0; i < count; i++) {
		CHECK(fabsf(real[i] - 1.0f) < 1e-5f);
		CHECK(fabsf(imag[i]) < 1e-5f);
	}
}


static void
TestTransformOfCosine()
{
	// A cosine at bin 5 puts half its energy in bins 5 and count - 5.
	const size_t count = 256;
	std::vector<float> real(count), imag(count, 0.0f);
	for (size_t i = 0; i < count; i++)
		real[i] = cosf(2.0f * (float)M_PI * 5 * i / count);
	snap::FastFourierTransform(real.data(), imag.data(), count);
	for (size_t i = 0; i < count; i++) {
		const float magnitude = hypotf(real[i], imag[i]);
		if (i == 5 || i == count - 5)
			CHECK(fabsf(magnitude - count / 2.0f) < 1e-2f);
		else
			CHECK(magnitude < 1e-2f);
	}
}


static void
TestSilence()
{
	AudioAnalyzer analyzer;
	analyzer.SetSampleRate(kRate);
	Feed(analyzer, std::vector<float>((size_t)kRate * 2, 0.0f));
	const AudioFeatures& features = analyzer.Features();
	CHECK(features.level < 0.01f);
	CHECK(features.bass < 0.01f);
	CHECK(features.beatCount == 0);
}


static void
TestBands()
{
	// A loud bass tone lights up the bass, not the treble; a whistle the
	// other way round.
	AudioAnalyzer bass;
	bass.SetSampleRate(kRate);
	Feed(bass, Sine(80.0f, 0.5f, 1.0f));
	CHECK(bass.Features().bass > 0.8f);
	CHECK(bass.Features().treble < 0.2f);
	CHECK(bass.Features().level > 0.8f);
	CHECK(bass.Features().spectrum[2] > bass.Features().spectrum[28]);

	AudioAnalyzer whistle;
	whistle.SetSampleRate(kRate);
	Feed(whistle, Sine(4000.0f, 0.5f, 1.0f));
	CHECK(whistle.Features().treble > 0.8f);
	CHECK(whistle.Features().bass < 0.2f);
}


static void
TestQuietIsQuiet()
{
	// Room noise far below the gain's floor stays near the bottom of the
	// range instead of being amplified to full scale.
	AudioAnalyzer analyzer;
	analyzer.SetSampleRate(kRate);
	std::mt19937 random(1);
	std::normal_distribution<float> noise(0.0f, 0.0005f);
	std::vector<float> samples((size_t)kRate * 3);
	for (float& sample : samples)
		sample = noise(random);
	Feed(analyzer, samples);
	CHECK(analyzer.Features().level < 0.25f);
	CHECK(analyzer.Features().beatCount == 0);
}


static void
TestClapsAreBeats()
{
	// Eight short noise bursts half a second apart over quiet noise: eight
	// beats, however the bursts fall across the analysis windows.
	AudioAnalyzer analyzer;
	analyzer.SetSampleRate(kRate);
	std::mt19937 random(7);
	std::normal_distribution<float> hiss(0.0f, 0.001f);
	std::normal_distribution<float> clap(0.0f, 0.4f);
	std::vector<float> samples((size_t)(kRate * 4.3f));
	for (size_t i = 0; i < samples.size(); i++) {
		const float t = i / kRate - 0.3f;
		const float phase = fmodf(t, 0.5f);
		const bool inClap = t >= 0 && phase < 0.03f;
		samples[i] = inClap ? clap(random) * expf(-phase * 120.0f)
			: hiss(random);
	}
	Feed(analyzer, samples);
	printf("  claps: %u beats for 8 claps\n", analyzer.Features().beatCount);
	CHECK(analyzer.Features().beatCount == 8);
}


static void
TestSteadyToneIsNoBeat()
{
	// A tone that starts once is at most one onset, never a stream of them.
	AudioAnalyzer analyzer;
	analyzer.SetSampleRate(kRate);
	Feed(analyzer, Sine(440.0f, 0.3f, 3.0f));
	CHECK(analyzer.Features().beatCount <= 1);
}


static void
TestReset()
{
	AudioAnalyzer analyzer;
	analyzer.SetSampleRate(kRate);
	Feed(analyzer, Sine(200.0f, 0.5f, 0.5f));
	CHECK(analyzer.Features().level > 0.5f);
	analyzer.Reset();
	CHECK(analyzer.Features().level == 0.0f);
	CHECK(analyzer.Features().spectrum[5] == 0.0f);
}


int
main()
{
	TestTransformOfImpulse();
	TestTransformOfCosine();
	TestSilence();
	TestBands();
	TestQuietIsQuiet();
	TestClapsAreBeats();
	TestSteadyToneIsNoBeat();
	TestReset();
	if (sFailures == 0)
		printf("audio analyzer: all tests passed\n");
	return sFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
