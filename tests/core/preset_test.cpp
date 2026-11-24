// Checks the factory presets: valid names and values, preset 0 equal to the defaults, and
// finite, bounded audio from every preset at two sample rates (the golden test only covers the defaults).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "LostTechCore.hpp"

static int failures = 0;

static void check(bool ok, const char *what, int preset) {
	if (!ok) {
		std::printf("FAIL preset %d (%s): %s\n", preset, LostTechCore::getPresetName(preset), what);
		failures++;
	}
}

// Renders two seconds of two different tones; returns the largest output magnitude, or NAN.
static float peakOutput(int preset, float rate) {
	LostTechCore p;
	p.setSampleRate(rate);
	for (int i = kPlugVersion + 1; i < kNumAllParams; i++)
		p.setParameter(i, LostTechCore::getPresetValue(preset, i));
	p.resume();
	std::vector<float> a(512), b(512), oa(512), ob(512);
	float *in[2] = {a.data(), b.data()}, *out[2] = {oa.data(), ob.data()};
	float peak = 0;
	long n = 0;
	for (int block = 0; block < (int) (2 * rate / 512); block++) {
		for (int i = 0; i < 512; i++, n++) {
			double t = n / (double) rate;
			a[i] = (float) (0.5 * std::sin(6.2831853 * 220 * t) + 0.15 * std::sin(6.2831853 * 660 * t));
			b[i] = (float) (0.4 * std::sin(6.2831853 * 330 * t));
		}
		p.process(in, out, 512);
		for (int i = 0; i < 512; i++)
			for (float s : {oa[i], ob[i]}) {
				if (!std::isfinite(s)) return NAN;
				peak = std::fmax(peak, std::fabs(s));
			}
	}
	p.suspend();
	return peak;
}

int main() {
	const int count = LostTechCore::getNumPresets();
	check(count > 0 && count <= 24, "preset count", 0);
	for (int preset = 0; preset < count; preset++) {
		check(std::strlen(LostTechCore::getPresetName(preset)) <= 24 &&
		      std::strlen(LostTechCore::getPresetName(preset)) > 0, "name length", preset);
		bool changed = false;
		for (int i = 0; i < kNumAllParams; i++) {
			float v = LostTechCore::getPresetValue(preset, i);
			check(v >= 0.0f && v <= 1.0f, "value outside 0..1", preset);
			changed |= v != LostTechCore::getParameterDefault(i);
		}
		check(changed == (preset != 0), "only preset 0 may equal the defaults", preset);
		for (float rate : {44100.0f, 96000.0f}) {
			float peak = peakOutput(preset, rate);
			check(std::isfinite(peak) && peak <= 4.0f, "output not finite or too loud", preset);
		}
	}
	check(LostTechCore::getPresetValue(count, 2) == LostTechCore::getParameterDefault(2), "unknown preset", count);
	std::printf("%d presets checked, %d failures\n", count, failures);
	return failures ? 1 : 0;
}
