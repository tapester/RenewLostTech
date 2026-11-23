#pragma once

#include "PluginProcessor.h"

// JUCE's generic parameter list plus the licence notice. The notice is also what AGPLv3
// section 13 asks for: it tells users where to get the source.
class LostTechEditor : public juce::AudioProcessorEditor {
public:
	explicit LostTechEditor(LostTechProcessor &p) : AudioProcessorEditor(p), generic(p) {
		addAndMakeVisible(generic);
		notice.setText(
			"Lost Technology " + juce::String(JucePlugin_VersionString) +
			" - free software under the GNU AGPLv3, built with JUCE (juce.com). "
			"Source: github.com/tapester/RenewLostTech",
			juce::dontSendNotification);
		notice.setJustificationType(juce::Justification::centred);
		notice.setMinimumHorizontalScale(1.0f);
		addAndMakeVisible(notice);
		setResizable(true, true);
		setSize(560, 640);
	}

	void resized() override {
		auto area = getLocalBounds();
		notice.setBounds(area.removeFromBottom(40));
		generic.setBounds(area);
	}

private:
	juce::GenericAudioProcessorEditor generic;
	juce::Label notice;
};
