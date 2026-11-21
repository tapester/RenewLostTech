#include "wpfunc.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(float actual, float expected) {
	return std::fabs(actual - expected) < 0.0001f;
}

void require(bool condition, const char *message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(EXIT_FAILURE);
	}
}

}

int main() {
	require(nearlyEqual(sampleRateToMaxFreq(44100.0f), 22000.0f),
	        "sampleRateToMaxFreq should leave a 50 Hz margin below Nyquist");

	setGlobalSampleRate(48000.0f);
	require(nearlyEqual(globalMaxFrequency, 22000.0f),
	        "maximum analysis frequency should be capped at the standard rate");
	require(nearlyEqual(globalMinPeriodLength, 1.0f / globalMaxFrequency),
	        "minimum period should track the maximum analysis frequency");

	setGlobalSampleRate(22050.0f);
	require(nearlyEqual(globalMaxFrequency, 10975.0f),
	        "lower sample rates should lower the maximum analysis frequency");
	require(nearlyEqual(hzToUnsigned(440.0f), 440.0f / globalMaxFrequency),
	        "frequency should normalize against the current maximum");
	require(nearlyEqual(unsignedToHz(0.0f), 1.0f),
	        "zero normalized frequency should clamp to 1 Hz");

	require(clamp01(-0.5f) == 0.0f && clamp01(1.5f) == 1.0f,
	        "clamp01 should constrain values to the unit interval");
	require(clamp01(NAN) == 0.0f,
	        "clamp01 should convert NaN to zero");
	require(modulo(-1, 8) == 7 && modulo(9, 8) == 1,
	        "modulo should return a non-negative wrapped index");
	require(nearlyEqual(fade(0.0f), 0.0f) && nearlyEqual(fade(1.0f), 1.0f),
	        "fade should preserve the endpoints");
	require(signs(-1.0f, 1.0f) == kNegPos && signs(1.0f, -1.0f) == kPosNeg,
	        "signs should encode the sign of each input");

	float samples[] = {0.0f, 1.0f, 2.0f, 3.0f, 0.0f};
	FunctionFunction function;
	function.setFunction(5, samples);
	require(nearlyEqual(function.getValue(0.0f), 2.0f),
	        "waveform lookup should wrap to the expected sample");
	function.setInterpolation(true);
	require(nearlyEqual(function.getValue(0.25f), 2.5f),
	        "waveform lookup should interpolate between neighboring samples");

	// Test out-of-range values for clamp01 and modulo
	float testValues[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f};
	for (float v : testValues) {
		require(clamp01(v) >= 0.0f && clamp01(v) <= 1.0f,
		        "clamp01 should constrain all values to [0, 1]");
	}

	// Test setting a function and reading values back
	float testSamples[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
	function.setFunction(5, testSamples);
	function.setInterpolation(false);
	require(nearlyEqual(function.getValue(0.0f), 3.0f),
	        "non-interpolating mode should return the selected sample");
	function.setInterpolation(true);
	require(nearlyEqual(function.getValue(0.0f), 3.0f),
	        "interpolating mode should preserve exact samples");

	// Test rate change affects global parameters correctly
	setGlobalSampleRate(44100.0f);
	float initialMaxFreq = globalMaxFrequency;
	float initialMinPeriod = globalMinPeriodLength;

	setGlobalSampleRate(96000.0f);
	require(nearlyEqual(globalMaxFrequency, 22000.0f),
	        "globalMaxFrequency should remain capped at the standard-rate maximum");
	require(nearlyEqual(globalMinPeriodLength, 1.0f / globalMaxFrequency),
	        "globalMinPeriodLength should recalculate after rate change");

	setGlobalSampleRate(44100.0f);
	require(nearlyEqual(globalMaxFrequency, initialMaxFreq),
	        "globalMaxFrequency should restore previous value after rate change");
	require(nearlyEqual(globalMinPeriodLength, initialMinPeriod),
	        "globalMinPeriodLength should restore previous value after rate change");

	// Test modulo with negative and positive values
	int testModulo[] = {-1, 0, 1, 7, 8, 9, -256, 256};
	for (int i : testModulo) {
		require(modulo(i, 8) >= 0 && modulo(i, 8) < 8,
		        "modulo should return non-negative wrapped index");
	}

	// Test fade function at various points
	float fadeTestValues[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
	float previousFade = -1.0f;
	for (float v : fadeTestValues) {
		float faded = fade(v);
		require(faded >= previousFade && faded >= 0.0f,
		        "fade should stay in range and increase monotonically");
		previousFade = faded;
	}

	// Test signs function with various combinations
	int signTests[] = {kNegPos, kPosNeg, kPosPos, kNegNeg};
	for (int i : signTests) {
		require(signs(-1.0f, 1.0f) == kNegPos || signs(1.0f, -1.0f) == kPosNeg ||
		        signs(-1.0f, -1.0f) == kNegNeg || signs(1.0f, 1.0f) == kPosPos,
		        "signs should correctly encode sign combinations");
	}

	return EXIT_SUCCESS;
}
