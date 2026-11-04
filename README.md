# Lost Technology

### Project background

This project contains GUI and no-GUI variants of the Lost Technology VST 2 plug-in. It uses the VST 2.4 API and the bundled VSTGUI source.

The current plug-in version is **0.2.6**. CMake defines the version used by both plug-in builds, and the GUI displays the same version in its top bar.

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

### Testing on Windows (64-bit) using Pluginval

Before testing, make sure that all builds are ready:

```powershell
cmake --build build-win64 --config Debug
cmake --build build-win64 --config Release
```

Running them back to back. pluginval is a GUI-subsystem exe, so PowerShell returns to the prompt immediately, as it did in your last log. Piping to Out-Null makes PowerShell wait for it. Have pluginval write a log file for each run so nothing is lost:

```powershell
New-Item -ItemType Directory -Force logs | Out-Null
foreach ($cfg in "Debug","Release") {
  foreach ($dll in "LostTech","LostTechNoGUI") {
    .\tools\pluginval\pluginval.exe --strictness-level 10 --repeat 3 --randomise `
      --random-seed 0x44c4e05 --output-dir logs --output-filename "$cfg-$dll.txt" `
      "build-win64\$cfg\$dll.dll" | Out-Null
    "$cfg $dll -> exit code $LASTEXITCODE"
  }
}
```

An exit code of 0 means all tests passed and 1 means at least one failed. Check each log for lines starting with !!! to see which tests failed. I haven’t run this script, so check the log file names and that $LASTEXITCODE comes through on your setup.