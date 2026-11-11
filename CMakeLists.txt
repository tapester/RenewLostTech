cmake_minimum_required(VERSION 4.2)
project(LostTech VERSION 0.2.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build the bundled VST 2.4 SDK and VSTGUI libraries before the plug-ins.
# ---------------------------------------------------------------------------
# Static libraries linked into the plug-in must be position independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(dependencies/vstsdk2.4/public.sdk)

# The GUI plug-in is Windows-only: VSTGUI 3.5 relies on Carbon on macOS, which does
# not exist on arm64. On other platforms only the no-GUI variant is built.
if(WIN32)
    add_subdirectory(dependencies/vstsdk2.4/vstgui.sf)
endif()

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
if(WIN32)
    add_library(LostTech SHARED ${GUI_SOURCE_FILES} LostTech.def)
    add_library(LostTechNoGUI SHARED ${NOGUI_SOURCE_FILES} LostTech.def)
elseif(APPLE)
    # VST 2 on macOS is a bundle (.vst) containing a loadable module.
    add_library(LostTechNoGUI MODULE ${NOGUI_SOURCE_FILES})
else()
    add_library(LostTechNoGUI SHARED ${NOGUI_SOURCE_FILES})
endif()

# ---------------------------------------------------------------------------
# Convenience target used with the Debug configuration to build the GUI plug-in.
# ---------------------------------------------------------------------------
if(TARGET LostTech)
    add_custom_target(LostTechDebug DEPENDS LostTech)
endif()

# Plug-in targets that exist on this platform.
set(PLUGIN_TARGETS LostTechNoGUI)
if(TARGET LostTech)
    list(APPEND PLUGIN_TARGETS LostTech)
endif()

# Apply common Windows compatibility and CRT settings to both plug-ins.
set(PLUGIN_VERSION_DEFINITIONS
    WP_MAJOR=${PROJECT_VERSION_MAJOR}
    WP_MINOR=${PROJECT_VERSION_MINOR}
    WP_UPDATE=${PROJECT_VERSION_PATCH}
)
foreach(PLUGIN_TARGET ${PLUGIN_TARGETS})
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC ${PLUGIN_VERSION_DEFINITIONS})
    if(WIN32)
        target_compile_definitions(${PLUGIN_TARGET} PUBLIC
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WINVER=0x0601
            _WIN32_WINNT=0x0601)
    endif()
endforeach()

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
foreach(PLUGIN_TARGET ${PLUGIN_TARGETS})
    target_include_directories(${PLUGIN_TARGET} PRIVATE ${PLUGIN_INCLUDE_DIRS})
endforeach()

# ---------------------------------------------------------------------------
# Link each variant to the SDK libraries it uses.
# ---------------------------------------------------------------------------
if(TARGET LostTech)
    target_link_libraries(LostTech PRIVATE VSTSDK2_4 vstgui)
endif()
target_link_libraries(LostTechNoGUI PRIVATE VSTSDK2_4)

if(WIN32)
    # Link GUI system libraries.
    target_link_libraries(LostTech PRIVATE gdi32 ole32 comdlg32 uuid)
endif()

# ---------------------------------------------------------------------------
# Keep exported symbols hidden and emit the plug-in DLL names without a lib prefix.
# ---------------------------------------------------------------------------
foreach(PLUGIN_TARGET ${PLUGIN_TARGETS})
    set_target_properties(${PLUGIN_TARGET} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PREFIX "")
endforeach()

if(APPLE)
    # Configure with -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" for a universal binary.
    set_target_properties(LostTechNoGUI PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION vst
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.losttechnology.losttechnogui"
        MACOSX_BUNDLE_BUNDLE_NAME "LostTechNoGUI"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

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