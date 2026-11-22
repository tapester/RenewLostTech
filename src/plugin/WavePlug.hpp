/*
Copyright (c) 2006-2007 Johan Sarge

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

#ifndef WP_WAVEPLUG_HPP
#define WP_WAVEPLUG_HPP

#include "wpstdinclude.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include "audioeffectx.h"
#include "LostTechCore.hpp"

// VST 2 shell: the host-facing API. All audio and parameter work is done by LostTechCore.
class WavePlug : public AudioEffectX {
public:
	typedef LostTechCore::funcfcp funcfcp;
	
	// Displayers used by the editor.
	static funcfcp getParamDisplayer(int index) {return LostTechCore::getParamDisplayer(index);}
	static void bufferSizeKByteDisplayer(float value, char *text) {LostTechCore::bufferSizeKByteDisplayer(value, text);}
	static void bufferSizeHzDisplayer(float value, char *text) {LostTechCore::bufferSizeHzDisplayer(value, text);}
	static void bufferSizeMillisDisplayer(float value, char *text) {LostTechCore::bufferSizeMillisDisplayer(value, text);}
	
	WavePlug(audioMasterCallback audioMaster);
	virtual ~WavePlug();
	
	// Plug info.
	virtual bool getEffectName(char* name) override;
	virtual bool getVendorString(char* text) override;
	virtual bool getProductString(char* text) override;
	virtual VstInt32 getVendorVersion() override;
	virtual VstPlugCategory getPlugCategory() override;
	virtual VstInt32 canDo(char *text) override;
	
	// Programs.
	virtual void setProgram(VstInt32 program) override;
	virtual void setProgramName(char *name) override;
	virtual void getProgramName(char *name) override;
	virtual bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char *text) override;
	
	// Parameters.
	virtual void setParameter(VstInt32 index, float value) override;
	virtual float getParameter(VstInt32 index) override {return core.getParameter(index);}
	virtual void getParameterDisplay(VstInt32 index, char *text) override {core.getParameterDisplay(index, text);}
	virtual void getParameterName(VstInt32 index, char *text) override {core.getParameterName(index, text);}
	virtual void getParameterLabel(VstInt32 index, char *label) override {core.getParameterLabel(index, label);}
	
	// Processing.
	virtual void setSampleRate(float sampleRate) override;
	virtual bool setBypass(bool onOff) override {return core.setBypass(onOff);}
	virtual void suspend() override {core.suspend();}
	virtual void resume() override {core.resume();}
	virtual void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override {
		core.process(inputs, outputs, sampleFrames);
	}
	
	// Used by the editor and the entry point.
	const char *getVersionString() {return core.getVersionString();}
	const char *getParamHelpText(int index) {return core.getParamHelpText(index);}
	bool isOperational() {return core.isOperational();}
	float getAmplitude(int channel, bool postmod = false) {return core.getAmplitude(channel, postmod);}
	float getFrequency(int channel, bool postmod = false) {return core.getFrequency(channel, postmod);}
	
private:
	LostTechCore core;
	// Only the host thread touches the program name, so it needs no lock.
	char programName[32];
};

#endif
