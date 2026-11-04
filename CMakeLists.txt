cmake_minimum_required(VERSION 3.10)
project(LostTech VERSION 0.2.6)

# ---------------------------------------------------------------------------
# Build the bundled VST 2.4 SDK and VSTGUI libraries before the plug-ins.
# ---------------------------------------------------------------------------
add_subdirectory(dependencies/vstsdk2.4/public.sdk)
add_subdirectory(dependencies/vstsdk2.4/vstgui.sf)

# ---------------------------------------------------------------------------
# DSP implementation shared by the GUI and no-GUI plug-ins.
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Common plug-in headers and parameter definitions.
# ---------------------------------------------------------------------------
set(PLUGIN_COMMON_FILES
        src/common/wpstdinclude.h
        src/plugin/WavePlug.hpp
        src/plugin/WavePlugParameters.cpp
        src/plugin/waveplugparams.h
        )

# ---------------------------------------------------------------------------
# GUI plug-in sources, including the VSTGUI editor and Windows resources.
# ---------------------------------------------------------------------------
set(GUI_SOURCE_FILES
        ${DSP_SOURCE_FILES}
        ${PLUGIN_COMMON_FILES}
        src/plugin/WavePlug.cpp
        src/plugin/WavePlugMain.cpp
        src/ui/WavePlugEditor.cpp
        src/ui/WavePlugEditor.hpp
        src/ui/WavePlugResource.rc
        )

# ---------------------------------------------------------------------------
# The no-GUI variant reuses WavePlug.cpp through a wrapper that defines WP_NO_GUI.
# ---------------------------------------------------------------------------
set(NOGUI_SOURCE_FILES
        ${DSP_SOURCE_FILES}
        ${PLUGIN_COMMON_FILES}
        src/plugin/WavePlugMainNoGUI.cpp
        src/plugin/WavePlugNoGUI.cpp
        )

# ---------------------------------------------------------------------------
# Build both plug-in variants as shared libraries.
# ---------------------------------------------------------------------------
add_library(LostTech SHARED ${GUI_SOURCE_FILES} LostTech.def)
add_library(LostTechNoGUI SHARED ${NOGUI_SOURCE_FILES} LostTech.def)

# ---------------------------------------------------------------------------
# Convenience target used with the Debug configuration to build the GUI plug-in.
# ---------------------------------------------------------------------------
add_custom_target(LostTechDebug DEPENDS LostTech)

# Apply common Windows compatibility and CRT settings to both plug-ins.
set(PLUGIN_VERSION_DEFINITIONS
    WP_MAJOR=${PROJECT_VERSION_MAJOR}
    WP_MINOR=${PROJECT_VERSION_MINOR}
    WP_UPDATE=${PROJECT_VERSION_PATCH}
)
target_compile_definitions(LostTech PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX ${PLUGIN_VERSION_DEFINITIONS})
target_compile_definitions(LostTechNoGUI PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX ${PLUGIN_VERSION_DEFINITIONS})

# ---------------------------------------------------------------------------
# Include paths shared by the plug-in source files.
# ---------------------------------------------------------------------------
set(PLUGIN_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        src/common
        src/dsp
        src/plugin
        src/ui
        )
