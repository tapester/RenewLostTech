# Lost Technology

Lost Technology is a VST 2.4 audio plug-in with GUI and no-GUI builds. It uses the bundled Steinberg VST 2.4 SDK and VSTGUI sources in `dependencies/vstsdk2.4/`.

The current plug-in version is **0.2.6**. CMake 4.2 or newer is required; CMake is the supported build system, while the legacy MinGW makefile is retained for reference and is not maintained.

## Platform support

| Platform | GUI plug-in (`LostTech`) | No-GUI plug-in (`LostTechNoGUI`) |
|---|---|---|
| Windows x64 | Yes (`.dll`) | Yes (`.dll`) |
| macOS (Apple Silicon and Intel) | No | Yes (`.vst` bundle, universal binary) |

The GUI plug-in is Windows-only because VSTGUI 3.5 relies on Carbon on macOS, which does not exist on arm64. Supporting it there requires a migration to VSTGUI 4.x (Cocoa).

For the parameter set and its recommended CLAP mapping, see [parameters.md](parameters.md).

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

The release preset also builds the DSP test executable `wpfunc_tests`.

The DLLs are written to `build-win64\Release\`:

- `LostTech.dll` — GUI plug-in
- `LostTechNoGUI.dll` — no-GUI plug-in

Build the GUI plug-in in Debug configuration with:

```powershell
cmake --build --preset windows-x64-debug-gui
```

This produces `build-win64\Debug\LostTech.dll`.

## Build on macOS

Only the no-GUI plug-in is built on macOS. It is produced as a `LostTechNoGUI.vst` bundle containing a universal binary (arm64 and x86_64), with the entry points `VSTPluginMain` and `main_macho`.

Requirements: CMake 4.2 or newer and Xcode command line tools. On a Mac:

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal-release --parallel
ctest --test-dir build-macos --output-on-failure
```

Apple Silicon refuses unsigned code, so sign the bundle at least ad hoc:

```bash
codesign --force --deep --sign - build-macos/LostTechNoGUI.vst
```

Ad-hoc signing is sufficient for use on the Mac that built it. Distributing the plug-in to other Macs requires a Developer ID signature and notarization.

### Building without a Mac (GitHub Actions)

The workflow `.github/workflows/macos.yml` builds, tests, signs ad hoc, and packages the plug-in on a macOS runner for every push, pull request, and manual run. The artifact `LostTechNoGUI-macos` contains a DMG and a ZIP with the bundle.

### Installing

1. Download the artifact from the workflow run and open the DMG.
2. Copy `LostTechNoGUI.vst` to `/Library/Audio/Plug-Ins/VST/` (all users) or `~/Library/Audio/Plug-Ins/VST/` (current user).
3. If macOS blocks the plug-in because it was downloaded, remove the quarantine attribute:

   ```bash
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST/LostTechNoGUI.vst
   ```

4. Rescan plug-ins in your host.

Finder shows the bundle as a generic "Package" while it is inside the DMG and as "Bundle" once it is in one of the plug-in folders above. This is expected.

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

The pluginval targets and CTest entries are only available on Windows. On macOS, the workflow runs pluginval as an optional step that does not fail the build.

The validation defaults are strictness level 10, three repetitions, and a fixed random seed. Override them at configure time with `PLUGINVAL_STRICTNESS`, `PLUGINVAL_REPEAT`, `PLUGINVAL_SEED`, or `PLUGINVAL_TIMEOUT`. The GUI validation opens the editor and requires an interactive desktop session.

See [THIRD_PARTY.md](THIRD_PARTY.md) for bundled dependency and pluginval licensing notes.
