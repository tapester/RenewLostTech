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

#include "LostTechCore.hpp"

// Parameter labels, defaults, help text, displayers, and setter dispatch.
// Private static data.
const char *const LostTechCore::paramNames[kNumParams] = {
	"AIncLag", "ADecLag", "GatLvlA", "GatLvlS", "HiTrig", "LowTrig",
	"FMin", "FMax", "FLag", "WLag", "InvTrig", "Interp",
	"AModTyp", "AModMix", "FModTyp", "FModMix", "WModTyp", "WModMix",
	"AOffset", "AGain", "FOffset", "FGain", "Oversmp", "SmooWin",
	"OModTyp", "OModMix"
};

const char *const LostTechCore::paramLabels[kNumParams] = {
	"%", "%", "%", "%", "%", "%",
	"Hz", "Hz", "%", "%", "", "",
	"type", "%", "type", "%", "type", "%",
	"%", "dB", "Hz", "octaves", "", "samples",
	"type", "%"
};

const char *const LostTechCore::initParamHelpTexts[kNumParams] = {
	"How quickly output amplitude reacts to an increase in AIL.", // 0
	"How quickly output amplitude reacts to a decrease in AIL.",
	"Minimum output amplitude for F&W updates.",
	"Minimum input signal level for F&W updates.",
	"RIL must go above this value to trigger a F&W update.",
	"RIL must go below this value to trigger a F&W update.",
	"Lowest frequency detected by F&W update trigger.",
	"Highest frequency detected by F&W update trigger.",
	"How quickly the output frequency reacts to changes in detected frequency.",
	"How quickly the output waveform reacts to changes in detected waveform.",
	"Toggles between standard and inverted F&W update trigger sequence.",
	"Turns waveform output interpolation off/on.",
	
	"", // 12
	"",
	
	"", // 14
	"",
	
	"", // 16
	"",
	
	"Constant term added to input amplitude.", // 18
	"Amount of gain applied to input amplitude.",
	"Constant term added to input frequency.",
	"Amount of gain applied to input frequency.",
	"Input waveform oversampling rate.",
	"Size of output smoothing window.",
	
	"", // 24
	""
};

const char *const LostTechCore::modFuncUHelpTexts[UnsignedModulator::kNModTypesU][2] = {
	{
		"Mixes input 1 with input 2.",
		"Amount of input 2 to mix in."
	},
	{
		"Mixes the lowest input with the absolute difference of the inputs.",
		"Amount of absolute difference of inputs to mix in."
	},
	{
		"Mixes the highest input with the absolute difference of the inputs.",
		"Amount of absolute difference of inputs to mix in."
	},
	{
		"Adds a mix of the fine tuning of the inputs to the order of magnitude of input 1.",
		"Amount of the fine tuning of input 2 to mix in."
	},
	{
		"Mixes input 1 with the product of the inputs.",
		"Amount of input product to mix in."
	},
	{
		"Distorts input 1. The amount of distortion is controlled by input 2.",
		"How quickly the distortion of input 1 increases with input 2."
	},
	{
		"Mixes in input 2 in the range between input 1 and the max level.",
		"Fraction of the range between input 1 and the max level to fill with input 2."
	},
	{
		"Bounces between input 1 and input 2.",
		"Bounce rate."
	}
};

const char *const LostTechCore::modFuncSHelpTexts[SignedModulator::kNModTypesS][2] = {
	{
		"Mixes input 1 with input 2.",
		"Amount of input 2 to mix in."
	},
	{
		"Mixes input 1 with input 2 inverted.",
		"Amount of inverted input 2 to mix in."
	},
	{
		"Mixes input 1 with the product of the inputs.",
		"Amount of input product to mix in."
	},
	{
		"Distorts input 1. The amount of distortion is controlled by input 2.",
		"How quickly the distortion of input 1 increases with input 2."
	},
	{
		"Mixes in input 2 in the range between input 1 and the max level.",
		"Fraction of the range between input 1 and the max level to fill with input 2."
	},
	{
		"Bounces between input 1 and input 2.",
		"Bounce rate."
	}
};

const char *const LostTechCore::modFuncFHelpTexts[FunctionModulator::kNModTypesF][2] = {
	{
		"Mixes input 1 with input 2.",
		"Amount of input 2 to mix in."
	},
	{
		"Mixes input 1 with input 2 inverted.",
		"Amount of inverted input 2 to mix in."
	},
	{
		"Mixes input 1 with the product of the inputs.",
		"Amount of input product to mix in."
	},
	{
		"Distorts input 1. The amount of distortion is controlled by input 2.",
		"How quickly the distortion of input 1 increases with input 2."
	},
	{
		"Mixes in input 2 in the range between input 1 and the max level.",
		"Fraction of the range between input 1 and the max level to fill with input 2."
	},
	{
		"Mixes input 1 with the functional composition of input1 with input2.",
		"Amount of input functional composition to mix in."
	},
	{
		"Filters input 1. Uses input2 as a finite filter impulse response.",
		"Length of filter impulse response."
	},
	{
		"Bounces between input 1 and input 2.",
		"Bounce rate."
	}
};

