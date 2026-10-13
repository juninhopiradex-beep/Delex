#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "License.h"

class DelexProcessor : public juce::AudioProcessor
{
public:
    DelexProcessor();
    ~DelexProcessor() override = default;

    // --- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return (int) delex::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (delex::id::bypass); }

    // --- estado partilhado com a interface
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    delex::RawParams raw { apvts };

    // presets
    void loadFactoryPreset (int index);
    void loadUserPreset (const juce::File& f);
    bool saveUserPreset (const juce::String& name);
    juce::File userPresetFolder() const;
    juce::String currentPresetName() const { return presetName; }
    bool isFavourite (const juce::String& name) const;
    void toggleFavourite (const juce::String& name);
    void applyStyle (int index);

    // A/B
    void selectSlot (int slot);       // 0 = A, 1 = B
    int  currentSlot() const { return slotIndex; }
    void copyToOtherSlot();

    // tap tempo (chamado pela interface)
    void tap();
    double tappedBpm() const { return tapBpm; }

    // medidores (lidos pela interface)
    std::atomic<float> inPeak[2] {}, outPeak[2] {}, duckGr { 0 }, correlation { 0 }, loopLimit { 0 };
    std::atomic<double> hostBpm { 120.0 }, currentSampleRate { 48000.0 };
    std::atomic<bool> hostHasBpm { false };

    // goniómetro: pares (L, R) do sinal de saída
    static constexpr int kScopeSize = 2048;
    juce::AbstractFifo scopeFifo { kScopeSize };
    std::array<float, kScopeSize * 2> scopeData {};

    // escala da interface (guardada no estado)
    float uiScale = 1.0f;
    bool  advancedView = false;
    int   advancedTab = 2;

    static constexpr int kStateVersion = 2;

private:
    template <typename Sample> void processAny (juce::AudioBuffer<Sample>&);
    void pushScope (const float* l, const float* r, int n);

    delex::Engine engine;
    juce::AudioBuffer<float> work;
    int currentPreset = 0;
    juce::String presetName { "Init" };

    juce::ValueTree slots[2];
    int slotIndex = 0;

    double tapBpm = 0.0;
    juce::int64 lastTapMs = 0;
    juce::Array<double> tapIntervals;

    std::unique_ptr<juce::PropertiesFile> settings;
    float corrXY = 0, corrXX = 0, corrYY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelexProcessor)
};
