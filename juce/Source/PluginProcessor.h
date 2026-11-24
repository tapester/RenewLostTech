#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "DryWet.hpp"
#include "LostTechCore.hpp"

// JUCE shell around LostTechCore. The core keeps its own parameter store and locking, so each
// JUCE parameter is a thin view of one core parameter, in the same order as the VST2 plug-in.
class LostTechProcessor : public juce::AudioProcessor {
public:
	LostTechProcessor();

	void prepareToPlay(double sampleRate, int) override;
	void releaseResources() override;
	bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
	void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

	juce::AudioProcessorEditor *createEditor() override;
	bool hasEditor() const override {return true;}

	const juce::String getName() const override {return "Lost Technology";}
	bool acceptsMidi() const override {return false;}
	bool producesMidi() const override {return false;}
	double getTailLengthSeconds() const override {return 0.0;}

	// Programs are the factory presets from the core. Names are fixed.
	int getNumPrograms() override {return LostTechCore::getNumPresets();}
	int getCurrentProgram() override {return currentProgram;}
	void setCurrentProgram(int index) override;
	// Applies a preset even when it is the current program (the editor's menu uses this to reset a sound).
	void loadPreset(int index);
	const juce::String getProgramName(int index) override {return LostTechCore::getPresetName(index);}
	void changeProgramName(int, const juce::String &) override {}

	// The editor reads the live signal monitors straight from the core.
	LostTechCore &getCore() {return core;}
	// Dry/wet mix of the whole plug-in (0 = input only, 1 = fully processed). Not part of the VST2 plug-in
	// or of the presets; it is saved with the state.
	juce::AudioParameterFloat &getMixParameter() {return *mix;}

	void getStateInformation(juce::MemoryBlock &) override;
	void setStateInformation(const void *, int) override;

private:
	LostTechCore core;
	juce::AudioParameterFloat *mix; // Owned by the processor, like every added parameter.
	DryWet dryWet;
	juce::AudioBuffer<float> dry;   // Input copy, sized in prepareToPlay so processBlock never allocates.
	std::atomic<int> currentProgram{0};
};
