cmake_minimum_required(VERSION 3.18)
project(dwm-ma-plugin VERSION 0.0.1 LANGUAGES CXX C)

add_subdirectory(dwm-ma)
# Override default dwm-ma parameters here (check dwm-ma/dwm_ma.h for more details)
target_compile_definitions(dwm-ma PUBLIC DWM_MA_MAX_INPUT_COUNT=4)

add_subdirectory(JUCE)

juce_add_plugin(dwm-ma-plugin
        COMPANY_NAME "JoySpuriForotti"
        PRODUCT_NAME "Digital Waveguide Mesh with Microphone Array output"
        PLUGIN_MANUFACTURER_CODE JSFX
        PLUGIN_CODE DWMA
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        FORMATS VST3 LV2
        LV2URI "https://github.com/J0ySF/dwm-ma-plugin"
        COPY_PLUGIN_AFTER_BUILD TRUE)
target_compile_definitions(dwm-ma-plugin
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
target_sources(dwm-ma-plugin
        PRIVATE
        plugin.cpp)
set_property(TARGET dwm-ma-plugin PROPERTY CXX_STANDARD 11)
target_link_libraries(dwm-ma-plugin PUBLIC
        dwm-ma
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)