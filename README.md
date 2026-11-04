# Lost Technology

Lost Technology is a VST 2.4 audio plug-in with GUI and no-GUI builds. It uses the bundled Steinberg VST 2.4 SDK and VSTGUI sources in `dependencies/vstsdk2.4/`.

The current plug-in version is **0.2.6**. CMake is the supported build system; the legacy MinGW makefile is retained for reference and is not maintained.

## Build on Windows

Requirements:

- CMake 4.2 or newer (the Visual Studio 2026 generator was added in CMake 4.2)
- Visual Studio 2026 with the x64 C++ build tools
- The bundled SDK and VSTGUI sources in `dependencies/vstsdk2.4/`

From a Visual Studio developer PowerShell, configure and build both release plug-ins:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release
```

The DLLs are written to `build-win64\Release\`:

- `LostTech.dll` — GUI plug-in
- `LostTechNoGUI.dll` — no-GUI plug-in

Build the GUI plug-in in Debug configuration with:

```powershell
cmake --build --preset windows-x64-debug-gui
```

This produces `build-win64\Debug\LostTech.dll`.

## Tests and plug-in validation

Run the DSP tests after configuring and building:

```powershell
ctest --test-dir build-win64 -C Release --output-on-failure
```

Optional plug-in validation uses Tracktion's pluginval. The executable is not committed; download a release and place `pluginval.exe` in `tools/pluginval/`, or configure CMake with `-DPLUGINVAL_EXE=<path>`. CMake enables `validate_losttech` and `validate_losttechnogui` build targets and corresponding CTest entries when the executable is present.

Run both validators through CTest:

```powershell
ctest --test-dir build-win64 -C Release --output-on-failure -R '^pluginval_'
```

Or validate one plug-in directly:

```powershell
cmake --build build-win64 --config Release --target validate_losttech
cmake --build build-win64 --config Release --target validate_losttechnogui
```

The validation defaults are strictness level 10, three repetitions, and a fixed random seed. Override them at configure time with `PLUGINVAL_STRICTNESS`, `PLUGINVAL_REPEAT`, `PLUGINVAL_SEED`, or `PLUGINVAL_TIMEOUT`. The GUI validation opens the editor and requires an interactive desktop session.

See [THIRD_PARTY.md](THIRD_PARTY.md) for bundled dependency and pluginval licensing notes.
