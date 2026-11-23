#pragma once

#include <array>
#include <cstdio>
#include "BinaryData.h"
#include "PluginProcessor.h"
#include "wpfunc.hpp"

// A port of the original VSTGUI panel. The layout is the original 512x488 grid of 64-pixel
// cells, but everything is drawn with JUCE's own graphics (only the logo is an SVG), so the
// whole panel scales to any window size.
namespace lostgui {

constexpr int kW = 512, kH = 488, kCell = 64, kTop = 20, kBarY = kTop + 7 * kCell;
// Lock buttons: one per parameter, then one per component, then the global one.
constexpr int kComponentLock = kNumParams, kGlobalLock = kNumParams + 6, kNLocks = kNumParams + 7;

// Grid cell (column, row) of each parameter, for channel 1 and channel 2.
const int8_t cells[kNumParams][2][2] = {
	{{0,0},{4,0}}, {{1,0},{5,0}}, {{2,0},{6,0}}, {{3,0},{7,0}},
	{{0,1},{4,1}}, {{1,1},{5,1}}, {{2,1},{6,1}}, {{3,1},{7,1}},
	{{0,2},{5,2}}, {{1,2},{6,2}}, {{2,2},{7,2}}, {{2,2},{7,2}},
	{{1,3},{6,3}}, {{1,4},{6,4}}, {{2,3},{5,3}}, {{2,4},{5,4}}, {{3,3},{4,3}}, {{3,4},{4,4}},
	{{0,5},{5,5}}, {{1,5},{6,5}}, {{0,6},{5,6}}, {{1,6},{6,6}}, {{2,5},{7,5}}, {{2,6},{7,6}},
	{{3,5},{4,5}}, {{3,6},{4,6}}};
// Which component lock also covers a parameter: analyzer, AMod, FMod, WMod, synthesizer, OMod.
const int componentOf[kNumParams] = {0,0,0,0,0,0,0,0,0,0,0,0, 1,1, 2,2, 3,3, 4,4,4,4,4,4, 5,5};
// Cell of each component lock (the six components, then global), for channel 1 and 2.
const int8_t lockCells[7][2][2] = {
	{{3,0},{7,0}}, {{1,3},{6,3}}, {{2,3},{5,3}}, {{3,3},{4,3}}, {{2,5},{7,5}}, {{3,5},{4,5}}, {{0,3},{7,3}}};

const char *const labels[kNumParams] = {
	"AIncLag %", "ADecLag %", "GateLvlA %", "GateLvlS %", "HighTrig %", "LowTrig %", "FMin Hz", "FMax Hz",
	"FLag %", "WLag %", "InvTrig", "Interp",
	"AModType", "AModMix %", "FModType", "FModMix %", "WModType", "WModMix %",
	"AOffset %", "AGain dB", "FOffset Hz", "FGain oct", "Oversmpl", "SmooWin",
	"OModType", "OModMix %"};

// Pointer colours, as in the original knob_handle*.bmp files (0 = the two on/off buttons).
const juce::uint32 pointerColours[kNumParams] = {
	0xffff816d, 0xffffd56d, 0xffd1ff6d, 0xff77ff6d, 0xff6dffb9, 0xff6dedff, 0xff6d96ff, 0xff9a6dff,
	0xfff16dff, 0xffff6db2, 0, 0,
	0xffff816d, 0xffffea6d, 0xffa4ff6d, 0xff6dffa1, 0xff6dedff, 0xff6d81ff,
	0xffff816d, 0xffedff6d, 0xff6dff7e, 0xff6dedff, 0xff7e6dff, 0xffff6ded,
	0xffc76dff, 0xffff6dca};

inline juce::Point<int> cellOrigin(int col, int row) {return {col * kCell, kTop + row * kCell};}
inline juce::Point<int> paramOrigin(int ch, int p) {
	auto o = cellOrigin(cells[p][ch][0], cells[p][ch][1]);
	if (p == kInterp) o.y += 32; // The two buttons share a cell.
	return o;
}
inline bool isButton(int p) {return p == kInvTrig || p == kInterp;}
inline juce::Font smallFont() {return juce::Font(juce::FontOptions(10.0f));}

// Text that the editor's timer pushes in.
class Readout : public juce::Component {
public:
	explicit Readout(juce::Colour c = juce::Colours::black, juce::Justification j = juce::Justification::centred) :
		colour(c), justification(j) {setInterceptsMouseClicks(false, false);}
	void set(const juce::String &s) {if (s != text) {text = s; repaint();}}
	void paint(juce::Graphics &g) override {
		g.setColour(colour);
		g.setFont(smallFont());
		g.drawFittedText(text, getLocalBounds(), justification, 1, 0.75f);
	}
private:
	juce::String text;
	juce::Colour colour;
	juce::Justification justification;
};

// 6x6 lock square: light when off, red when on.
class LockButton : public juce::Button {
public:
	LockButton() : Button({}) {setClickingTogglesState(true);}
	void paintButton(juce::Graphics &g, bool, bool) override {
		g.fillAll(juce::Colours::black);
		g.setColour(getToggleState() ? juce::Colour(0xffff7d6c) : juce::Colour(0xffebebeb));
		g.fillRect(getLocalBounds().reduced(1));
	}
};

// 16x16 on/off button: black when off, four red arrows when on.
class SquareButton : public juce::Button {
public:
	SquareButton() : Button({}) {setClickingTogglesState(true);}
	void paintButton(juce::Graphics &g, bool, bool) override {
		g.fillAll(juce::Colours::black);
		if (!getToggleState()) return;
		auto c = getLocalBounds().toFloat().getCentre();
		g.setColour(juce::Colour(0xff520000));
		g.drawEllipse(c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f, 1.0f);
		g.setColour(juce::Colour(0xffff836f));
		juce::Path arrow;
		arrow.addTriangle(0.0f, -7.0f, -2.5f, -3.5f, 2.5f, -3.5f);
		for (int i = 0; i < 4; i++)
			g.fillPath(arrow, juce::AffineTransform::rotation(i * juce::MathConstants<float>::halfPi).translated(c));
	}
};

// The 30x10 HELP switch: white on black when off, inverted when on.
class HelpButton : public juce::Button {
public:
	HelpButton() : Button({}) {setClickingTogglesState(true);}
	void paintButton(juce::Graphics &g, bool, bool) override {
		const bool on = getToggleState();
		g.fillAll(on ? juce::Colours::white : juce::Colours::black);
		g.setColour(on ? juce::Colours::black : juce::Colours::white);
		g.drawRect(getLocalBounds(), 1);
		g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
		g.drawText("HELP", getLocalBounds(), juce::Justification::centred, false);
	}
};

// Black knob disc with a coloured triangle that sweeps 270 degrees, from 7:30 to 4:30.
struct KnobLook : juce::LookAndFeel_V4 {
	void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float pos, float, float,
	                      juce::Slider &s) override {
		auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
		g.setColour(juce::Colours::black);
		g.fillEllipse(c.x - 15.0f, c.y - 15.0f, 30.0f, 30.0f);
		const float a = juce::degreesToRadians(-135.0f + 270.0f * pos);
		auto p = c + juce::Point<float>(std::sin(a), -std::cos(a)) * 10.0f;
		juce::Path t;
		t.addTriangle(p.x, p.y - 3.5f, p.x - 4.0f, p.y + 3.5f, p.x + 4.0f, p.y + 3.5f);
		auto colour = s.findColour(juce::Slider::thumbColourId);
		g.setColour(colour);
		g.fillPath(t);
		g.setColour(colour.darker(0.7f));
		g.strokePath(t, juce::PathStrokeType(1.0f));
	}
	// The buffer-size box is just white text on black, like the original menu (no arrow).
	void drawComboBox(juce::Graphics &g, int, int, bool, int, int, int, int, juce::ComboBox &) override {
		g.fillAll(juce::Colours::black);
	}
	void positionComboBoxText(juce::ComboBox &box, juce::Label &label) override {
		label.setBounds(1, 0, box.getWidth() - 2, box.getHeight());
		label.setFont(getComboBoxFont(box));
	}
	juce::Font getComboBoxFont(juce::ComboBox &) override {return smallFont();}
};

