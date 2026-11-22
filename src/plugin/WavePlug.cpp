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
#define F_MIN_T(v, maxFreq) (((maxFreq) - 1.0f)*(std::pow(2.0f, 9.0f*(v)) - 1.0f)/511.0f + 1.0f)
#define F_MAX_T(v, maxFreq) F_MIN_T(v, maxFreq)
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

// Processing handlers.
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
	std::sprintf(text, "%1.2f", F_MIN_T(value, globalMaxFrequency));
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
	std::sprintf(text, "%1.2f", 100.0f*A_OFFSET_T(value));
}

void WavePlug::aGainDisplayer(float value, char *text) {
	float value2 = A_GAIN_T(value);
	if (value2 == 0.0f)
		std::strcpy(text, "Quiet");
	else
		std::sprintf(text, "%1.2f", 20.0f*std::log10(value2));
}

void WavePlug::fOffsetDisplayer(float value, char *text) {
	float value2 = F_OFFSET_T(value);
	std::sprintf(text, "%1.1f", globalMaxFrequency*value2);
}

void WavePlug::fGainDisplayer(float value, char *text) {
	float value2 = F_GAIN_T(value);
	if (value2 == 0.0f)
		std::strcpy(text, "Min");
	else
		std::sprintf(text, "%1.3f", std::log(value2)/M_LN2);
}

void WavePlug::oversamplingDisplayer(float value, char *text) {
	std::sprintf(text, "%i", OVER_T(value));
}

void WavePlug::smoothingWinDisplayer(float value, char *text) {
	std::sprintf(text, "%i", WINDOW_T(value));
}


// Constructor.
WavePlug::WavePlug(audioMasterCallback audioMaster) :
	AudioEffectX(audioMaster, 1, kNumAllParams)
{
	InitializeCriticalSectionAndSpinCount(&myCriticalSection, 0x80000400);
	
	EnterCriticalSection(&myCriticalSection);
	
	setNumInputs(2);
	setNumOutputs(2);
	setUniqueID(CCONST('C','Q','C','Q'));
	canProcessReplacing();
	std::strcpy(programName, "Default");
	
	paramHelpTexts[kPlugVersion] = "Plugin version number.";
	paramHelpTexts[kBufferSize] = "Sample buffer size multiplier.";
	std::memcpy(
		paramHelpTexts + kNumMonoParams,
		initParamHelpTexts, kNumParams * sizeof (const char *));
	std::memcpy(
		paramHelpTexts + kNumMonoParams + kNumParams,
		initParamHelpTexts, kNumParams * sizeof (const char *));
	
	sharedData.operational = true;
	sharedData.bypassedFlag = false;
	sharedData.input0Connected = sharedData.output0Connected = 1;
	sharedData.input1Connected = sharedData.output1Connected = 0;
	sharedData.bufferSizeMultiplier = 1;
	
	sharedData.paramValues[kPlugVersion] = 0.0f;
	sharedData.paramValues[kBufferSize] = 2.0f / 7.0f;
	std::memcpy(
		sharedData.paramValues + kNumMonoParams,
		initParamValues, kNumParams * sizeof (float));
	std::memcpy(
		sharedData.paramValues + kNumMonoParams + kNumParams,
		initParamValues, kNumParams * sizeof (float));
	
	sharedData.preA1 = sharedData.postA1 = sharedData.preF1 = sharedData.postF1 =
	sharedData.preA2 = sharedData.postA2 = sharedData.preF2 = sharedData.postF2 = 0.0f;
	
	sharedData.reinitFlag = true;
	sharedData.resetFlag = false;
	sharedData.setProcessHandlersFlag = false;
	sharedData.newSampleRate = NAN;
	std::memcpy(
		sharedData.newParamValues, sharedData.paramValues, kNumAllParams * sizeof (float));
	
	editor = NULL;
	
#ifndef WP_NO_GUI
	try {
		editor = new WavePlugEditor(this);
	}
	catch (std::bad_alloc e) {
		sharedData.operational = false;
	}
#endif
	
	LeaveCriticalSection(&myCriticalSection);
}

