# Third-party components

## VST 2.4 SDK

The sources under `dependencies/vstsdk2.4/public.sdk/source/vst2.x/` identify themselves as the VST Plug-Ins SDK 2.4 and carry 2006 Steinberg Media Technologies copyright notices. No separate SDK license or provenance record is included in the bundled files, so the exact upstream package and its redistribution terms have not been independently established here. Preserve the upstream notices and confirm the applicable license before redistributing the SDK or binaries that include it.

## VSTGUI

The sources under `dependencies/vstsdk2.4/vstgui.sf/vstgui/` identify themselves as VSTGUI 3.5 and carry a 2008 Steinberg Media Technologies copyright notice. The source headers include VSTGUI's redistribution terms: retain the copyright, conditions, and disclaimer in source redistributions; reproduce them with binary redistributions; and do not use the copyright holder's or contributors' names for endorsement without permission. The license also includes warranty and liability disclaimers. See the upstream notices in `aeffguieditor.h` and `vstgui.h`.

The bundled source tree does not record an upstream commit or package checksum. Keep its original notices intact and record the precise source and license if replacing or refreshing this legacy dependency.

## pluginval (development tool, not part of the plug-in)

pluginval is Tracktion's plug-in validation tool. It is used only to validate the built `LostTech.dll` and `LostTechNoGUI.dll`, from the command line, from the Visual Studio debugger (F5 on the `LostTech` project), and through the `pluginval_*` CTest entries defined in `CMakeLists.txt`.

- Upstream: <https://github.com/Tracktion/pluginval>, released under the GNU General Public License v3 (GPLv3), according to its README.
- Binaries: download a release build from <https://github.com/Tracktion/pluginval/releases>. The project's README states the pre-compiled releases include VST2 testing support. Place `pluginval.exe` at `tools/pluginval/pluginval.exe`, or point CMake at another location with `-DPLUGINVAL_EXE=<path>`.
- Version used: record the output of `.\tools\pluginval\pluginval.exe --version` here when updating the tool.
- Scope: pluginval runs as a separate process, is not linked into the plug-in, and is not shipped with it. `/tools/pluginval/` is listed in `.gitignore`, so the binary is not meant to be committed.
- If a pluginval binary is ever redistributed (for example attached to a release), the GPLv3 terms apply to that distribution: include the license text and make the corresponding source available, for example by referencing the exact upstream release.