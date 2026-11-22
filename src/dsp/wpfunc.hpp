/*
Copyright (c) 2006 Johan Sarge

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef WP_WPFUNC_HPP
#define WP_WPFUNC_HPP

#include <algorithm>
#include <cmath>

#define WP_STD_SAMPLE_RATE (44100.0f)

// Last sample rate set by any instance. Only for display strings (static displayers and the
// editor) that have no instance to ask. The DSP uses each instance's own SampleRate.
// shortcut: two instances at different rates can show each other's rate in the UI text,
// move the displayers onto the instance if that matters.
extern float globalSampleRate;

extern float globalMaxFrequency;

extern float globalMinPeriodLength;

#define WP_EXT_INLINE inline

WP_EXT_INLINE float sampleRateToMaxFreq(float sampleRate) {
	return (sampleRate - 100.0f)/2.0f;
}

// Sample rate of one plug-in instance and the limits derived from it. Each instance owns one
// and hands a pointer to its analyzers, synthesizers and modulators.
// Frequency normalization stays compatible with the 44.1 kHz reference range.
struct SampleRate {
	float rate = WP_STD_SAMPLE_RATE;
	float maxFrequency = sampleRateToMaxFreq(WP_STD_SAMPLE_RATE);
	float minPeriodLength = 1.0f / sampleRateToMaxFreq(WP_STD_SAMPLE_RATE);
	
	void set(float sampleRate) {
		rate = sampleRate;
		maxFrequency = std::min(sampleRateToMaxFreq(rate), sampleRateToMaxFreq(WP_STD_SAMPLE_RATE));
		minPeriodLength = 1.0f / maxFrequency;
	}
	float hzToUnsigned(float hz) const {return hz * std::min(minPeriodLength, 1.0f);}
	float unsignedToHz(float usigned) const {return std::max(usigned * maxFrequency, 1.0f);}
};

WP_EXT_INLINE void setGlobalSampleRate(float sampleRate) {
	SampleRate s;
	s.set(sampleRate);
	globalSampleRate = s.rate;
	globalMaxFrequency = s.maxFrequency;
	globalMinPeriodLength = s.minPeriodLength;
}

WP_EXT_INLINE float hzToUnsigned(float hz) {
	return hz * std::min(globalMinPeriodLength, 1.0f);
}

WP_EXT_INLINE float unsignedToHz(float usigned) {
	return std::max(usigned * globalMaxFrequency, 1.0f);
}

WP_EXT_INLINE float clamp01(float x) {return std::max(0.0f, std::min(x, 1.0f));}

WP_EXT_INLINE float fade(float x) {
	float x2 = x*x;
	return x / std::sqrt(x2*x2 - x2 + 1.0f);
}

enum SignCode {kPosPos, kPosNeg, kNegPos, kNegNeg};

WP_EXT_INLINE unsigned int signs(float a, float b) {return (a < 0.0f) << 1 | (b < 0.0f);}

class RealFunction {
private:
	float *value;
	
public:
	RealFunction(float *vp = NULL) : value(vp) {}
	
	float getValue() {return *value;}
	void setValue(float *vp) {value = vp;}
};

class FunctionFunction {
private:
	unsigned int interpolation;
	unsigned int fSize;
	float fSizeCoefficient, *function;
	
public:
	FunctionFunction() : interpolation(false), fSize(0), fSizeCoefficient(0.0f), function(NULL) {}
	
	bool getInterpolation() {return interpolation;}
	void setInterpolation(bool onOff) {interpolation = onOff;}
	
	// The buffer includes a duplicate next-cycle sample so interpolation can cross
	// the cycle boundary without a separate wrap check.
	//void setFunction(int fSizePlus1, float *f);
	//float getValue(float x);
	
	// f must point to a buffer containing fSizePlus1 samples.
	// The last sample in the buffer (at offset (fSizePlus1 - 1))
	// must be the first sample of the next waveform.
	void setFunction(int fSizePlus1, float *f) {
		fSize = (unsigned int) (fSizePlus1 - 1);
		fSizeCoefficient = 0.5f * fSize;
		function = f;
	}
	
	float getValue(float x) {
		float sampleIndexF;
		float sampleWeight = std::modf(fSizeCoefficient * (x + 3.0f), &sampleIndexF);
		float *sample = function + (unsigned int) sampleIndexF % fSize;
		
		return *sample + sampleWeight * (*(sample + interpolation) - *sample);
	}
};

#endif
