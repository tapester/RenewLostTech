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

// Parameter labels, defaults, help text, displayers, and setter dispatch.
// Private static data.
const char *const WavePlug::paramNames[kNumParams] = {
	"AIncLag", "ADecLag", "GatLvlA", "GatLvlS", "HiTrig", "LowTrig",
	"FMin", "FMax", "FLag", "WLag", "InvTrig", "Interp",
	"AModTyp", "AModMix", "FModTyp", "FModMix", "WModTyp", "WModMix",
	"AOffset", "AGain", "FOffset", "FGain", "Oversmp", "SmooWin",
	"OModTyp", "OModMix"
};

const char *const WavePlug::paramLabels[kNumParams] = {
	"%", "%", "%", "%", "%", "%",
	"Hz", "Hz", "%", "%", "", "",
	"type", "%", "type", "%", "type", "%",
	"%", "dB", "Hz", "octaves", "", "samples",
	"type", "%"
};

const char *const WavePlug::initParamHelpTexts[kNumParams] = {
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

const char *const WavePlug::modFuncUHelpTexts[kNModTypesU][2] = {
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

const char *const WavePlug::modFuncSHelpTexts[kNModTypesS][2] = {
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

const char *const WavePlug::modFuncFHelpTexts[kNModTypesF][2] = {
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

const float WavePlug::initParamValues[kNumParams] = {
	0.1f, 0.8f, 0.05f, 0.75f, 0.783f, 0.217f, 0.00055f, 0.73f, 0.0f, 0.0f, 0.0f, 1.0f,
	0.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 0.0f,
	0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
	0.0f, 0.0f
};

const WavePlug::funcfcp WavePlug::paramDisplayers[kNumParams] = {
	&WavePlug::aIncLagDisplayer,
	&WavePlug::aDecLagDisplayer,
	&WavePlug::gateLevelDisplayer,
	&WavePlug::gateLevelDisplayer,
	&WavePlug::trigLevelDisplayer,
	&WavePlug::trigLevelDisplayer,
	&WavePlug::fMinMaxDisplayer,
	&WavePlug::fMinMaxDisplayer,
	&WavePlug::fwLagDisplayer,
	&WavePlug::fwLagDisplayer,
	&WavePlug::onOffDisplayer,
	&WavePlug::onOffDisplayer,
	
	&WavePlug::usignModTypeDisplayer,
	&WavePlug::pcntDisplayer,
	
	&WavePlug::usignModTypeDisplayer,
	&WavePlug::pcntDisplayer,
	
	&WavePlug::funcModTypeDisplayer,
	&WavePlug::pcntDisplayer,
	
	&WavePlug::aOffsetDisplayer,
	&WavePlug::aGainDisplayer,
	&WavePlug::fOffsetDisplayer,
	&WavePlug::fGainDisplayer,
	&WavePlug::oversamplingDisplayer,
	&WavePlug::smoothingWinDisplayer,
	
	&WavePlug::signModTypeDisplayer,
	&WavePlug::pcntDisplayer
};

const WavePlug::methodf WavePlug::paramSetters[kNumParams] = {
	&WavePlug::aAIWSetter,
	&WavePlug::aADWSetter,
	&WavePlug::aAGLSetter,
	&WavePlug::aSGLSetter,
	&WavePlug::aFHTSetter,
	&WavePlug::aFLTSetter,
	&WavePlug::aFMinSetter,
	&WavePlug::aFMaxSetter,
	&WavePlug::aFWSetter,
	&WavePlug::aWWSetter,
	&WavePlug::aITSetter,
	&WavePlug::aWISetter,
	
	&WavePlug::mAModSetter,
	&WavePlug::mAMixSetter,
	
	&WavePlug::mFModSetter,
	&WavePlug::mFMixSetter,
	
	&WavePlug::mWModSetter,
	&WavePlug::mWMixSetter,
	
	&WavePlug::sAOffSetter,
	&WavePlug::sAGainSetter,
	&WavePlug::sFOffSetter,
	&WavePlug::sFGainSetter,
	&WavePlug::sOverSetter,
	&WavePlug::sWindSetter,
	
	&WavePlug::mOModSetter,
	&WavePlug::mOMixSetter
};