// The static artwork: background, dividers, and the fixed labels.
class Backdrop : public juce::Component {
public:
	Backdrop() :
		logo(juce::Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize)) {
		setInterceptsMouseClicks(false, false);
		setBufferedToImage(true); // The logo has thousands of shapes: render it once per size.
	}
	void paint(juce::Graphics &g) override {
		g.fillAll(juce::Colour(0xffebebeb));
		g.setColour(juce::Colours::black);
		g.fillRect(0, 0, kW, kTop);
		g.fillRect(0, kBarY, kW, kH - kBarY);
		g.fillRect(255, kTop, 2, 128);                  // Channel divider above the logo.
		g.fillRect(0, 211, kW, 2);                      // Row 3 top.
		g.fillRect(0, 339, kW, 2);                      // Row 5 top.
		for (int x : {63, 127, 191, 255, 319, 383, 447})
			g.fillRect(x, 211, 2, 130);
		for (int x : {191, 255, 319})
			g.fillRect(x, 339, 2, kBarY - 339);
		g.fillRect(191, 147, 130, 1);                   // Logo box.
		g.fillRect(191, 147, 1, 66);
		g.fillRect(320, 147, 1, 66);
		if (logo != nullptr)
			logo->drawWithin(g, juce::Rectangle<float>(192.0f, 148.0f, 128.0f, 64.0f),
			                 juce::RectanglePlacement::stretchToFit, 1.0f);

		g.setColour(juce::Colours::white);                    // Frames of the buffer-size boxes.
		for (int x : {4, 108, 163, 218})
			g.drawRect(x, 4, x == 4 ? 30 : 35, 12, 1);
		g.setFont(smallFont());
		g.drawText("buffer size", 36, 5, 70, 10, juce::Justification::left, false);
		g.drawText("kB", 146, 5, 18, 10, juce::Justification::left, false);
		g.drawText("Hz", 201, 5, 18, 10, juce::Justification::left, false);
		g.drawText("ms", 256, 5, 18, 10, juce::Justification::left, false);

		g.setColour(juce::Colours::black);
		for (int ch = 0; ch < 2; ch++)
			for (int p = 0; p < kNumParams; p++) {
				auto o = paramOrigin(ch, p);
				g.drawText(labels[p], o.x, o.y + (isButton(p) ? 17 : 49), kCell, 14, juce::Justification::centred, false);
			}
		for (int col : {0, 7})
			for (int row : {3, 4}) {
				auto o = cellOrigin(col, row);
				g.drawText(row == 3 ? "PreAmp %" : "PostAmp %", o.x, o.y + 23, kCell, 14, juce::Justification::centred, false);
				g.drawText(row == 3 ? "PreFrq Hz" : "PostFrq Hz", o.x, o.y + 49, kCell, 14, juce::Justification::centred, false);
			}
	}
