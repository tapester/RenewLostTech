/*
Copyright (c) 2006 Johan Sarge

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

#ifndef WP_WAVEPLUGEDITOR_HPP
#define WP_WAVEPLUGEDITOR_HPP

#include "wpstdinclude.h"
#include <atomic>

#include "aeffguieditor.h"
#include "waveplugparams.h"
#include "WavePlug.hpp"

class WavePlug;

// Builds the plug-in's custom VSTGUI panel and handles its control events.
class WavePlugEditor : public AEffGUIEditor, public CControlListener {
private:
	enum {
		// Lock groups cover the analyzer, five processing components, and global settings.
		kNComponents = 6,
		kNMultiLocks = kNComponents + 1,
		kNParamLocks = kNumParams + kNMultiLocks,
		kNParamControls = kNumStereoParams,
		// Each lock group and parameter has one button for each stereo channel.
		kNMultiButtons = 2 * kNMultiLocks,
		kNLockButtons = 2 * kNParamLocks,
		kHelpButtonTag = kNumAllParams + kNLockButtons
	};
	
	// Per-control layout and presentation metadata, indexed by parameter/control number.
	static const int
		controlPositions[kNParamControls][2],
		componentPositions[kNMultiButtons][2],
		controlTypes[kNumParams],
		handleBitmapIds[kNumParams],
		paramComponentNumber[kNumParams];
	
	static void ampToString(float a, char *text);
	static void freqToString(float f, char *text);
	
	// The editor updates this effect instance when controls change.
	WavePlug *wavePlug;
	
   	// setParameter() may be called on any host thread. It only records the value here;
   	// idle() applies it to the VSTGUI controls on the UI thread.
   	std::atomic<float> pendingValue[kNumAllParams];
   	std::atomic<bool>  pendingDirty[kNumAllParams];

	CColor displayBackColor, displayFontColor;
	
	CMovieBitmap *logo;
	
	// Parameter, component, and global locks share one backing array.
	bool paramLocks[kNParamLocks], *const componentLocks, *const globalLock;
	COnOffButton *lockButtons[kNLockButtons];
	
	// Stereo parameter controls and their textual value displays.
	CControl *controls[kNParamControls];
	CParamDisplay *displays[kNParamControls];
	
	// Buffer-size selection plus live signal monitors.
	COptionMenu *bufrMenu;
	CParamDisplay *bufrDispKBytes, *bufrDispHz, *bufrDispMillis;
	
	CParamDisplay *aDisp0Pre, *fDisp0Pre, *aDisp1Pre, *fDisp1Pre,
	              *aDisp0Post, *fDisp0Post, *aDisp1Post, *fDisp1Post;
	
	// Tracks the parameter whose help text should be shown while the panel is open.
	CTextLabel *helpDisplay;
	COnOffButton *helpButton;
	int lastTouchedParamIndex;
	bool showHelp, paramTouched;
	
public:
	WavePlugEditor(AudioEffect *effect);
	virtual ~WavePlugEditor();
	
	// VST host lifecycle and parameter synchronization callbacks.
	virtual bool open(void *ptr) override;
	virtual void close() override;
	virtual void idle() override;
	
	virtual void setParameter(VstInt32 index, float value) override;
	virtual void valueChanged(CControl* control) override;
	
private:
	// Helpers create controls from the shared layout and bitmap resources.
	void applyPendingParameters();
	void initKnob(int tag, CBitmap *knobBitmap, CBitmap *handleBitmap, CBitmap *lockBitmap);
	void initButton(int tag, CBitmap *buttonBitmap, CBitmap *lockBitmap);
	void initLockButton(int tag, const int *pos, CBitmap *lockBitmap, int padW = 3, int padH = 3);
	// Creates a monitor display, using editor colors when explicit colors are omitted.
	CParamDisplay *initMonitor(
		CRect &size, void (*converter)(float, char *), float value = 0.0f,
		const CColor *tColorP = NULL, const CColor *fColorP = NULL, const CColor *bColorP = NULL,
		CHoriTxtAlign hAlign = kCenterText);
};

#endif
