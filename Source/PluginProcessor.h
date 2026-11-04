#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "License.h"
#include "Taps.h"
#include "Analysis.h"

class DelexProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DelexProcessor();
    ~DelexProcessor() override { stopTimer(); }

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
    bool acceptsMidi() const override { return true; }   // MIDI learn e pads de performance
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

    // MIDI learn (clique direito num controlo → aprender; mexe num controlador MIDI)
    void startMidiLearn (const juce::String& pid);
    void forgetMidi (const juce::String& pid);
    int  midiCcFor (const juce::String& pid) const;
    bool isLearning (const juce::String& pid) const;
    std::atomic<int> lastLearnedCc { -1 };

    // bloquear grupos de parâmetros ao trocar de preset (delex::LockGroup)
    int  getLockMask() const { return lockMask; }
    void setLockMask (int m);

    std::atomic<bool> sidechainActive { false };

    // pads de performance (0 Throw · 1 Freeze · 2 Spin · 3 Stop); MIDI: notas 36–39 (C1–D#1)
    enum Pad { PadThrow = 0, PadFreeze = 1, PadSpin = 2, PadStop = 3 };
    void setPad (int pad, bool down);
    bool padActive (int pad) const { return ((uiPads.load() | midiPads.load()) >> pad) & 1; }
    std::atomic<int> throwCounter { 0 };          // incrementa a cada throw (para a interface piscar)
    std::atomic<float> modSourceView[4] {};       // valor actual de cada fonte da matriz (interface)

    // morph A↔B: alvo = valores guardados no outro slot
    void refreshMorphTarget();
    // taps / grooves / harmonia (vaga C)
    delex::InputAnalyser analyser;
    void applyGroove (int index);                 // grooves() → padrão Multi-Tap
    void startGrooveCapture();                    // espera pelo primeiro som e grava 1 compasso
    enum Capture { CapIdle = 0, CapArmed = 1, CapRecording = 2, CapDone = 3 };
    std::atomic<int> captureState { CapIdle };
    int lastCaptureTaps = -1;                     // taps criados pela última captura (−1 = nenhuma)
    delex::taps::HarmonyInfo currentHarmony() const;
    void updateTapsNow() { timerCallback(); }     // usado pelos testes

    // valores efectivos do último pedaço de áudio (depois de morph, pads e matriz) — para testes/medição
    delex::Values lastChunkValues {};

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

    static constexpr int kStateVersion = 3;

private:
    template <typename Sample> void processAny (juce::AudioBuffer<Sample>&);
    void handleMidi (const juce::MidiBuffer&);
    void handleMidiNote (const juce::MidiMessage&);
    delex::Values valuesForChunk (double bpm, float inLevel, float keyLevel, int len);
    std::atomic<int> uiPads { 0 }, midiPads { 0 };
    // estado da matriz e dos pads (thread de áudio)
    float mmPhase = 0, mmRandom = 0, mmRandomS = 0, envIn = 0, envKey = 0, spinDeg = 0, timeFactor = 1;
    juce::Random mmRng;
    // morph
    juce::SpinLock morphLock;
    delex::Values morphShared {}, morphLocal {};
    std::atomic<int> morphVersion { 0 };
    int morphSeen = -1;
    bool morphHave = false, morphHaveShared = false;
    std::array<bool, (size_t) delex::Values::count()> continuous {}, morphable {};
    void timerCallback() override;
    // taps: padrão publicado para a thread de áudio
    juce::SpinLock tapLock;
    delex::TapSet tapShared {}, tapPublished {};
    std::atomic<int> tapVersion { 0 };
    int tapSeen = -1;
    // captura de groove (thread de áudio)
    std::array<double, 32> capOnsets {};
    std::atomic<int> capCount { 0 };
    double capSamples = 0, capLastOnset = -1.0e9;
    float capFast = 0, capSlow = 0;
    std::array<int, 32> modDestIdx {};
    static constexpr int kChunk = 32;
    std::array<std::atomic<int>, 128> ccMap {};
    std::atomic<int> learnTarget { -1 };
    int lockMask = 0;
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
