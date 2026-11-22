# JUCE build of Lost Technology (VST3 and CLAP). Off by default: configure with
# -DLOSTTECH_JUCE=ON. Needs CMake 3.22 or newer (JUCE's requirement).
#
# JUCE is AGPLv3 or commercial. Binaries built here are AGPL unless you hold a JUCE licence.
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.6
    GIT_SHALLOW ON)
# JUCE 9 has no CLAP format; clap-juce-extensions adds it to a JUCE plug-in target.
# shortcut: pinned to a commit of its main branch; switch to a release tag when one fits JUCE 9.
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 3837f4a6ed2ab0ba46aba69ae5b7041c7c64ce74)
FetchContent_MakeAvailable(JUCE clap-juce-extensions)

# PLUGIN_CODE keeps the old VST2 unique ID ('CQCQ').
juce_add_plugin(LostTechJUCE
    PRODUCT_NAME "Lost Technology"
    COMPANY_NAME "Transvaal Audio"
    BUNDLE_ID com.transvaalaudio.losttech
    PLUGIN_MANUFACTURER_CODE Trva
    PLUGIN_CODE CQCQ
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VERSION ${PROJECT_VERSION}
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LostTechJUCE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(LostTechJUCE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=1
    # A VST2 version was released and no VST3 yet: keep parameter IDs equal to the VST2 indexes.
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1)

target_link_libraries(LostTechJUCE
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET LostTechJUCE
    CLAP_ID "com.transvaalaudio.losttech"
    CLAP_FEATURES audio-effect)
