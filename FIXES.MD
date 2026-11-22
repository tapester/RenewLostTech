## Must fix

**1. Reading a knob gives the old value until audio runs** (`src/plugin/WavePlug.cpp:405-456`, `:734-742`)
- **What this is:** The host sets a knob with `setParameter` and reads it back with `getParameter`.
- **Problem:** `setParameter` stores the value in a pending slot. `getParameter` reads a different slot that only the audio thread fills in, and only while it processes a block. I ran it: I set Amp Gain to 0.9 and read back 0.500 with no audio running. I got 0.900 only after the next block. A host that saves the project, records automation or redraws its own display while idle gets the old value.
- **Fix:** In `setParameter`, which already holds the lock, also write `sharedData.paramValues[index] = value;`. Then delete the `memcpy` copy-back at lines 737-738. I have not tested this fix.
- **If we skip it:** The last knob changes can be missing from saved projects in hosts that stop audio when idle.
- **FIXED**

**2. A knob value outside 0 to 1 crashes the host** (`WavePlug.cpp:405-431`, macros at `:83-97`, `src/dsp/wpmodulators.cpp:94`)
- **What this is:** Every knob value from the host is turned into a table position, for example the modulation type.
- **Problem:** Nothing checks the range. I ran it under a memory-error checker (ASan/UBSan). Setting Amp Mod Type to 1.5 reads entry 11 of an 8-entry table on the host thread. The audio thread then takes a garbage function pointer from the same overrun and segfaults. This starts above about 1.001 or below about −0.13 on the six type knobs. A broken automation curve or a crafted project file can send such a value.
- **Fix:** Add two lines at the top of `setParameter`: `if (index < 0 || index >= kNumAllParams) return;` and `value = clamp01(value);`. `clamp01` already exists in `wpfunc.hpp` and turns NaN into 0.
- **If we skip it:** One bad value takes down the whole music program.
- **FIXED**

**3. A long program name overflows a 32-byte buffer** (`WavePlug.cpp:374-380`, `WavePlug.hpp:114`)
- **What this is:** The host can rename the single preset with `setProgramName`.
- **Problem:** The code uses `strcpy` into `programName[32]` with no length check. VST says 24 characters at most but does not enforce it. I fed it 40 characters in a build with overflow checks and it aborted with "buffer overflow detected". In a normal build it silently overwrites `paramHelpTexts`, the table of help-text pointers stored right after it.
- **Fix:** `std::snprintf(programName, kVstMaxProgNameLen + 1, "%s", name);`. This also keeps `getProgramName` from writing more than 24 characters into the host's buffer.
- **If we skip it:** A host that passes a long preset name can crash the plug-in or corrupt its memory.
- **FIXED**

## Should fix

**4. A sample-rate change leaves old-rate values in place** (`src/dsp/Analyzer.cpp:59-77`, `:113-131`, `wpmodulators.cpp:50-58`, `WavePlug.cpp:712-742`)
- **What this is:** Some settings depend on the sample rate: the shortest cycle to accept, the amplitude follower speed, and the LFO speed of the "Pong" modulation type. They are computed when a knob moves.
- **Problem:** When the host changes the rate on a live instance, the code resizes buffers but does not recompute these values. I ran 44.1 kHz to 96 kHz with no knob touched:
  - Minimum cycle length stayed 45 instead of 97.
  - Amplitude rise coefficient stayed 0.500 instead of 0.273.
  - Amplitude fall coefficient stayed 0.100 instead of 0.047.
  - Pong LFO step stayed 0.000100 instead of 0.000046, so it runs 2.2 times too fast.
  
  Loading a saved project is fine, because all knobs are re-sent after the rate is set. Only a live switch breaks.
- **Fix:** In `doThreadSynchronizedDataExchange`, when a new rate arrives, copy every stored value into `newParamValues` wherever it is NaN. The existing `doParameterUpdates()` call just below then re-applies all knobs at the new rate. That is about 3 lines.
- **If we skip it:** After a rate change the plug-in sounds different until you touch each knob.
- **FIXED**

**5. The sample rate is one global shared by every instance** (`src/dsp/wpfunc.hpp:30-34`, `:49-54`, `wpfunc.cpp:24-28`)
- **What this is:** `globalSampleRate`, `globalMaxFrequency` and `globalMinPeriodLength` are plain global floats. About 20 places read them.
- **Problem:** Two instances at different rates overwrite each other. I ran it: A ran at 44.1 kHz, then B started at 96 kHz, and the global became 96000. When A processed again it still read 96000. A then uses the wrong rate for its limits and time constants. Different audio threads also write the globals without any lock.
- **Fix:** Keep the rate in each plug-in instance and pass it to the analyzer, synthesizer and modulators instead of reading the globals.
- **If we skip it:** Wrong sound, or a data race, if a host runs two projects at different rates in one process. This is rare.
- **FIXED**

