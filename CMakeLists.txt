cmake_minimum_required(VERSION 3.10)
project(LostTech)

# Build the bundled VST 2.4 SDK and VSTGUI libraries before the plug-ins.
add_subdirectory(dependencies/vstsdk2.4/public.sdk)
add_subdirectory(dependencies/vstsdk2.4/vstgui.sf)

# DSP implementation shared by the GUI and no-GUI plug-ins.
set(DSP_SOURCE_FILES
        src/dsp/Analyzer.cpp
        src/dsp/Analyzer.hpp
        src/dsp/BufferManager.cpp
        src/dsp/BufferManager.hpp
        src/dsp/Synthesizer.cpp
        src/dsp/Synthesizer.hpp
        src/dsp/wpfunc.cpp
        src/dsp/wpfunc.hpp
        src/dsp/wpmodulators.cpp
        src/dsp/wpmodulators.hpp
        )

# Common plug-in headers and parameter definitions.
set(PLUGIN_COMMON_FILES
        src/common/wpstdinclude.h
        src/plugin/WavePlug.hpp
        src/plugin/WavePlugParameters.cpp
        src/plugin/waveplugparams.h
        )

# GUI plug-in sources, including the VSTGUI editor and Windows resources.
set(GUI_SOURCE_FILES
        ${DSP_SOURCE_FILES}
        ${PLUGIN_COMMON_FILES}
        src/plugin/WavePlug.cpp
        src/plugin/WavePlugMain.cpp
        src/ui/WavePlugEditor.cpp
        src/ui/WavePlugEditor.hpp
        src/ui/WavePlugResource.rc
        )

# The no-GUI variant reuses WavePlug.cpp through a wrapper that defines WP_NO_GUI.
set(NOGUI_SOURCE_FILES
        ${DSP_SOURCE_FILES}
        ${PLUGIN_COMMON_FILES}
        src/plugin/WavePlugMainNoGUI.cpp
        src/plugin/WavePlugNoGUI.cpp
        )

# Build both plug-in variants as shared libraries.
add_library(LostTech SHARED ${GUI_SOURCE_FILES} LostTech.def)
add_library(LostTechNoGUI SHARED ${NOGUI_SOURCE_FILES} LostTech.def)

# Convenience target used with the Debug configuration to build the GUI plug-in.
add_custom_target(LostTechDebug DEPENDS LostTech)

# Apply common Windows compatibility and CRT settings to both plug-ins.
target_compile_definitions(LostTech PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
target_compile_definitions(LostTechNoGUI PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)

# Include paths shared by the plug-in source files.
set(PLUGIN_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        src/common
        src/dsp
        src/plugin
        src/ui
        )
target_include_directories(LostTech PRIVATE ${PLUGIN_INCLUDE_DIRS})
target_include_directories(LostTechNoGUI PRIVATE ${PLUGIN_INCLUDE_DIRS})

# Link each variant to the SDK libraries it uses.
target_link_libraries(LostTech PRIVATE VSTSDK2_4 vstgui)
target_link_libraries(LostTechNoGUI PRIVATE VSTSDK2_4)

if(WIN32)
    # Match VSTGUI's minimum Windows API and link GUI system libraries.
    target_compile_definitions(LostTech PRIVATE WINVER=0x0501 _WIN32_WINNT=0x0501)
    target_compile_definitions(LostTechNoGUI PRIVATE WINVER=0x0501 _WIN32_WINNT=0x0501)
    target_link_libraries(LostTech PRIVATE gdi32 ole32 comdlg32 uuid)
endif()

# Keep exported symbols hidden and emit the plug-in DLL names without a lib prefix.
SET_TARGET_PROPERTIES(LostTech PROPERTIES CXX_VISIBILITY_PRESET hidden)
SET_TARGET_PROPERTIES(LostTechNoGUI PROPERTIES CXX_VISIBILITY_PRESET hidden)

SET_TARGET_PROPERTIES(LostTech PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(LostTechNoGUI PROPERTIES PREFIX "")

# Register focused DSP tests when CTest support is enabled.
include(CTest)
if(BUILD_TESTING)
    add_executable(wpfunc_tests tests/dsp/wpfunc_tests.cpp src/dsp/wpfunc.cpp)
    target_include_directories(wpfunc_tests PRIVATE src/dsp)
    add_test(NAME wpfunc_tests COMMAND wpfunc_tests)
endif()