WavePlug::~WavePlug() {
#ifndef WP_NO_GUI
	EnterCriticalSection(&myCriticalSection);
	delete editor;
	editor = NULL;
	LeaveCriticalSection(&myCriticalSection);
#endif
	
	DeleteCriticalSection(&myCriticalSection);
}


// Inherited virtual public methods.
bool WavePlug::getEffectName(char *text) {
	std::strcpy(text, "Lost Technology");
	return true;
}

bool WavePlug::getProductString(char *text) {
	std::strcpy(text, "Lost Recombinant Audio Technology");
	return true;
}

bool WavePlug::getVendorString(char *text) {
	std::strcpy(text, "Transvaal Audio");
	return true;
}

VstInt32 WavePlug::getVendorVersion() {return WP_VENDOR_VERSION;}

VstPlugCategory WavePlug::getPlugCategory() {return kPlugCategEffect;}

VstInt32 WavePlug::canDo(char *text) {
	if (!std::strcmp(text, "sendVstEvents") ||
	    !std::strcmp(text, "sendVstMidiEvent") ||
	    !std::strcmp(text, "sendVstTimeInfo"))
		return -1l;
	if (!std::strcmp(text, "receiveVstEvents") ||
	    !std::strcmp(text, "receiveVstMidiEvent") ||
	    !std::strcmp(text, "receiveVstTimeInfo"))
		return -1l;
	else if (!std::strcmp(text, "offline") || !std::strcmp(text, "noRealTime"))
		return -1l;
	else if (!std::strcmp(text, "1in1out") || !std::strcmp(text, "1in2out") ||
		       !std::strcmp(text, "2in1out") || !std::strcmp(text, "2in2out"))
		return 1l;
	else if (!std::strcmp(text, "2in4out") || !std::strcmp(text, "4in2out") ||
		       !std::strcmp(text, "4in4out") || !std::strcmp(text, "4in8out") ||
	         !std::strcmp(text, "8in4out") || !std::strcmp(text, "8in8out"))
		return -1l;
	else if (!std::strcmp(text, "bypass"))
		return 1l;
	return 0l;
}

void WavePlug::setProgram(VstInt32 program) {}

void WavePlug::setProgramName(char *name) {
	EnterCriticalSection(&myCriticalSection);
	std::snprintf(programName, sizeof(programName), "%s", name);
	LeaveCriticalSection(&myCriticalSection);
}

void WavePlug::getProgramName(char *name) {
	EnterCriticalSection(&myCriticalSection);
	std::snprintf(name, 32, "%s", programName);
	LeaveCriticalSection(&myCriticalSection);
}

bool WavePlug::getProgramNameIndexed(VstInt32 category, VstInt32 index, char *text) {
	bool success = false;
	EnterCriticalSection(&myCriticalSection);
	if ((category == 0 || category == -1) && index == 0) {
		std::snprintf(text, 32, "%s", programName);
		success = true;
	}
	LeaveCriticalSection(&myCriticalSection);
	return success;
}

void WavePlug::setParameter(VstInt32 index, float value) {
	if (index < 0 || index >= kNumAllParams)
		return;
	value = clamp01(value);

	const char *const*texts = NULL;
	
	switch (index) {
		case (kNumMonoParams + kAModType):
		case (kNumMonoParams + kNumParams + kAModType):
		case (kNumMonoParams + kFModType):
		case (kNumMonoParams + kNumParams + kFModType):
		texts = modFuncUHelpTexts[U_MOD_TYPE_T(value)];
		break;
		
		case (kNumMonoParams + kWModType):
		case (kNumMonoParams + kNumParams + kWModType):
		texts = modFuncFHelpTexts[F_MOD_TYPE_T(value)];
		break;
		
		case (kNumMonoParams + kOModType):
		case (kNumMonoParams + kNumParams + kOModType):
		texts = modFuncSHelpTexts[S_MOD_TYPE_T(value)];
		break;
	}
	
	EnterCriticalSection(&myCriticalSection);
	
	sharedData.newParamValues[index] = value;
	sharedData.paramValues[index] = value;
	
	if (texts != NULL) {
		paramHelpTexts[index] = texts[0];
		paramHelpTexts[index+1] = texts[1];
	}
	
#ifndef WP_NO_GUI
	if (editor != NULL)
		((AEffGUIEditor *) editor)->setParameter(index, value);
#endif
	
	LeaveCriticalSection(&myCriticalSection);
}

