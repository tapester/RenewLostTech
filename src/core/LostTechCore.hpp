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

#ifndef WP_LOSTTECHCORE_HPP
#define WP_LOSTTECHCORE_HPP

#include "wpstdinclude.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include "Analyzer.hpp"
#include "wpmodulators.hpp"
#include "Synthesizer.hpp"
#include "waveplugparams.h"

#ifndef WP_MAJOR
#define WP_MAJOR 0
#endif
#ifndef WP_MINOR
#define WP_MINOR 2
#endif
#ifndef WP_UPDATE
#define WP_UPDATE 6
#endif
#define WP_VENDOR_VERSION ((long) (WP_MAJOR*10000 + WP_MINOR*100 + WP_UPDATE))
#define WP_STRINGIFY(arg) #arg
#define WP_VERSION_STRING(ma, mi, u) WP_STRINGIFY(ma) "." WP_STRINGIFY(mi) "." WP_STRINGIFY(u)

#define WP_ANA_BUFFER_SIZE 1250
#define WP_SYN_BUFFER_SIZE 1250

// The whole effect, with no plug-in SDK dependency. A shell (VST2 today) owns one and forwards to it.
class LostTechCore {
public: // public typedefs
	typedef void (*funcfcp)(float, char *);
	
private: // private typedefs
	typedef void (LostTechCore::*methodf)(float);
	typedef void (LostTechCore::*method4fpi)(float *, float *, float *, float *, int);
	
private: // private static data members
	static const char *const paramNames[kNumParams];
	static const char *const paramLabels[kNumParams];
	static const char *const initParamHelpTexts[kNumParams];
	static const char *const modFuncUHelpTexts[UnsignedModulator::kNModTypesU][2];
	static const char *const modFuncSHelpTexts[SignedModulator::kNModTypesS][2];
	static const char *const modFuncFHelpTexts[FunctionModulator::kNModTypesF][2];
	
	static const float initParamValues[kNumParams];
	
	// Displayers and setters.
	static const funcfcp paramDisplayers[kNumParams];
	static const methodf paramSetters[kNumParams];
	
	// Processing handlers.
	static const method4fpi procRHandlers[9];
	
public: // public static methods
	// Displayers.
	static funcfcp getParamDisplayer(int index) {return paramDisplayers[index % kNumParams];}
	
	static void bufferSizeDisplayer(float value, char *text);
	static void bufferSizeKByteDisplayer(float value, char *text);
	static void bufferSizeHzDisplayer(float value, char *text);
	static void bufferSizeMillisDisplayer(float value, char *text);
	
	static void onOffDisplayer(float value, char *text);
	static void pcntDisplayer(float value, char *text);
	
	static void aIncLagDisplayer(float value, char *text);
	static void aDecLagDisplayer(float value, char *text);
	static void gateLevelDisplayer(float value, char *text);
	static void trigLevelDisplayer(float value, char *text);
	static void fMinMaxDisplayer(float value, char *text);
	static void fwLagDisplayer(float value, char *text);
	
	static void usignModTypeDisplayer(float value, char *text);
	static void signModTypeDisplayer(float value, char *text);
	static void funcModTypeDisplayer(float value, char *text);
	
	static void aOffsetDisplayer(float value, char *text);
	static void aGainDisplayer(float value, char *text);
	static void fOffsetDisplayer(float value, char *text);
	static void fGainDisplayer(float value, char *text);
	static void oversamplingDisplayer(float value, char *text);
	static void smoothingWinDisplayer(float value, char *text);
	
private: // private data members
	// ---<<< Shared data                     >>>---
	// Guards the shared data below between host, editor and audio threads.
	SampleRate sampleRate; // This instance's rate; globalSampleRate is for display only.
	CRITICAL_SECTION myCriticalSection;
	
	// ---<<< ALL ACCESS MUST BE SYNCHRONIZED >>>---
	
	// Plugin info. (Not touched by the processing thread.)
	const char *paramHelpTexts[kNumAllParams];
	
