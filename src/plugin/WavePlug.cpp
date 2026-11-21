/*
Copyright (c) 2006-2007 Johan Sarge

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

#include "WavePlug.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include "wpfunc.hpp"
#ifndef WP_NO_GUI
#include "WavePlugEditor.hpp"
#endif

// Debugging: trap floating-point exceptions on MSVC debug builds. This is a no-op on other compilers and build configurations.
/*
#include <float.h>
#if defined(_DEBUG) && defined(_MSC_VER)
   struct FpTrap {
       unsigned prev, cur;
       FpTrap()  { _controlfp_s(&prev, 0, 0);
                   _controlfp_s(&cur, 0, _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW); }
       ~FpTrap() { _clearfp(); _controlfp_s(&cur, prev, _MCW_EM); }
   };   
#endif
*/

// Flush denormals to zero while processing audio. The control register is
// architecture specific: MXCSR on x86, FPCR on arm64 (Apple Silicon).
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
struct ScopedFtz {
    unsigned prev;
    ScopedFtz() : prev(_mm_getcsr()) { _mm_setcsr(prev | 0x8040); }  // FTZ | DAZ
    ~ScopedFtz() { _mm_setcsr(prev); }
};
#elif defined(__aarch64__)
struct ScopedFtz {
    unsigned long long prev;
    ScopedFtz() {
        asm volatile("mrs %0, fpcr" : "=r"(prev));
        unsigned long long cur = prev | (1ULL << 24);  // FZ: flush inputs and outputs
        asm volatile("msr fpcr, %0" : : "r"(cur));
    }
    ~ScopedFtz() { asm volatile("msr fpcr, %0" : : "r"(prev)); }
};
#else
struct ScopedFtz {};  // No denormal control available on this architecture.
#endif

// Convert the host's normalized [0,1] parameter values into DSP-domain settings.
#define BUFFER_SIZE_T(v) (1 << (int) (7.0f*(v) + 0.5f))

#define INC_LAG_T(v) (std::pow(std::log10(9.0f*(v) + 1.0f), 0.2f))
#define DEC_LAG_T(v) (std::pow(std::log10(9.0f*(v) + 1.0f), 0.01f))
#define A_GATE_LVL_T(v) ((std::pow(2.0f, 12.0f*(v)) - 1.0f)/4095.0f)
#define S_GATE_LVL_T(v) A_GATE_LVL_T(v)
#define TRIG_LVL_T(v) (3.0f*((v)-0.5f))
#define F_MIN_T(v) ((globalMaxFrequency - 1.0f)*(std::pow(2.0f, 9.0f*(v)) - 1.0f)/511.0f + 1.0f)
#define F_MAX_T(v) F_MIN_T(v)
#define F_LAG_T(v) (std::pow(std::log10(9.0f*(v) + 1.0f), 0.4f))
#define W_LAG_T(v) F_LAG_T(v)
#define BOOL_T(v) ((v) > 0.5f)
#define U_MOD_TYPE_T(v) ((ModulationTypeU) (unsigned int) (0.999f*((v)*kNModTypesU)))
#define F_MOD_TYPE_T(v) ((ModulationTypeF) (unsigned int) (0.999f*((v)*kNModTypesF)))
#define A_OFFSET_T(v) (2.0f*((v)-0.5f))
#define A_GAIN_T(v) (((v) > 0.5f) \
                     ? ((((v) > 0.9999f)) ? 5000.0f : 1.0f / (2.0f*(1.0f - (v)))) \
                     : 2.0f*(v))
#define F_OFFSET_T(v) (((v) > 0.5f) \
                       ? (std::pow(2.0f, 32.0f*((v)-0.5f)) - 1.0f)/65535.0f \
                       : -(std::pow(2.0f, 32.0f*(0.5f-(v))) - 1.0f)/65535.0f)
#define F_GAIN_T(v) (((v) > 0.5f) \
                     ? ((((v) > 0.9999f)) ? 5000.0f : 1.0f / (2.0f*(1.0f - (v)))) \
                     : 2.0f*(v))
#define OVER_T(v) (1 + (int) (15.0f*(v)))
#define WINDOW_T(v) ((int) (250.0f*(v)))
#define S_MOD_TYPE_T(v) ((ModulationTypeS) (unsigned int) (0.999f*((v)*kNModTypesS)))

#define M_LN2 0.69314718055994530942