**6. The audio thread allocates and frees memory** (`WavePlug.cpp:694-697`, `:715-722`, `src/dsp/Synthesizer.cpp:57-90`, `src/dsp/BufferManager.cpp:76-135`)
- **What this is:** The audio thread is the one that must finish each block in a few milliseconds. Memory allocation can wait on a system lock.
- **Problem:** I counted `new` and `delete` calls inside `processReplacing`:

  | Moment | Calls |
  |---|---|
  | First block after load | 22 new, 12 delete |
  | Turning the Smoothing Window on | 2 new |
  | Every `suspend()`/`resume()` (play/stop in many hosts) | 2 new, 2 delete |
  | A Buffer Size change | 10 new, 10 delete |
  | Steady state | 0 |

- **Fix:** The cheapest step is to make `Synthesizer::reset()` reuse `fadeBuffer` when it is already big enough. Finding 9 removes the rest.
- **If we skip it:** Occasional clicks at play/stop and when knobs change.

**7. Tests miss the risky code, and plug-in validation never fails a build** (`tests/dsp/wpfunc_tests.cpp`, `.github/workflows/macos.yml:68-76`, pluginval block in `CMakeLists.txt`)
- **What this is:** The one test checks helper maths in `wpfunc.hpp`. pluginval is a third-party tool that checks whether a plug-in behaves.
- **Problem:** No test covers the cycle finder, the output ring buffer, the modulators, the knob mappings or the rate change. Findings 1, 2 and 4 would each be caught by a few lines of test. The macOS pluginval step has `continue-on-error: true`. The Windows job never runs it. The local pluginval tests only exist if a git-ignored `pluginval.exe` is present.
- **Fix:** Add three small tests and build them with `-fsanitize=address,undefined` on the macOS runner: out-of-range values (−1, 1.5, NaN), a set-then-get round trip, and the rate-change check. My rate-change probe is about 60 lines and needs no SDK. Remove `continue-on-error` once pluginval passes.
- **If we skip it:** These bugs can return without anyone noticing.
- **FIXED**

**8. CI uses mutable third-party references and does not build the documented way** (`.github/workflows/windows.yml:17-28`, `macos.yml:68-76`)
- **What this is:** These workflows build the DLLs and the macOS bundle that people download.
- **Problem:** `lukka/get-cmake@latest` and `ilammy/msvc-dev-cmd@v1` can change without notice. The macOS job downloads pluginval from `releases/latest` and runs it with no checksum. There is no `permissions:` block. Windows CI configures "Visual Studio 17 2022", but the README and the preset use "Visual Studio 18 2026", so the documented build never runs in CI.
- **Fix:** Pin the two actions to commit SHAs, add `permissions: contents: read`, and pin pluginval to a version and checksum. Make the Windows job use `cmake --preset windows-x64`, or change the README.
- **If we skip it:** A changed action or tool can alter the shipped binaries, and a preset break goes unseen.
- **FIXED:** Actions are pinned to commit SHAs, `permissions: contents: read` is set, the pluginval download is checked against a SHA-256, and the Windows job now runs `cmake --preset windows-x64` on `windows-2025-vs2026`.

**9. Out-of-memory recovery code for buffers that never fail to allocate** (`src/dsp/BufferManager.cpp`/`.hpp`, `WavePlug.cpp:724-730`, `:743-746`, `:796-798`, `:815-833`, `Analyzer.cpp:79-111`, `Synthesizer.cpp:92-126`)
- **What this is:** A home-made buffer tracker with its own list and lock. On top of it sit "if allocation fails, restore the old size, else go silent" paths.
- **Problem:** At the largest Buffer Size setting all buffers together are about 4.4 MB at 44.1 kHz. A 64-bit process does not fail at that size, so the recovery paths can never be exercised. If they ever ran, the do-nothing handler would leave the output buffer unwritten, so the host would hear whatever was in it.
- **Fix:** Use `std::vector<float>` and `std::vector<int>` members in `Analyzer` and `Synthesizer`. Delete `BufferManager`. Keep one `std::recursive_mutex` in `WavePlug`. Drop the bool returns, the rollback and the emergency handlers. That is about −250 lines. Do it after the tests from finding 7 exist.
- **If we skip it:** Nothing breaks, but every later change (finding 6, for example) has to go through this layer.

**10. An audio path that never runs** (`WavePlug.cpp:111-122`, `:559-564`, `:958-1000`, `WavePlug.hpp:75`, `:149`, `:247-256`)
- **What this is:** VST has an older `process()` call that adds to the output and a newer `processReplacing()` that overwrites it. This plug-in declares only the new one with `canProcessReplacing()`.
- **Problem:** `process()` is commented out. So `procHandler`, the `procHandlers[]` table and the 8 functions `proc1In1Out` through `proc2In2OutB` never run. A grep shows the only use of `procHandler` is inside that comment.
- **Fix:** Delete them, about −65 lines. A VST 2.4 host calls `processReplacing`. A very old host that calls `process` would get silence either way.
- **If we skip it:** Readers have to understand twice as much audio code.

