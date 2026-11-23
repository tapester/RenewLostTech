#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {

// Saved state: a format number, the parameter count, then every parameter value.
constexpr int kStateFormat = 1;

class CoreParameter : public juce::AudioProcessorParameter {
public:
	CoreParameter(LostTechCore &c, int i) : core(c), index(i) {}

	float getValue() const override {return core.getParameter(index);}
	void setValue(float value) override {core.setParameter(index, value);}
	float getDefaultValue() const override {return LostTechCore::getParameterDefault(index);}

	juce::String getName(int maxLength) const override {
		char text[64] = "";
		core.getParameterName(index, text);
		return juce::String(text).substring(0, maxLength);
	}
	juce::String getLabel() const override {
		char text[64] = "";
		core.getParameterLabel(index, text);
		return text;
	}
	juce::String getText(float value, int maxLength) const override {
		char text[64] = "";
		core.getParameterDisplay(index, value, text);
		return juce::String(text).substring(0, maxLength);
	}
	// shortcut: typed values are ignored (the host keeps the current value), because the
	// core only formats values and cannot parse them; add a search over 0..1 if hosts need it.
	float getValueForText(const juce::String &) const override {return getValue();}

	bool isAutomatable() const override {return index != kPlugVersion;}

private:
	LostTechCore &core;
	int index;
};

} // namespace

LostTechProcessor::LostTechProcessor() :
	AudioProcessor(BusesProperties()
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
	for (int i = 0; i < kNumAllParams; i++)
		addParameter(new CoreParameter(core, i));
}

void LostTechProcessor::prepareToPlay(double sampleRate, int) {
	core.setSampleRate((float) sampleRate);
	core.resume();
}

void LostTechProcessor::releaseResources() {core.suspend();}

// shortcut: stereo only, like the VST2 plug-in (it always had two inputs and two outputs).
bool LostTechProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
	return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
	       layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LostTechProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &) {
	if (buffer.getNumChannels() < 2)
		return;
	// The core reads each input sample before it writes the output, so in-place is safe.
	float *channels[2] = {buffer.getWritePointer(0), buffer.getWritePointer(1)};
	core.process(channels, channels, buffer.getNumSamples());
}

juce::AudioProcessorEditor *LostTechProcessor::createEditor() {
	return new LostTechEditor(*this);
}

void LostTechProcessor::getStateInformation(juce::MemoryBlock &destData) {
	juce::MemoryOutputStream out(destData, false);
	out.writeInt(kStateFormat);
	out.writeInt(kNumAllParams);
	for (int i = 0; i < kNumAllParams; i++)
		out.writeFloat(core.getParameter(i));
}

void LostTechProcessor::setStateInformation(const void *data, int sizeInBytes) {
	juce::MemoryInputStream in(data, (size_t) sizeInBytes, false);
	if (sizeInBytes < 8 || in.readInt() != kStateFormat || in.readInt() != kNumAllParams ||
	    sizeInBytes != 8 + 4 * kNumAllParams)
		return;
	for (int i = 0; i < kNumAllParams; i++) {
		core.setParameter(i, in.readFloat());
		// Tell the editor; not the host, which is the one loading this state.
		getParameters()[i]->sendValueChangedMessageToListeners(core.getParameter(i));
	}
}

// Entry point JUCE's plug-in wrappers look for.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
	return new LostTechProcessor();
}