target_include_directories(LostTech PRIVATE ${PLUGIN_INCLUDE_DIRS})
target_include_directories(LostTechNoGUI PRIVATE ${PLUGIN_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Link each variant to the SDK libraries it uses.
# ---------------------------------------------------------------------------
target_link_libraries(LostTech PRIVATE VSTSDK2_4 vstgui)
target_link_libraries(LostTechNoGUI PRIVATE VSTSDK2_4)

if(WIN32)
    # Match VSTGUI's minimum Windows API and link GUI system libraries.
    target_compile_definitions(LostTech PRIVATE WINVER=0x0501 _WIN32_WINNT=0x0501)
    target_compile_definitions(LostTechNoGUI PRIVATE WINVER=0x0501 _WIN32_WINNT=0x0501)
    target_link_libraries(LostTech PRIVATE gdi32 ole32 comdlg32 uuid)
endif()

# ---------------------------------------------------------------------------
# Keep exported symbols hidden and emit the plug-in DLL names without a lib prefix.
# ---------------------------------------------------------------------------
SET_TARGET_PROPERTIES(LostTech PROPERTIES CXX_VISIBILITY_PRESET hidden)
SET_TARGET_PROPERTIES(LostTechNoGUI PROPERTIES CXX_VISIBILITY_PRESET hidden)

SET_TARGET_PROPERTIES(LostTech PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(LostTechNoGUI PROPERTIES PREFIX "")

# ---------------------------------------------------------------------------
# Register focused DSP tests when CTest support is enabled.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(wpfunc_tests tests/dsp/wpfunc_tests.cpp src/dsp/wpfunc.cpp)
    target_include_directories(wpfunc_tests PRIVATE src/dsp)
    add_test(NAME wpfunc_tests COMMAND wpfunc_tests)
endif()

# ---------------------------------------------------------------------------
# pluginval (validation / debugging). Optional: only active on Windows if the exe exists.
# Adds CTest entries for the GUI and no-GUI plug-ins:
#   ctest --test-dir build-win64 -C Release --output-on-failure -R pluginval
# ---------------------------------------------------------------------------
set(PLUGINVAL_EXE "${CMAKE_CURRENT_SOURCE_DIR}/tools/pluginval/pluginval.exe"
    CACHE FILEPATH "Path to pluginval.exe")
set(PLUGINVAL_STRICTNESS 10 CACHE STRING "pluginval strictness level (1-10)")
set(PLUGINVAL_REPEAT 3 CACHE STRING "Number of times pluginval repeats the tests")
set(PLUGINVAL_SEED "0x44c4e05" CACHE STRING "Fixed pluginval random seed for reproducible runs (empty = random)")
set(PLUGINVAL_TIMEOUT 900 CACHE STRING "CTest timeout per pluginval test, in seconds")

if(WIN32 AND EXISTS "${PLUGINVAL_EXE}")
    set(PLUGINVAL_ARGS
        --strictness-level ${PLUGINVAL_STRICTNESS}
        --repeat ${PLUGINVAL_REPEAT}
        --randomise)
    if(PLUGINVAL_SEED)
        list(APPEND PLUGINVAL_ARGS --random-seed ${PLUGINVAL_SEED})
    endif()

    # F5 in Visual Studio on the LostTech project launches pluginval with the DLL loaded,
    # so the debugger stops on a crash with LostTech.pdb available.
    # (Only honoured by the Visual Studio generators.)
    string(REPLACE ";" " " PLUGINVAL_ARGS_STR "${PLUGINVAL_ARGS}")
    set_target_properties(LostTech PROPERTIES
        VS_DEBUGGER_COMMAND           "${PLUGINVAL_EXE}"
        VS_DEBUGGER_COMMAND_ARGUMENTS "${PLUGINVAL_ARGS_STR} --verbose \"$<TARGET_FILE:LostTech>\""
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:LostTech>")

    # One build target and one CTest entry per plug-in variant.
    # Command line: cmake --build build-win64 --config Release --target validate_losttech
    foreach(PV_TARGET LostTech LostTechNoGUI)
        string(TOLOWER ${PV_TARGET} PV_NAME)

        add_custom_target(validate_${PV_NAME}
            COMMAND "${PLUGINVAL_EXE}" ${PLUGINVAL_ARGS} "$<TARGET_FILE:${PV_TARGET}>"
            DEPENDS ${PV_TARGET}
            USES_TERMINAL
            COMMENT "Validating ${PV_TARGET} with pluginval (strictness ${PLUGINVAL_STRICTNESS})")

        # The GUI variant opens its editor, so CTest needs an interactive desktop session.
        if(BUILD_TESTING)
            add_test(NAME pluginval_${PV_NAME}
                     COMMAND "${PLUGINVAL_EXE}" ${PLUGINVAL_ARGS} "$<TARGET_FILE:${PV_TARGET}>")
            set_tests_properties(pluginval_${PV_NAME} PROPERTIES TIMEOUT ${PLUGINVAL_TIMEOUT})
        endif()
    endforeach()
endif()