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
}
