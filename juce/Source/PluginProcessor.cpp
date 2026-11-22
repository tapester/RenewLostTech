#include "PluginProcessor.h"

LostTechProcessor::LostTechProcessor() :
	AudioProcessor(BusesProperties()
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

bool LostTechProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
	const auto &in = layouts.getMainInputChannelSet(), &out = layouts.getMainOutputChannelSet();
	return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

juce::AudioProcessorEditor *LostTechProcessor::createEditor() {
	return new juce::GenericAudioProcessorEditor(*this);
}

// Entry point JUCE's plug-in wrappers look for.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
	return new LostTechProcessor();
}