float WavePlug::getParameter(VstInt32 index) {
	float value = NAN;
	EnterCriticalSection(&myCriticalSection);
	value = sharedData.paramValues[index];
	LeaveCriticalSection(&myCriticalSection);
	return value;
}

void WavePlug::getParameterName(VstInt32 index, char *label) {
	if (index < kNumMonoParams) {
		if (index == kPlugVersion)
			std::strcpy(label, "Version");
		else if (index == kBufferSize)
			std::strcpy(label, "BufrSize");
	}
	else {
		index -= kNumMonoParams;
		std::strcpy(label, paramNames[index % kNumParams]);
		std::strcat(label, (index < kNumParams) ? "1" : "2");
	}
}

void WavePlug::getParameterDisplay(VstInt32 index, char *text) {
	float value = getParameter(index);
	if (index < kNumMonoParams) {
		if (index == kPlugVersion)
			std::strcpy(text, WP_VERSION_STRING(WP_MAJOR, WP_MINOR, WP_UPDATE));
		else if (index == kBufferSize)
			bufferSizeDisplayer(value, text);
	}
	else {
		index -= kNumMonoParams;
		paramDisplayers[index % kNumParams](value, text);
	}
}

void WavePlug::getParameterLabel(VstInt32 index, char *label) {
	if (index < kNumMonoParams)
		std::strcpy(label, "");
	else {
		index -= kNumMonoParams;
		std::strcpy(label, paramLabels[index % kNumParams]);
	}
}

void WavePlug::setSampleRate(float sRate) {
	EnterCriticalSection(&myCriticalSection);
	sharedData.newSampleRate = sRate;
	LeaveCriticalSection(&myCriticalSection);
}

bool WavePlug::setBypass(bool onOff) {
	EnterCriticalSection(&myCriticalSection);
	sharedData.bypassedFlag = onOff;
	sharedData.setProcessHandlersFlag = true;
	LeaveCriticalSection(&myCriticalSection);
	return true;
}

void WavePlug::suspend() {
	EnterCriticalSection(&myCriticalSection);
	sharedData.resetFlag = true;
	LeaveCriticalSection(&myCriticalSection);
}

void WavePlug::resume() {
	bool noInputs = false, noOutputs = false;
	EnterCriticalSection(&myCriticalSection);
	sharedData.resetFlag = true;
	sharedData.setProcessHandlersFlag = true;
	sharedData.input0Connected = true;
	sharedData.input1Connected = true;
	sharedData.output0Connected = true;
	sharedData.output1Connected = true;
	
	if (!(sharedData.input0Connected | sharedData.input1Connected)) {
		sharedData.operational = false;
		noInputs = true;
	}
	
	if (!(sharedData.output0Connected | sharedData.output1Connected)) {
		sharedData.operational = false;
		noOutputs = true;
	}
	
	LeaveCriticalSection(&myCriticalSection);
	
	if (noInputs & noOutputs)
		throw std::runtime_error("WavePlug::resume - No inputs or outputs connected.");
	else if (noInputs)
		throw std::runtime_error("WavePlug::resume - No inputs connected.");
	else if (noOutputs)
		throw std::runtime_error("WavePlug::resume - No outputs connected.");
}

void WavePlug::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) {
	#if defined(_DEBUG) && defined(_MSC_VER)
	#endif
	ScopedFtz ftz;

	doThreadSynchronizedDataExchange();
	
	(this->*procRHandler)(
		inputs[in0Index], inputs[in1Index], outputs[out0Index], outputs[out1Index], sampleFrames);
}


// Non-inherited non-virtual public methods.
const char *WavePlug::getParamHelpText(int index) {
	const char *helpText = "";
	EnterCriticalSection(&myCriticalSection);
	helpText = paramHelpTexts[index];
	LeaveCriticalSection(&myCriticalSection);
	return helpText;
}

