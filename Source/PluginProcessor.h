#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
struct DelexPreset
{
    juce::String name;
    float time;
    float feedback;
    float mix;
    float spread;
    float hicut;
    float locut;
    float eqLow;
    float eqMid;
    float eqHigh;
    float modRate;
    float modDepth;
    float drive;
    float lofi;
    bool  pingpong;
};

//==============================================================================
class DelexAudioProcessor : public juce::AudioProcessor
{
public:
    DelexAudioProcessor();
    ~DelexAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float> outputLevelL { 0.0f };
    std::atomic<float> outputLevelR { 0.0f };

    static const std::vector<DelexPreset>& getFactoryPresets();
    void applyPreset (int index);

    bool isUnlocked();
    bool tryUnlock (const juce::String& serial);
    static bool isValidSerial (const juce::String& serial);

    void registerTap();
    std::atomic<float> currentBPM { 120.0f };

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 192000 };

    juce::dsp::StateVariableTPTFilter<float> hiCutFilter;
    juce::dsp::StateVariableTPTFilter<float> loCutFilter;

    juce::dsp::IIR::Filter<float> eqLowL, eqLowR;
    juce::dsp::IIR::Filter<float> eqMidL, eqMidR;
    juce::dsp::IIR::Filter<float> eqHighL, eqHighR;

    float lfoPhase = 0.0f;

    double currentSampleRate = 44100.0;

    void updateEQ();
    float saturate (float x, float drive);
    float applyLofi (float x, float amount, int channel);

    float lofiHoldL = 0.0f, lofiHoldR = 0.0f;
    int lofiCounterL = 0, lofiCounterR = 0;

    juce::int64 lastTapTime = 0;

    juce::File getLicenseFile() const;
    bool unlockedState = false;
    bool unlockChecked = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelexAudioProcessor)
};
