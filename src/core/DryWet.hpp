#pragma once

#include <algorithm>

// Dry/wet crossfade of two channels. The mix moves along a short linear ramp, so turning the control
// does not click. The core has no latency, so the dry signal needs no delay.
class DryWet {
public:
	void setTarget(float amount) {target = std::min(1.0f, std::max(0.0f, amount));}
	// Call after setTarget when playback starts: the mix then begins at its target instead of ramping.
	void prepare(float sampleRate) {
		rampStep = 1.0f / std::max(1.0f, 0.02f * sampleRate);
		current = target;
	}
	// Fully wet and not moving: the caller can skip the dry copy and the mix.
	bool isTransparent() const {return current == 1.0f && target == 1.0f;}

	// Replaces wet[ch][i] with its crossfade towards dry[ch][i]: 0 is all dry, 1 all wet.
	void mix(float *const wet[2], const float *const dry[2], int count) {
		for (int i = 0; i < count; i++) {
			if (current < target)
				current = std::min(target, current + rampStep);
			else if (current > target)
				current = std::max(target, current - rampStep);
			for (int ch = 0; ch < 2; ch++)
				wet[ch][i] = dry[ch][i] + (wet[ch][i] - dry[ch][i]) * current;
		}
	}

private:
	float current = 1.0f, target = 1.0f, rampStep = 1.0f;
};
