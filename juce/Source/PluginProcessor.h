#pragma once

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

	int getNumPrograms() override {return 1;}
	int getCurrentProgram() override {return 0;}
	void setCurrentProgram(int) override {}
	const juce::String getProgramName(int) override {return "Default";}
	void changeProgramName(int, const juce::String &) override {}

	// The editor reads the live signal monitors straight from the core.
	LostTechCore &getCore() {return core;}

	void getStateInformation(juce::MemoryBlock &) override;
	void setStateInformation(const void *, int) override;

private:
	LostTechCore core;
};