// Macro for standard processing method.
#define PROC_METHOD(ioStatements) \
(float *in0, float *in1, float *out0, float *out1, int sampleFrames) { \
	while (--sampleFrames >= 0) { \
		{ioStatements} \
		syn1.tick(); syn2.tick(); \
	} \
}

// Processing handlers.
const WavePlug::method4fpi WavePlug::procHandlers[9] = {
	&WavePlug::proc1In1Out,
	&WavePlug::proc1In1OutB,
	&WavePlug::proc1In2Out,
	&WavePlug::proc1In2OutB,
	&WavePlug::proc2In1Out,
	&WavePlug::proc2In1OutB,
	&WavePlug::proc2In2Out,
	&WavePlug::proc2In2OutB,
	
	&WavePlug::procDoNothing // Fallback handler for error states.
};

const WavePlug::method4fpi WavePlug::procRHandlers[9] = {
	&WavePlug::procR1In1Out,
	&WavePlug::procR1In1OutB,
	&WavePlug::procR1In2Out,
	&WavePlug::procR1In2OutB,
	&WavePlug::procR2In1Out,
	&WavePlug::procR2In1OutB,
	&WavePlug::procR2In2Out,
	&WavePlug::procR2In2OutB,
	
	&WavePlug::procDoNothing // Fallback handler for error states.
};


// Displayers.
void WavePlug::bufferSizeDisplayer(float value, char *text) {
	std::sprintf(text, "%i", BUFFER_SIZE_T(value));
}

void WavePlug::bufferSizeKByteDisplayer(float value, char *text) {
	int multiplier = BUFFER_SIZE_T(value);
	float sRateModifier = globalSampleRate / WP_STD_SAMPLE_RATE;
	int kBytes = (int) (sizeof(float) * 2.0f * multiplier * sRateModifier *
	                    (2.0f * WP_ANA_BUFFER_SIZE + 1.5f * WP_SYN_BUFFER_SIZE) / 1024.0f);
	std::sprintf(text, "%i", kBytes);
}

void WavePlug::bufferSizeHzDisplayer(float value, char *text) {
	int multiplier = BUFFER_SIZE_T(value);
	int hz = (int) (1.0f / (multiplier * (WP_ANA_BUFFER_SIZE / WP_STD_SAMPLE_RATE)));
	std::sprintf(text, "%i", hz);
}

void WavePlug::bufferSizeMillisDisplayer(float value, char *text) {
	int multiplier = BUFFER_SIZE_T(value);
	int ms = (int) (1000.0f * multiplier * (WP_ANA_BUFFER_SIZE / WP_STD_SAMPLE_RATE));
	std::sprintf(text, "%i", ms);
}

void WavePlug::onOffDisplayer(float value, char *text) {
	std::strcpy(text, BOOL_T(value) ? "On" : "Off");
}

void WavePlug::pcntDisplayer(float value, char *text) {
	std::sprintf(text, "%1.2f", 100.0f*value);
}

void WavePlug::aIncLagDisplayer(float value, char *text) {
	std::sprintf(text, "%1.3f", 100.0f*INC_LAG_T(value));
}

void WavePlug::aDecLagDisplayer(float value, char *text) {
	std::sprintf(text, "%1.3f", 100.0f*DEC_LAG_T(value));
}

void WavePlug::gateLevelDisplayer(float value, char *text) {
	std::sprintf(text, "%1.2f", 100.0f*A_GATE_LVL_T(value));
}

void WavePlug::trigLevelDisplayer(float value, char *text) {
	std::sprintf(text, "%1.2f", 100.0f*TRIG_LVL_T(value));
}

void WavePlug::fMinMaxDisplayer(float value, char *text) {
	std::sprintf(text, "%1.2f", F_MIN_T(value));
}

void WavePlug::fwLagDisplayer(float value, char *text) {
	std::sprintf(text, "%1.3f", 100.0f*F_LAG_T(value));
}

void WavePlug::usignModTypeDisplayer(float value, char *text) {
	std::strcpy(text, modTypeUNames[U_MOD_TYPE_T(value)]);
}

void WavePlug::signModTypeDisplayer(float value, char *text) {
	std::strcpy(text, modTypeSNames[S_MOD_TYPE_T(value)]);
}

void WavePlug::funcModTypeDisplayer(float value, char *text) {
	std::strcpy(text, modTypeFNames[F_MOD_TYPE_T(value)]);
}

void WavePlug::aOffsetDisplayer(float value, char *text) {
	std::sprintf(text, "%1.2f", 100.0f*A_OFFSET