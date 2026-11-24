#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {

// Saved state: a format number, the parameter count, the program number, every parameter value, then the
// dry/wet mix. Format 1 (no program number) and format 2 (no mix, so fully wet) still load.
constexpr int kStateFormat = 3;

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
	// After the core parameters, so their indices stay those of the VST2 plug-in and of the presets.
	addParameter(mix = new juce::AudioParameterFloat(
		juce::ParameterID("drywet", 1), "Dry/Wet", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
		juce::AudioParameterFloatAttributes().withStringFromValueFunction(
			[](float v, int) {return juce::String(juce::roundToInt(100.0f * v)) + " %";})));
}

void LostTechProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
	core.setSampleRate((float) sampleRate);
	core.resume();
	dry.setSize(2, juce::jmax(1, samplesPerBlock));
	dryWet.setTarget(mix->get());
	dryWet.prepare((float) sampleRate);
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
	const int total = buffer.getNumSamples(), chunk = dry.getNumSamples();
	dryWet.setTarget(mix->get());
	if (dryWet.isTransparent() || chunk == 0) { // Fully wet, or called before prepareToPlay.
		core.process(channels, channels, total);
		return;
	}
	// A host may send more than the block size it announced: work through it in pieces.
	for (int pos = 0; pos < total; pos += chunk) {
		const int count = juce::jmin(chunk, total - pos);
		float *wet[2] = {channels[0] + pos, channels[1] + pos};
		const float *in[2] = {dry.getReadPointer(0), dry.getReadPointer(1)};
		for (int ch = 0; ch < 2; ch++)
			dry.copyFrom(ch, 0, wet[ch], count);
		core.process(wet, wet, count);
		dryWet.mix(wet, in, count);
	}
}

juce::AudioProcessorEditor *LostTechProcessor::createEditor() {
	return new LostTechEditor(*this);
}

// Selecting a preset overwrites every parameter. Re-selecting the current program does nothing,
// because some hosts replay the program number after loading the state, which would wipe the sound.
void LostTechProcessor::setCurrentProgram(int index) {
	if (index != currentProgram)
		loadPreset(index);
}

void LostTechProcessor::loadPreset(int index) {
	if (index < 0 || index >= getNumPrograms())
		return;
	currentProgram = index;
	for (int i = kPlugVersion + 1; i < kNumAllParams; i++)
		getParameters()[i]->setValueNotifyingHost(LostTechCore::getPresetValue(index, i));
}

void LostTechProcessor::getStateInformation(juce::MemoryBlock &destData) {
	juce::MemoryOutputStream out(destData, false);
	out.writeInt(kStateFormat);
	out.writeInt(kNumAllParams);
	out.writeInt(currentProgram);
	for (int i = 0; i < kNumAllParams; i++)
		out.writeFloat(core.getParameter(i));
	out.writeFloat(mix->get());
}

void LostTechProcessor::setStateInformation(const void *data, int sizeInBytes) {
	juce::MemoryInputStream in(data, (size_t) sizeInBytes, false);
	if (sizeInBytes < 8)
		return;
	const int format = in.readInt();
	if (format < 1 || format > kStateFormat || in.readInt() != kNumAllParams)
		return;
	const int headerBytes = format == 1 ? 8 : 12, mixBytes = format == kStateFormat ? 4 : 0;
	if (sizeInBytes != headerBytes + 4 * kNumAllParams + mixBytes)
		return;
	if (format >= 2) {
		const int program = in.readInt();
		if (program >= 0 && program < getNumPrograms())
			currentProgram = program;
	}
	for (int i = 0; i < kNumAllParams; i++) {
		core.setParameter(i, in.readFloat());
		// Tell the editor; not the host, which is the one loading this state.
		getParameters()[i]->sendValueChangedMessageToListeners(core.getParameter(i));
	}
	const float mixValue = format == kStateFormat ? juce::jlimit(0.0f, 1.0f, in.readFloat()) : 1.0f;
	mix->setValueNotifyingHost(mixValue); // Range is 0..1, so the value is already normalised.
}

// Entry point JUCE's plug-in wrappers look for.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
	return new LostTechProcessor();
}
