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

#include "WavePlug.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#ifndef WP_NO_GUI
#include "WavePlugEditor.hpp"
#endif

WavePlug::WavePlug(audioMasterCallback audioMaster) :
	AudioEffectX(audioMaster, 1, kNumAllParams)
{
	setNumInputs(2);
	setNumOutputs(2);
	setUniqueID(CCONST('C','Q','C','Q'));
	canProcessReplacing();
	std::strcpy(programName, "Default");
	
	editor = NULL;
	
#ifndef WP_NO_GUI
	try {
		editor = new WavePlugEditor(this);
	}
	catch (std::bad_alloc e) {
		core.setOperational(false);
	}
#endif
}

WavePlug::~WavePlug() {
#ifndef WP_NO_GUI
	delete editor;
	editor = NULL;
#endif
}

bool WavePlug::getEffectName(char *text) {
	std::strcpy(text, "Lost Technology");
	return true;
}

bool WavePlug::getProductString(char *text) {
	std::strcpy(text, "Lost Recombinant Audio Technology");
	return true;
}

bool WavePlug::getVendorString(char *text) {
	std::strcpy(text, "Transvaal Audio");
	return true;
}

VstInt32 WavePlug::getVendorVersion() {return WP_VENDOR_VERSION;}

VstPlugCategory WavePlug::getPlugCategory() {return kPlugCategEffect;}

VstInt32 WavePlug::canDo(char *text) {
	if (!std::strcmp(text, "sendVstEvents") ||
	    !std::strcmp(text, "sendVstMidiEvent") ||
	    !std::strcmp(text, "sendVstTimeInfo"))
		return -1l;
	if (!std::strcmp(text, "receiveVstEvents") ||
	    !std::strcmp(text, "receiveVstMidiEvent") ||
	    !std::strcmp(text, "receiveVstTimeInfo"))
		return -1l;
	else if (!std::strcmp(text, "offline") || !std::strcmp(text, "noRealTime"))
		return -1l;
	else if (!std::strcmp(text, "1in1out") || !std::strcmp(text, "1in2out") ||
		       !std::strcmp(text, "2in1out") || !std::strcmp(text, "2in2out"))
		return 1l;
	else if (!std::strcmp(text, "2in4out") || !std::strcmp(text, "4in2out") ||
		       !std::strcmp(text, "4in4out") || !std::strcmp(text, "4in8out") ||
	         !std::strcmp(text, "8in4out") || !std::strcmp(text, "8in8out"))
		return -1l;
	else if (!std::strcmp(text, "bypass"))
		return 1l;
	return 0l;
}

void WavePlug::setProgram(VstInt32 program) {}

void WavePlug::setProgramName(char *name) {
	std::snprintf(programName, sizeof(programName), "%s", name);
}

void WavePlug::getProgramName(char *name) {
	std::snprintf(name, 32, "%s", programName);
}

bool WavePlug::getProgramNameIndexed(VstInt32 category, VstInt32 index, char *text) {
	if ((category == 0 || category == -1) && index == 0) {
		std::snprintf(text, 32, "%s", programName);
		return true;
	}
	return false;
}

void WavePlug::setParameter(VstInt32 index, float value) {
	core.setParameter(index, value);
	
#ifndef WP_NO_GUI
	if (editor != NULL && index >= 0 && index < kNumAllParams)
		((AEffGUIEditor *) editor)->setParameter(index, clamp01(value));
#endif
}

void WavePlug::setSampleRate(float sampleRate) {
	AudioEffectX::setSampleRate(sampleRate);
	core.setSampleRate(sampleRate);
}
