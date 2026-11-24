#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

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
	const juce::String getProgramName(int index) override {return LostTechCore::getPresetName(index);}
	void changeProgramName(int, const juce::String &) override {}

	// The editor reads the live signal monitors straight from the core.
	LostTechCore &getCore() {return core;}

	void getStateInformation(juce::MemoryBlock &) override;
	void setStateInformation(const void *, int) override;

private:
	LostTechCore core;
	std::atomic<int> currentProgram{0};
};