const float LostTechCore::initParamValues[kNumParams] = {
	0.1f, 0.8f, 0.05f, 0.75f, 0.783f, 0.217f, 0.00055f, 0.73f, 0.0f, 0.0f, 0.0f, 1.0f,
	0.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 0.0f,
	0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
	0.0f, 0.0f
};

const LostTechCore::funcfcp LostTechCore::paramDisplayers[kNumParams] = {
	&LostTechCore::aIncLagDisplayer,
	&LostTechCore::aDecLagDisplayer,
	&LostTechCore::gateLevelDisplayer,
	&LostTechCore::gateLevelDisplayer,
	&LostTechCore::trigLevelDisplayer,
	&LostTechCore::trigLevelDisplayer,
	&LostTechCore::fMinMaxDisplayer,
	&LostTechCore::fMinMaxDisplayer,
	&LostTechCore::fwLagDisplayer,
	&LostTechCore::fwLagDisplayer,
	&LostTechCore::onOffDisplayer,
	&LostTechCore::onOffDisplayer,
	
	&LostTechCore::usignModTypeDisplayer,
	&LostTechCore::pcntDisplayer,
	
	&LostTechCore::usignModTypeDisplayer,
	&LostTechCore::pcntDisplayer,
	
	&LostTechCore::funcModTypeDisplayer,
	&LostTechCore::pcntDisplayer,
	
	&LostTechCore::aOffsetDisplayer,
	&LostTechCore::aGainDisplayer,
	&LostTechCore::fOffsetDisplayer,
	&LostTechCore::fGainDisplayer,
	&LostTechCore::oversamplingDisplayer,
	&LostTechCore::smoothingWinDisplayer,
	
	&LostTechCore::signModTypeDisplayer,
	&LostTechCore::pcntDisplayer
};

const LostTechCore::methodf LostTechCore::paramSetters[kNumParams] = {
	&LostTechCore::aAIWSetter,
	&LostTechCore::aADWSetter,
	&LostTechCore::aAGLSetter,
	&LostTechCore::aSGLSetter,
	&LostTechCore::aFHTSetter,
	&LostTechCore::aFLTSetter,
	&LostTechCore::aFMinSetter,
	&LostTechCore::aFMaxSetter,
	&LostTechCore::aFWSetter,
	&LostTechCore::aWWSetter,
	&LostTechCore::aITSetter,
	&LostTechCore::aWISetter,
	
	&LostTechCore::mAModSetter,
	&LostTechCore::mAMixSetter,
	
	&LostTechCore::mFModSetter,
	&LostTechCore::mFMixSetter,
	
	&LostTechCore::mWModSetter,
	&LostTechCore::mWMixSetter,
	
	&LostTechCore::sAOffSetter,
	&LostTechCore::sAGainSetter,
	&LostTechCore::sFOffSetter,
	&LostTechCore::sFGainSetter,
	&LostTechCore::sOverSetter,
	&LostTechCore::sWindSetter,
	
	&LostTechCore::mOModSetter,
	&LostTechCore::mOMixSetter
};

