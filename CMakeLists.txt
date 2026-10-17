cmake_minimum_required(VERSION 3.22)
project(DelexByPiradex VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Compatível com Macs antigos (High Sierra 10.13 / Catalina 10.15, Intel) e novos (Apple Silicon)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS mínimo")

# Windows: runtime do Visual C++ embutido no plugin (não precisa do "VC++ Redistributable")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: usa uma cópia local (-DJUCE_PATH=...) ou descarrega a versão fixa
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(DLX_FORMATS VST3 AU AAX)
elseif(WIN32)
    set(DLX_FORMATS VST3 AAX)
else()
    set(DLX_FORMATS VST3)            # Linux: só desenvolvimento/testes
endif()

juce_add_plugin(Delex
    COMPANY_NAME "Piradex"
    COMPANY_WEBSITE "https://www.beatfreakstudio.com"
    COMPANY_EMAIL "juninhopiradex@hotmail.com"
    BUNDLE_ID "com.piradex.delex2"
    PLUGIN_MANUFACTURER_CODE Prdx
    PLUGIN_CODE PxDX
    FORMATS ${DLX_FORMATS}
    PRODUCT_NAME "DELEX by Piradex"
    DESCRIPTION "Delay espacial estereo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY Delay)   # AAX: SDK incluído na JUCE 8; o Pro Tools normal exige assinatura PACE

juce_add_binary_data(DelexAssets SOURCES
    Resources/BarlowSemiCondensed-Medium.ttf
    Resources/BarlowSemiCondensed-SemiBold.ttf
    Resources/BarlowSemiCondensed-Bold.ttf
    Resources/JetBrainsMono-Medium.ttf)

target_sources(Delex PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/License.cpp
    Source/i18n/PiradexLanguage.cpp)

target_include_directories(Delex PRIVATE Source)

# Fontes em UTF-8 (acentos nas etiquetas) também no Visual Studio
if(MSVC)
    target_compile_options(Delex PRIVATE /utf-8 /bigobj)
endif()

target_compile_definitions(Delex PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Delex
    PRIVATE
        DelexAssets
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Testes (opcional): cmake -DDLX_BUILD_TESTS=ON
option(DLX_BUILD_TESTS "Compilar testes" OFF)
if(DLX_BUILD_TESTS)
    add_executable(delex_core_test tests/test_core.cpp)
    target_include_directories(delex_core_test PRIVATE Source)

    target_compile_definitions(Delex PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1 DLX_ALLOW_TEST_UNLOCK=1)
    add_executable(delex_plugin_test tests/test_plugin.cpp)
    target_include_directories(delex_plugin_test PRIVATE Source $<TARGET_PROPERTY:Delex,INCLUDE_DIRECTORIES>)
    target_compile_definitions(delex_plugin_test PRIVATE $<TARGET_PROPERTY:Delex,COMPILE_DEFINITIONS>)
    target_link_libraries(delex_plugin_test PRIVATE Delex)
endif()
