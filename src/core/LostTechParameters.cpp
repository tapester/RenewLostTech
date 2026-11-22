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