float WavePlug::getAmplitude(int channel, bool postmod) {
	float value = NAN;
	EnterCriticalSection(&myCriticalSection);
	value = (channel == 0)
		? ((postmod) ? sharedData.postA1 : sharedData.preA1)
		: ((channel == 1)
			? ((postmod) ? sharedData.postA2 : sharedData.preA2)
	    : 0.0f);
	LeaveCriticalSection(&myCriticalSection);
	return value;
}

float WavePlug::getFrequency(int channel, bool postmod) {
	float value = NAN;
	EnterCriticalSection(&myCriticalSection);
	value = (channel == 0)
		? ((postmod) ? sharedData.postF1 : sharedData.preF1)
		: ((channel == 1)
			? ((postmod) ? sharedData.postF2 : sharedData.preF2)
	    : 0.0f);
	LeaveCriticalSection(&myCriticalSection);
	return value;
}

bool WavePlug::isOperational() {
	bool flag = false;
	EnterCriticalSection(&myCriticalSection);
	flag = sharedData.operational;
	LeaveCriticalSection(&myCriticalSection);
	return flag;
}

int WavePlug::getBufferSizeMultiplier() {
	int multiplier = -1;
	EnterCriticalSection(&myCriticalSection);
	multiplier = sharedData.bufferSizeMultiplier;
	LeaveCriticalSection(&myCriticalSection);
	return multiplier;
}


// ---<<< PRIVATE METHODS BEGIN HERE >>>---
void WavePlug::setOperational(bool flag) {
	EnterCriticalSection(&myCriticalSection);
	sharedData.operational = flag;
	LeaveCriticalSection(&myCriticalSection);
}

void WavePlug::WavePlugData::clearUpdateFields() {
	reinitFlag = false;
	resetFlag = false;
	setProcessHandlersFlag = false;
	newSampleRate = NAN;
	std::fill(newParamValues, newParamValues + kNumAllParams, NAN);
}

void WavePlug::doThreadSynchronizedDataExchange() {
	EnterCriticalSection(&myCriticalSection);
	
	if (sharedData.operational & !sharedData.reinitFlag) {
		sharedData.preA1 = ana1.getAmplitude();
		sharedData.postA1 = syn1.getAmplitude();
		sharedData.preF1 = ana1.getFrequency();
		sharedData.postF1 = syn1.getFrequency();
		sharedData.preA2 = ana2.getAmplitude();
		sharedData.postA2 = syn2.getAmplitude();
		sharedData.preF2 = ana2.getFrequency();
		sharedData.postF2 = syn2.getFrequency();
	}
	
	processingData = sharedData;
	sharedData.clearUpdateFields();
	
	LeaveCriticalSection(&myCriticalSection);
	
	if (processingData.reinitFlag) {
		if (!reinitialize())
			setOperational(false);
	}
	
	if (processingData.setProcessHandlersFlag)
		setProcHandlers();
	
	if (!processingData.operational)
		return;
	
	if (processingData.resetFlag) {
		ana1.reset();
		ana2.reset();
		syn1.reset();
		syn2.reset();
	}
	
	if (!std::isnan(processingData.newSampleRate)) {
		for (int index = 0; index < kNumAllParams; index++) {
			if (std::isnan(processingData.newParamValues[index]))
				processingData.newParamValues[index] = processingData.paramValues[index];
		}
		
		sampleRate.set(processingData.newSampleRate);
		setGlobalSampleRate(processingData.newSampleRate);
		
		setBufferSizeMultiplier(processingData.bufferSizeMultiplier);
		
		EnterCriticalSection(&myCriticalSection);
		
		AudioEffectX::setSampleRate(processingData.newSampleRate);
		
		LeaveCriticalSection(&myCriticalSection);
	}
	
	if (doParameterUpdates() > 0) {
		EnterCriticalSection(&myCriticalSection);
		sharedData.bufferSizeMultiplier = processingData.bufferSizeMultiplier;
		LeaveCriticalSection(&myCriticalSection);
	}
}