private:
	std::unique_ptr<juce::Drawable> logo;
};

} // namespace lostgui

class LostTechEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
	explicit LostTechEditor(LostTechProcessor &p) :
		AudioProcessorEditor(p), core(p.getCore()),
		version(juce::Colours::white, juce::Justification::right),
		helpBar(juce::Colours::white, juce::Justification::left) {
		using namespace lostgui;
		for (auto *param : p.getParameters())
			params.push_back(param);

		addAndMakeVisible(content);
		content.addAndMakeVisible(backdrop);
		backdrop.setBounds(0, 0, kW, kH);

		for (int i = 0; i < 2 * kNumParams; i++) {
			auto &c = controls[i];
			c.ch = i / kNumParams;
			c.p = i % kNumParams;
			c.index = kNumMonoParams + i;
			auto o = paramOrigin(c.ch, c.p);
			if (isButton(c.p)) {
				c.button.setBounds(o.x + 12, o.y + 3, 16, 16);
				c.button.onClick = [this, &c] {edit(c, c.button.getToggleState() ? 1.0f : 0.0f);};
				content.addAndMakeVisible(c.button);
				c.value.setBounds(o.x + 31, o.y + 3, 30, 10);
			} else {
				auto &s = c.slider;
				s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
				s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
				s.setRange(0.0, 1.0, 0.0);
				s.setDoubleClickReturnValue(true, LostTechCore::getParameterDefault(c.index));
				s.setColour(juce::Slider::thumbColourId, juce::Colour(pointerColours[c.p]));
				s.setLookAndFeel(&knobLook);
				s.setBounds(o.x + 15, o.y + 3, 34, 34);
				s.onDragStart = [this, &c] {
					c.dragging = true;
					params[(size_t) c.index]->beginChangeGesture();
					touched = c.index;
				};
				s.onDragEnd = [this, &c] {
					params[(size_t) c.index]->endChangeGesture();
					c.dragging = false;
				};
				s.onValueChange = [this, &c] {edit(c, (float) c.slider.getValue());};
				content.addAndMakeVisible(s);
				c.value.setBounds(o.x + 2, o.y + 39, 59, 10);
			}
			content.addAndMakeVisible(c.value);

			locks[c.ch][c.p].setBounds(o.x + 3, o.y + 3, 6, 6);
		}
		for (int l = 0; l < 7; l++)
			for (int ch = 0; ch < 2; ch++) {
				auto o = cellOrigin(lockCells[l][ch][0], lockCells[l][ch][1]);
				locks[ch][kComponentLock + l].setBounds(o.x + 55, o.y + 3, 6, 6);
			}
		for (int ch = 0; ch < 2; ch++)
			for (int l = 0; l < kNLocks; l++) {
				// The two channels' buttons of one lock always agree.
				locks[ch][l].onClick = [this, ch, l] {
					locks[1 - ch][l].setToggleState(locks[ch][l].getToggleState(), juce::dontSendNotification);
				};
				content.addAndMakeVisible(locks[ch][l]);
			}

		// Pre/post-modulation monitors in the two outer columns.
		for (int side = 0; side < 2; side++)
			for (int row = 0; row < 2; row++)
				for (int k = 0; k < 2; k++) {
					auto o = cellOrigin(side == 0 ? 0 : 7, 3 + row);
					auto &r = monitors[side][row][k];
					r.setBounds(o.x + 2, o.y + (k == 0 ? 13 : 39), 59, 10);
					content.addAndMakeVisible(r);
				}

		bufferSize.setBounds(5, 5, 28, 10);
		for (int n = 1; n <= 128; n *= 2)
			bufferSize.addItem(juce::String(n), bufferSize.getNumItems() + 1);
		bufferSize.setLookAndFeel(&knobLook);
		bufferSize.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
		bufferSize.setColour(juce::ComboBox::textColourId, juce::Colours::white);
		bufferSize.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
		bufferSize.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
		bufferSize.onChange = [this] {
			touched = kBufferSize;
			gesture(kBufferSize, (float) (bufferSize.getSelectedId() - 1) / 7.0f);
		};
		content.addAndMakeVisible(bufferSize);
		for (int k = 0; k < 3; k++) {
			bufferInfo[k].setBounds(109 + 55 * k, 5, 33, 10);
			content.addAndMakeVisible(bufferInfo[k]);
		}

		version.setBounds(407, 5, 100, 10);
		version.set(core.getVersionString());
		content.addAndMakeVisible(version);

		helpBar.setBounds(5, kBarY + 2, 465, 13);
		content.addAndMakeVisible(helpBar);
		help.setBounds(475, kBarY + 4, 30, 10);
		content.addAndMakeVisible(help);

		setResizeLimits(kW / 2, kH / 2, kW * 4, kH * 4);
		getConstrainer()->setFixedAspectRatio((double) kW / kH);
		setSize(kW, kH); // shortcut: the window size is not saved with the plug-in state.
		timerCallback();
		startTimerHz(30);
	}

	~LostTechEditor() override {
		for (auto &c : controls)
			c.slider.setLookAndFeel(nullptr);
		bufferSize.setLookAndFeel(nullptr);
	}

	void resized() override {
		content.setBounds(0, 0, lostgui::kW, lostgui::kH);
		content.setTransform(juce::AffineTransform::scale((float) getWidth() / lostgui::kW));
	}

