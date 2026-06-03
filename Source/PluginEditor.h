#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class DelexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DelexLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
class DelexAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit DelexAudioProcessorEditor (DelexAudioProcessor&);
    ~DelexAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DelexAudioProcessor& audioProcessor;
    DelexLookAndFeel lookAndFeel;

    const juce::Colour colBg       { 0xFF06060E };
    const juce::Colour colCard     { 0xFF0B0B16 };
    const juce::Colour colCardLine { 0xFF16263A };
    const juce::Colour colGreen    { 0xFF00FFB2 };
    const juce::Colour colWhite    { 0xFFE8E8F0 };
    const juce::Colour colTeal     { 0xFF7FB5A8 };

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob timeKnob, feedbackKnob, mixKnob, spreadKnob;
    Knob hicutKnob, locutKnob;
    Knob eqLowKnob, eqMidKnob, eqHighKnob;
    Knob modRateKnob, modDepthKnob;
    Knob driveKnob, lofiKnob;

    std::unique_ptr<juce::TextButton> pingpongButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingpongAttachment;

    std::unique_ptr<juce::TextButton> modWaveButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modWaveAttachment;

    std::unique_ptr<juce::TextButton> tapButton;

    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;

    std::unique_ptr<juce::TextButton> aboutButton;
    bool showAbout = false;

    bool unlocked = false;
    std::unique_ptr<juce::TextEditor> serialInput;
    std::unique_ptr<juce::TextButton> activateButton;
    std::unique_ptr<juce::Label> serialMessage;

    void setupKnob (Knob& knob, const juce::String& paramID, const juce::String& labelText);
    void buildMainUI();
    void buildSerialUI();
    void attemptActivation();
    void paintAbout (juce::Graphics& g);
    void setControlsVisible (bool shouldBeVisible);
    void drawCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title);

    float meterL = 0.0f, meterR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelexAudioProcessorEditor)
};