	struct WavePlugData {
		// ---<<< State fields (what is the case now) >>>---
		// Plugin configuration.
		bool operational, bypassedFlag;
		int input0Connected, input1Connected, output0Connected, output1Connected;
		int bufferSizeMultiplier;
		
		// Parameter and signal monitor data.
		float paramValues[kNumAllParams];
		float preA1, postA1, preF1, postF1, preA2, postA2, preF2, postF2;
		
		// ---<<< Update fields (what the processing thread should change) >>>---
		// Host-thread requests are consumed and cleared at the next audio callback.
		bool reinitFlag, resetFlag, setProcessHandlersFlag;
		float newSampleRate, newParamValues[kNumAllParams];
		
		void clearUpdateFields();
		
	} sharedData, processingData;
	
	// ---<<< Private data of processing object           >>>---
	// ---<<< ALL ACCESS MUST HAPPEN ON PROCESSING THREAD >>>---
	
	// Processing components.
	Analyzer ana1, ana2, *anaE;
	UnsignedModulator modA1, modA2, *modAE, modF1, modF2, *modFE;
	FunctionModulator modW1, modW2, *modWE;
	Synthesizer syn1, syn2, *synE;
	SignedModulator modO1, modO2, *modOE;
	
	// Setter and processing handler state.
	int editMode;
	method4fpi procRHandler;
	int in0Index, in1Index, out0Index, out1Index;
	
public: // public methods
	LostTechCore();
	~LostTechCore();
	
	// Parameters. Indexes and values follow waveplugparams.h; values are 0..1.
	void setParameter(int index, float value);
	float getParameter(int index);
	void getParameterDisplay(int index, char *text);
	void getParameterDisplay(int index, float value, char *text); // For a value other than the current one.
	static float getParameterDefault(int index);
	void getParameterName(int index, char *text);
	void getParameterLabel(int index, char *label);
	
	// Processing.
	void setSampleRate(float sampleRate);
	bool setBypass(bool onOff);
	void suspend();
	void resume();
	void process(float **inputs, float **outputs, int sampleFrames);
	
	// Custom.
	const char *getVersionString() {return WP_VERSION_STRING(WP_MAJOR, WP_MINOR, WP_UPDATE);}
	const char *getParamHelpText(int index);
	
	bool isOperational();
	void setOperational(bool flag);
	
	float getAmplitude(int channel, bool postmod = false);
	float getFrequency(int channel, bool postmod = false);
	
private: // private methods
	void doThreadSynchronizedDataExchange();
	
	bool reinitialize();
	
	// Setters.
	int doParameterUpdates();
	
	void setBufferSizeMultiplier(int multiplier);
	
	void setEditMode(int mode);
	
	void aAIWSetter(float value);
	void aADWSetter(float value);
	void aAGLSetter(float value);
	void aSGLSetter(float value);
	void aFHTSetter(float value);
	void aFLTSetter(float value);
	void aFMinSetter(float value);
	void aFMaxSetter(float value);
	void aFWSetter(float value);
	void aWWSetter(float value);
	void aITSetter(float value);
	void aWISetter(float value);
	
	void mAModSetter(float value);
	void mAMixSetter(float value);
	
	void mFModSetter(float value);
	void mFMixSetter(float value);
	
	void mWModSetter(float value);
	void mWMixSetter(float value);
	
	void sAOffSetter(float value);
	void sAGainSetter(float value);
	void sFOffSetter(float value);
	void sFGainSetter(float value);
	void sOverSetter(float value);
	void sWindSetter(float value);
	
	void mOModSetter(float value);
	void mOMixSetter(float value);
	
	// Processing handlers.
	void setProcHandlers();
	
	void procDoNothing(float *in0, float *in1, float *out0, float *out1, int sampleFrames) {}
	
	void procR1In1Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR1In1OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR1In2Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR1In2OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR2In1Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR2In1OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR2In2Out(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
	void procR2In2OutB(float *in0, float *in1, float *out0, float *out1, int sampleFrames);
};

#endif