bool WavePlug::reinitialize() {
	// Processing components.
	ana1.initialize(&sampleRate);
	ana2.initialize(&sampleRate);
	modA1.initialize(ana1.getAmpFunction(), ana2.getAmpFunction());
	modA2.initialize(ana2.getAmpFunction(), ana1.getAmpFunction());
	modF1.initialize(ana1.getFreqFunction(), ana2.getFreqFunction(), sampleRate.hzToUnsigned(261.63f));
	modF2.initialize(ana2.getFreqFunction(), ana1.getFreqFunction(), sampleRate.hzToUnsigned(261.63f));
	modW1.initialize(ana1.getWaveFunction(), ana2.getWaveFunction());
	modW2.initialize(ana2.getWaveFunction(), ana1.getWaveFunction());
	syn1.initialize(&sampleRate, &modA1, &modF1, &modW1);
	syn2.initialize(&sampleRate, &modA2, &modF2, &modW2);
	modO1.initialize(syn1.getAudioFunction(), syn2.getAudioFunction());
	modO2.initialize(syn2.getAudioFunction(), syn1.getAudioFunction());
	modW1.setSampleRate(&sampleRate);
	modW2.setSampleRate(&sampleRate);
	modO1.setSampleRate(&sampleRate);
	modO2.setSampleRate(&sampleRate);
	
	// Allocate the sample buffers.
	setBufferSizeMultiplier(processingData.bufferSizeMultiplier);
	
	// Set initial parameter values in components.
	editMode = -1; // NOT 0 or 1.
	doParameterUpdates(); // MUST be called AFTER editMode initialization.
	
	setProcHandlers();
	
	return processingData.operational;
}


// Setters.
int WavePlug::doParameterUpdates() {
	int nUpdated = 0;
	
	for (int index = 0; index < kNumAllParams; index++) {
		if (std::isnan(processingData.newParamValues[index]))
			continue;
		else if (index < kNumMonoParams) {
			if (index == kBufferSize) {
				setBufferSizeMultiplier(BUFFER_SIZE_T(processingData.newParamValues[index]));
				processingData.paramValues[index] = processingData.newParamValues[index];
				
				nUpdated++;
			}
		}
		else {
			long stereoIndex = index - kNumMonoParams;
			
			setEditMode((stereoIndex < kNumParams) ? 0 : 1);
			
			(this->*paramSetters[stereoIndex % kNumParams])(processingData.newParamValues[index]);
			processingData.paramValues[index] = processingData.newParamValues[index];
			
			nUpdated++;
		}
	}
	
	return nUpdated;
}

void WavePlug::setBufferSizeMultiplier(int multiplier) {
	int anaSize = (int) (multiplier * (WP_ANA_BUFFER_SIZE * sampleRate.rate) / WP_STD_SAMPLE_RATE),
			synSize = (int) (multiplier * (WP_SYN_BUFFER_SIZE * sampleRate.rate) / WP_STD_SAMPLE_RATE);
	
	ana1.setBufferSize(anaSize);
	ana2.setBufferSize(anaSize);
	syn1.setBufferSize(synSize);
	syn2.setBufferSize(synSize);
	
	processingData.bufferSizeMultiplier = multiplier;
}

void WavePlug::setEditMode(int mode) {
	if (mode == 0 && editMode != 0) {
		editMode = 0;
		anaE  = &ana1;
		modAE = &modA1;
		modFE = &modF1;
		modWE = &modW1;
		synE  = &syn1;
		modOE = &modO1;
	}
	else if (mode == 1 && editMode != 1) {
		editMode = 1;
		anaE  = &ana2;
		modAE = &modA2;
		modFE = &modF2;
		modWE = &modW2;
		synE  = &syn2;
		modOE = &modO2;
	}
}