// Factory presets. Each one starts from the defaults and lists only what differs.
// Frequencies are for 44.1 kHz and up (the 22 kHz maximum), and scale down below that.
// Modulator types sit mid-step, so floor(0.999 * v * count) lands on the intended entry.
namespace {

enum {kEnd, kGlobal, kCh1, kCh2, kBoth}; // Where an edit applies; kEnd (0) closes the list.

struct PresetEdit {int where, param; float value;};

struct Preset {
	const char *name; // At most 24 characters, the VST2 program-name limit.
	PresetEdit edits[6];
};

const Preset presets[] = {
	{"Init Reference", {}},
	// Analyzer range limited to a mono lead (80 Hz to 1 kHz), 4x oversampling, light smoothing.
	{"Clean Mono Tracker", {{kBoth, kFMin, 0.167f}, {kBoth, kFMax, 0.5108f},
	                        {kBoth, kOversmpl, 0.2333f}, {kBoth, kSmooWin, 0.066f}}},
	// FLag 93%: the detected pitch is approached slowly.
	{"Portamento Glide", {{kBoth, kFLag, 0.6472f}}},
	// Gates at about 43%: pitch and timbre only update on loud hits, and are held in between.
	{"Gate Hold", {{kBoth, kGateLvlA, 0.9f}, {kBoth, kGateLvlS, 0.9f}}},
	// AOffset +15% keeps it sounding on silence; gates at about 3.6% then freeze the last pitch
	// and waveform (without them the pitch decays to a low rumble).
	{"Drone Sustain", {{kBoth, kAOffset, 0.575f}, {kBoth, kGateLvlA, 0.6f}, {kBoth, kGateLvlS, 0.6f}}},
	// FGain +1 octave (factor 2) and +0.585 octaves (factor 1.5), with 4x oversampling.
	{"Octave Up", {{kBoth, kFGain, 0.75f}, {kBoth, kOversmpl, 0.2333f}}},
	{"Fifth Up", {{kBoth, kFGain, 0.6667f}, {kBoth, kOversmpl, 0.2333f}}},
	// FGain at minimum, FOffset +165 Hz: a fixed pitch with the input's amplitude and waveform.
	{"Monotone Robot", {{kBoth, kFMin, 0.167f}, {kBoth, kFMax, 0.4766f},
	                    {kBoth, kFGain, 0.0f}, {kBoth, kFOffset, 0.7795f}}},
	// FOffset +3 Hz and -3 Hz: one mono input, two detuned voices.
	{"Stereo Detune Widener", {{kCh1, kFOffset, 0.6035f}, {kCh2, kFOffset, 0.3965f}}},
	// Input 1 gives the pitch; Input 2 gives amplitude and waveform. Output 2 repeats channel 1.
	{"Arp Painter", {{kCh1, kAModMix, 1.0f}, {kCh1, kWModMix, 1.0f}, {kCh2, kOModMix, 1.0f}}},
	// The same with FMod1 = Fine 100% (octave jumps).
	{"Octave-Jump Arpeggiator", {{kCh1, kAModMix, 1.0f}, {kCh1, kWModMix, 1.0f},
	                             {kCh2, kOModMix, 1.0f}, {kCh1, kFModType, 0.4375f},
	                             {kCh1, kFModMix, 1.0f}}},
	// WMod1 = Comp 100%: Input 2's waveform shapes Input 1.
	{"Waveshaper", {{kCh1, kWModType, 0.6875f}, {kCh1, kWModMix, 1.0f}, {kCh2, kOModMix, 1.0f}}},
	// AMod1 = Add 100%: Input 2's loudness plays Input 1's pitch and timbre.
	{"Rhythm Gate", {{kCh1, kAModMix, 1.0f}, {kCh2, kOModMix, 1.0f}}},
	// WMod1 = Dist 70%: the level of Input 2 sets how hard Input 1 is distorted.
	{"Signal Distortion", {{kCh1, kWModType, 0.4375f}, {kCh1, kWModMix, 0.7f}, {kCh2, kOModMix, 1.0f}}},
	// OMod = Mult: the product of the two resynthesized voices.
	{"Ring Mult", {{kCh1, kOModType, 0.4167f}, {kCh1, kOModMix, 1.0f},
	               {kCh2, kOModType, 0.4167f}, {kCh2, kOModMix, 0.6f}}},
	// WMod1 = Conv 40% (about 13 taps): Input 2's waveform is the filter's impulse response.
	{"Convolution Filter", {{kCh1, kWModType, 0.8125f}, {kCh1, kWModMix, 0.4f}, {kCh2, kOModMix, 1.0f}}},
	// WMod1 = Pong 30%, OMod = Pong 15%: slow morphing between the two sources.
	{"Pong Morph", {{kCh1, kWModType, 0.9375f}, {kCh1, kWModMix, 0.3f},
	                {kBoth, kOModType, 0.9167f}, {kBoth, kOModMix, 0.15f}}},
	// No interpolation, smallest useful buffer (2x).
	{"Lo-Fi Crunch", {{kBoth, kInterp, 0.0f}, {kGlobal, kBufferSize, 1.0f / 7.0f}}},
	// 8x oversampling, 40-sample smoothing, a little waveform and frequency lag.
	{"Soft Pad", {{kBoth, kOversmpl, 0.5f}, {kBoth, kSmooWin, 0.162f},
	              {kBoth, kWLag, 0.0558f}, {kBoth, kFLag, 0.0292f}}},
	// Inverted trigger on channel 1 only: the two channels cut cycles differently.
	{"Inverted Trigger Texture", {{kCh1, kInvTrig, 1.0f}}}
};

} // namespace

int LostTechCore::getNumPresets() {return (int) (sizeof presets / sizeof *presets);}

const char *LostTechCore::getPresetName(int preset) {
	return (preset >= 0 && preset < getNumPresets()) ? presets[preset].name : "";
}

float LostTechCore::getPresetValue(int preset, int index) {
	float value = getParameterDefault(index);
	if (preset < 0 || preset >= getNumPresets())
		return value;
	const bool stereo = index >= kNumMonoParams;
	const int channel = (index - kNumMonoParams) / kNumParams; // Only meaningful when stereo.
	for (const PresetEdit &e : presets[preset].edits) {
		if (e.where == kEnd)
			break;
		const bool hit = (e.where == kGlobal) ? index == e.param :
			stereo && (index - kNumMonoParams) % kNumParams == e.param &&
			(e.where == kBoth || e.where == kCh1 + channel);
		if (hit)
			value = e.value;
	}
	return value;
}

