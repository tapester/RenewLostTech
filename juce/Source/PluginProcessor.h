#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Placeholder: passes audio through. The next migration step replaces processBlock with
// LostTechCore and declares the 54 parameters.
class LostTechProcessor : public juce::AudioProcessor {
public:
	LostTechProcessor();

	void prepareToPlay(double, int) override {}
	void releaseResources() override {}
	bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
	void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}

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

	void getStateInformation(juce::MemoryBlock &) override {}
	void setStateInformation(const void *, int) override {}
};