void WavePlug::aAIWSetter(float value) {anaE->setAIncWeight(INC_LAG_T(value));}
void WavePlug::aADWSetter(float value) {anaE->setADecWeight(DEC_LAG_T(value));}
void WavePlug::aAGLSetter(float value) {anaE->setAmpGateLevel(A_GATE_LVL_T(value));}
void WavePlug::aSGLSetter(float value) {anaE->setSampleGateLevel(S_GATE_LVL_T(value));}
void WavePlug::aFHTSetter(float value) {anaE->setHighTrig(TRIG_LVL_T(value));}
void WavePlug::aFLTSetter(float value) {anaE->setLowTrig(TRIG_LVL_T(value));}
void WavePlug::aFMinSetter(float value) {anaE->setFMin(F_MIN_T(value, sampleRate.maxFrequency));}
void WavePlug::aFMaxSetter(float value) {anaE->setFMax(F_MAX_T(value, sampleRate.maxFrequency));}
void WavePlug::aFWSetter(float value) {anaE->setFWeight(F_LAG_T(value));}
void WavePlug::aWWSetter(float value) {anaE->setWWeight(W_LAG_T(value));}
void WavePlug::aITSetter(float value) {anaE->setTrigInverted(BOOL_T(value));}
void WavePlug::aWISetter(float value) {anaE->setWInterpolation(BOOL_T(value));}
void WavePlug::mAModSetter(float value) {modAE->setModulation(U_MOD_TYPE_T(value));}
void WavePlug::mAMixSetter(float value) {modAE->setMix(value);}
void WavePlug::mFModSetter(float value) {modFE->setModulation(U_MOD_TYPE_T(value));}
void WavePlug::mFMixSetter(float value) {modFE->setMix(value);}
void WavePlug::mWModSetter(float value) {modWE->setModulation(F_MOD_TYPE_T(value));}
void WavePlug::mWMixSetter(float value) {modWE->setMix(value);}
void WavePlug::sAOffSetter(float value) {synE->setAOffset(A_OFFSET_T(value));}
void WavePlug::sAGainSetter(float value) {synE->setAGain(A_GAIN_T(value));}
void WavePlug::sFOffSetter(float value) {synE->setFOffset(F_OFFSET_T(value));}
void WavePlug::sFGainSetter(float value) {synE->setFGain(F_GAIN_T(value));}
void WavePlug::sOverSetter(float value) {synE->setOversamplingMultiplier(OVER_T(value));}
void WavePlug::sWindSetter(float value) {synE->setSmoothingWindow(WINDOW_T(value));}
void WavePlug::mOModSetter(float value) {modOE->setModulation(S_MOD_TYPE_T(value));}
void WavePlug::mOMixSetter(float value) {modOE->setMix(value);}


// Processing handlers.
void WavePlug::setProcHandlers() {
	if (!processingData.operational) {
		procRHandler = procRHandlers[8];
		return;
	}
	
	int handlerIndex = 0;
	
	switch (processingData.input1Connected << 1 | processingData.input0Connected) {
		case 1:
		in1Index = 0; in0Index = 0;
		break;
		
		case 2:
		in1Index = 1; in0Index = 1;
		break;
		
		case 3:
		in1Index = 1; in0Index = 0;
		handlerIndex += 4;
		break;
	}
	
	switch (processingData.output1Connected << 1 | processingData.output0Connected) {
		case 1:
		out1Index = 0; out0Index = 0;
		break;
		
		case 2:
		out1Index = 1; out0Index = 1;
		break;
		
		case 3:
		out1Index = 1; out0Index = 0;
		handlerIndex += 2;
		break;
	}
	
	if (processingData.bypassedFlag)
		handlerIndex += 1;
	
	procRHandler = procRHandlers[handlerIndex];
}

void WavePlug::procR1In1Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in0++);
		*out0++ = modO1.getValue();
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR1In1OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in0);
		*out0++ = *in0++;
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR1In2Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in0++);
		*out0++ = modO1.getValue();
		*out1++ = modO2.getValue();
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR1In2OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in0);
		*out0++ = *in0;
		*out1++ = *in0++;
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR2In1Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0++);
		ana2.addSample(*in1++);
		*out0++ = modO1.getValue();
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR2In1OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in1++);
		*out0++ = *in0++;
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR2In2Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0++);
		ana2.addSample(*in1++);
		*out0++ = modO1.getValue();
		*out1++ = modO2.getValue();
		syn1.tick(); syn2.tick();
	}
}

void WavePlug::procR2In2OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {
	while (--sampleFrames >= 0) {
		ana1.addSample(*in0);
		ana2.addSample(*in1);
		*out0++ = *in0++;
		*out1++ = *in1++;
		syn1.tick(); syn2.tick();
	}
}
