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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include "wpfunc.hpp"
#ifndef WP_NO_GUI
#include "WavePlugEditor.hpp"
#endif

// Debugging: trap floating-point exceptions on MSVC debug builds. This is a no-op on other compilers and build configurations.
/*
#include <float.h>
#if defined(_DEBUG) && defined(_MSC_VER)
   struct FpTrap {
       unsigned prev, cur;
       FpTrap()  { _controlfp_s(&prev, 0, 0);
                   _controlfp_s(&cur, 0, _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW); }
       ~FpTrap() { _clearfp(); _controlfp_s(&cur, prev, _MCW_EM); }
   };   
#endif
*/

// Flush denormals to zero while processing audio. The control register is
// architecture specific: MXCSR on x86, FPCR on arm64 (Apple Silicon).
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
struct ScopedFtz {
    unsigned prev;
    ScopedFtz() : prev(_mm_getcsr()) { _mm_setcsr(prev | 0x8040); }  // FTZ | DAZ
    ~ScopedFtz() { _mm_setcsr(prev); }
};
#elif defined(__aarch64__)
struct ScopedF