**11. Checks in `resume()` that can never fire** (`WavePlug.cpp:526-557`)
- **What this is:** `resume()` is meant to refuse to run if no input or output is connected.
- **Problem:** Lines 534-537 set all four connection flags to true, and the real queries are commented out. So `noInputs` and `noOutputs` are never true, and the three `throw`s and `operational = false` cannot run. Because the flags are always true after `resume()`, only the stereo-in/stereo-out pair (`procR2In2Out`, `procR2In2OutB`) is ever chosen. The other six variants run only if a host skips `resume()`.
- **Fix:** Delete the unreachable branches, about −20 lines. Then decide whether to drop the six unused variants or restore the real connection queries.
- **If we skip it:** The error handling looks real but does nothing.

**12. Commented-out and never-called code** (several files)
- **What this is:** Code that is switched off or that nothing calls.
- **Problem:** About 85 lines are comments or `#if` blocks that never compile:
  - `WavePlug.cpp:35-45` and `:559-564`
  - `wpfunc.cpp:30-45` and `wpfunc.hpp:38-42`
  - `WavePlugEditor.cpp:412-428` and `:470-490`
  - the `#if GDIPLUS` blocks at `:221` and `:244`, where the macro is never defined anywhere
  - the `WP_OLD_WINDOWS` branch at `BufferManager.cpp:28-30`
  
  Another 35 lines are getters and setters that nothing calls, for example 12 getters at `Analyzer.hpp:60-71` and `getBufferSizeMultiplier`. `modulo()` is used only by its own test. `Analyzer::oldWaveSize` is written and never read.
- **Fix:** Delete all of it, about −120 lines. I grepped the whole tree first.
- **If we skip it:** More code to read, with nothing gained.

## Nice to have

**13. One display string is 9 characters long** (`WavePlug.cpp:219-222`)
- **What this is:** Each knob shows its value as text, and VST allows 8 characters.
- **Problem:** I called every display function across the knob range. Only Freq Offset (both channels) overshoots, with "−22000.00" at the knob minimum. A host that allocates just the 8 characters VST allows overflows by a byte or two. Most hosts use bigger buffers.
- **Fix:** Use `"%1.1f"` for that display, which gives "−22000.0".
- **If we skip it:** A small overflow risk in strict hosts.

**14. The CMake fallback for other platforms cannot build** (`CMakeLists.txt:80-81`)
- **What this is:** The `else()` branch builds the no-GUI plug-in for anything that is not Windows or Mac.
- **Problem:** The SDK header defines `VSTCALLBACK` as `__cdecl` under GCC (`aeffect.h:39`), so it fails with compile errors on Linux. I hit those errors. The README lists only Windows and macOS.
- **Fix:** Delete the two lines.
- **If we skip it:** A branch that looks supported and is not.

**15. CMake 4.2 is required more broadly than needed** (`CMakeLists.txt:1`, `CMakePresets.json:3-6`)
- **What this is:** The project refuses to configure on any CMake older than 4.2.
- **Problem:** I tried CMake 3.28. As shipped it refuses to configure. With the minimum lowered to 3.20 it configures fine. Only the Visual Studio 2026 generator in the Windows preset needs 4.2.
- **Fix:** Set `cmake_minimum_required(VERSION 3.20)` and keep 4.2 only for the Windows preset.
- **If we skip it:** People with older CMake are blocked for no reason.

**16. The most extreme patch is CPU heavy** (`Synthesizer.cpp:189-199`, `wpmodulators.cpp:299-316`)
- **What this is:** Oversampling and the "Conv" wave mode multiply each other.
- **Problem:** Each output sample can need up to 16 oversampling steps × 30 filter taps × 2 lookups. I measured the worst patch (maximum Oversampling, Conv at 100% mix) on this machine:

  | Sample rate | Load, worst patch | Load, default patch |
  |---|---|---|
  | 44.1 kHz | 37% of one core | not measured |
  | 96 kHz | 80% of one core | not measured |
  | 192 kHz | 168% of one core | 1% of one core |

- **Fix:** Nothing is needed unless users hit it. If they do, cap oversampling at high rates, for example by scaling `OVER_T` by 44100 divided by the sample rate.
- **If we skip it:** That patch cannot run in real time at 192 kHz.

I left out 4 smaller points: the version number is repeated in three places (CMake, `WavePlug.hpp:36-44`, README), exceptions are caught by value, `Synthesizer::reset()` clears playback state whenever the Smoothing knob moves, and the dummy "Version" parameter.

**Verdict:** Fix 1, 2 and 3 first. Each is a few lines and each one is a crash or data-loss path. Then do 4 and the tests in 7. After that, finding 9 is the big clean-up.

**Lean:** −210 lines without finding 9, about −460 with it. No dependencies to drop.

**Not checked:**
- The GUI editor code beyond reading it, the Windows resource file, and the vendored SDK and VSTGUI sources.
- The Windows and macOS builds. All my runs used g++ on Linux, with the one `-D__cdecl=` workaround.
- How the plug-in sounds.
- None of the fixes were applied or tested.
- The CPU numbers come from this sandbox machine.

Sources: [tapester/RenewLostTech](https://github.com/tapester/RenewLostTech)