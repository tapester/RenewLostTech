// Renders a fixed scenario through LostTechCore and compares every output sample with
// golden_reference.bin, which was rendered by the original VST2 plug-in before the core was
// split out. Guards refactors and the JUCE migration. Usage: golden_test <reference.bin> [--write]
// shortcut: the tolerance is absolute and fixed; tighten it, or compare bit for bit, if drift
// between compilers never shows up in practice.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "LostTechCore.hpp"

static void setBoth(LostTechCore &p, int i, float v) {
	p.setParameter(kNumMonoParams + i, v);
	p.setParameter(kNumMonoParams + kNumParams + i, v);
}

static std::vector<float> render() {
	std::vector<float> out;
	LostTechCore p;
	p.setSampleRate(44100.0f);
	p.resume();
	std::vector<float> a(512), b(512), oa(512), ob(512);
	float *in[2] = {a.data(), b.data()}, *o[2] = {oa.data(), ob.data()};
	long n = 0;
	double sr = 44100;
	auto blocks = [&](int k) {
		for (int j = 0; j < k; j++) {
			for (int i = 0; i < 512; i++, n++) {
				double t = n / sr;
				a[i] = (float) (0.5 * std::sin(6.2831853 * 220 * t) + 0.15 * std::sin(6.2831853 * 660 * t));
				b[i] = (float) (0.4 * std::sin(6.2831853 * 330 * t));
			}
			p.process(in, o, 512);
			out.insert(out.end(), oa.begin(), oa.end());
			out.insert(out.end(), ob.begin(), ob.end());
		}
	};
	blocks(40);
	setBoth(p, kSmooWin, 0.6f); blocks(40);
	setBoth(p, kOversmpl, 0.5f); blocks(20);
	p.setParameter(kBufferSize, 5.0f / 7.0f); blocks(40);
	p.suspend(); p.resume(); blocks(20);
	p.setParameter(kBufferSize, 1.0f / 7.0f); blocks(40);
	setBoth(p, kSmooWin, 1.0f); blocks(20);
	p.suspend(); p.setSampleRate(96000.0f); p.resume(); sr = 96000; blocks(60);
	return out;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: golden_test <reference.bin> [--write]\n");
		return 2;
	}
	std::vector<float> got = render();
	if (argc > 2 && !std::strcmp(argv[2], "--write")) {
		FILE *f = std::fopen(argv[1], "wb");
		if (!f || std::fwrite(got.data(), 4, got.size(), f) != got.size()) return 2;
		std::fclose(f);
		return 0;
	}
	std::vector<float> want(got.size() + 1);
	FILE *f = std::fopen(argv[1], "rb");
	if (!f) {
		std::fprintf(stderr, "cannot open %s\n", argv[1]);
		return 2;
	}
	size_t n = std::fread(want.data(), 4, want.size(), f);
	std::fclose(f);
	if (n != got.size()) {
		std::fprintf(stderr, "reference has %zu samples, rendered %zu\n", n, got.size());
		return 1;
	}
	double worst = 0;
	for (size_t i = 0; i < n; i++) {
		double d = std::fabs((double) got[i] - want[i]);
		if (!(d <= worst)) worst = d; // also catches NaN
	}
	std::printf("max abs difference over %zu samples: %g\n", n, worst);
	return worst <= 1e-4 ? 0 : 1;
}
