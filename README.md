# Lost Technology

### Project background

This project contains GUI and no-GUI variants of the Lost Technology VST 2 plug-in. It uses the VST 2.4 API and the bundled VSTGUI source.

The supported build is CMake. The old MinGW makefile is retained under `legacy/` for historical reference and is not maintained. The checked-in CMake presets require CMake 3.20 or newer.

### Source layout

- `src/plugin/` contains the VST effect, parameter definitions, and plug-in entry points.
- `src/dsp/` contains the analyzer, synthesizer, modulation, buffer, and signal utility code.
- `src/ui/` contains the VSTGUI editor and Windows resource script.
- `src/common/` contains headers shared across those layers.
- `resources/` and `docs/` contain UI assets and user-facing documentation.
- `dependencies/vstsdk2.4/` contains the bundled VST 2.4 SDK and VSTGUI sources; see [THIRD_PARTY.md](THIRD_PARTY.md).

### Building on Windows (64-bit)

The VST 2.4 SDK and VSTGUI sources must be present in `dependencies/vstsdk2.4`. From an x64 Visual Studio developer shell with CMake available, configure and build both plug-in variants with:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release
```

To build the GUI plug-in with debug symbols and no optimization, use:

```powershell
cmake --build --preset windows-x64-debug-gui
```

This writes `build-win64\Debug\LostTech.dll` with the Visual Studio generator.

The GUI plug-in is written to `build-win64\Release\LostTech.dll`; the alternative without a GUI is `build-win64\Release\LostTechNoGUI.dll`. These are 64-bit VST 2 plug-ins.

The Visual Studio generator stores configuration-specific binaries under `Release`. Other CMake generators may use a different output location.

Run the focused DSP utility tests with:

```powershell
ctest --test-dir build-win64 -C Release --output-on-failure
```