private:
	using Readout = lostgui::Readout;

	struct Control {
		juce::Slider slider;
		lostgui::SquareButton button;
		Readout value;
		int ch = 0, p = 0, index = 0;
		bool dragging = false;
	};

	// One change of one parameter, as a host automation gesture.
	void gesture(int index, float value) {
		auto &prm = *params[(size_t) index];
		prm.beginChangeGesture();
		prm.setValueNotifyingHost(value);
		prm.endChangeGesture();
	}

	// A lock links the two channels' copies of a parameter: its own, its component's, or the global one.
	bool linked(int p) const {
		using namespace lostgui;
		return locks[0][p].getToggleState() || locks[0][kComponentLock + componentOf[p]].getToggleState() ||
		       locks[0][kGlobalLock].getToggleState();
	}

	// The user moved a control. During a drag the slider already holds the gesture open.
	void edit(Control &c, float value) {
		touched = c.index;
		if (c.dragging)
			params[(size_t) c.index]->setValueNotifyingHost(value);
		else
			gesture(c.index, value);
		if (linked(c.p))
			gesture(controls[(size_t) ((1 - c.ch) * kNumParams + c.p)].index, value);
	}

	// Refreshes every control from the parameters (host automation, state loads) and the live monitors.
	void timerCallback() override {
		using namespace lostgui;
		char text[64];
		for (auto &c : controls) {
			const float v = params[(size_t) c.index]->getValue();
			if (isButton(c.p))
				c.button.setToggleState(v >= 0.5f, juce::dontSendNotification);
			else if (!c.dragging && !c.slider.isMouseButtonDown())
				c.slider.setValue(v, juce::dontSendNotification);
			core.getParameterDisplay(c.index, v, text);
			c.value.set(text);
		}

		for (int ch = 0; ch < 2; ch++)
			for (int post = 0; post < 2; post++) {
				auto &r = monitors[ch][post];
				std::snprintf(text, sizeof text, "%1.2f", 100.0f * core.getAmplitude(ch, post != 0));
				r[0].set(text);
				std::snprintf(text, sizeof text, "%1.2f", unsignedToHz(core.getFrequency(ch, post != 0)));
				r[1].set(text);
			}

		const float size = params[kBufferSize]->getValue();
		if (!bufferSize.isPopupActive())
			bufferSize.setSelectedItemIndex(juce::roundToInt(7.0f * size), juce::dontSendNotification);
		LostTechCore::bufferSizeKByteDisplayer(size, text);
		bufferInfo[0].set(text);
		LostTechCore::bufferSizeHzDisplayer(size, text);
		bufferInfo[1].set(text);
		LostTechCore::bufferSizeMillisDisplayer(size, text);
		bufferInfo[2].set(text);

		// The bar shows the help for the last control used while HELP is on, otherwise the licence notice.
		// The notice tells users where to get the source (this build is distributed under the AGPLv3).
		helpBar.set(help.getToggleState() && touched >= 0
			? juce::String(core.getParamHelpText(touched))
			: juce::String("Free software under the GNU AGPLv3, built with JUCE. Source: github.com/tapester/RenewLostTech"));
	}

	LostTechCore &core;
	std::vector<juce::AudioProcessorParameter *> params;
	lostgui::KnobLook knobLook;
	juce::Component content;
	lostgui::Backdrop backdrop;
	std::array<Control, 2 * kNumParams> controls;
	lostgui::LockButton locks[2][lostgui::kNLocks];
	Readout monitors[2][2][2]; // [channel][pre/post][amplitude/frequency]
	juce::ComboBox bufferSize;
	Readout bufferInfo[3] = {Readout(juce::Colours::white, juce::Justification::right),
	                         Readout(juce::Colours::white, juce::Justification::right),
	                         Readout(juce::Colours::white, juce::Justification::right)};
	Readout version, helpBar;
	lostgui::HelpButton help;
	int touched = -1;
};
