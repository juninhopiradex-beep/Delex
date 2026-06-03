#include "PluginEditor.h"

//==============================================================================
DelexLookAndFeel::DelexLookAndFeel() {}

void DelexLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider&)
{
    const juce::Colour green (0xFF00FFB2);
    const juce::Colour track (0xFF16263A);

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path backRing;
    backRing.addCentredArc (centreX, centreY, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (track);
    g.strokePath (backRing, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueRing;
    valueRing.addCentredArc (centreX, centreY, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (green);
    g.strokePath (valueRing, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (green);
    g.fillEllipse (centreX - 2.5f, centreY - 2.5f, 5.0f, 5.0f);
}

//==============================================================================
DelexAudioProcessorEditor::DelexAudioProcessorEditor (DelexAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    unlocked = audioProcessor.isUnlocked();

    if (unlocked)
        buildMainUI();
    else
        buildSerialUI();

    setSize (680, 560);
    startTimerHz (30);
}

DelexAudioProcessorEditor::~DelexAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void DelexAudioProcessorEditor::buildSerialUI()
{
    serialInput = std::make_unique<juce::TextEditor>();
    serialInput->setTextToShowWhenEmpty ("DLX-XXXX-XXXX-XXXX", juce::Colour (0xFF445566));
    serialInput->setJustification (juce::Justification::centred);
    serialInput->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF12122A));
    serialInput->setColour (juce::TextEditor::textColourId, colWhite);
    serialInput->setColour (juce::TextEditor::outlineColourId, colGreen);
    serialInput->setColour (juce::TextEditor::focusedOutlineColourId, colGreen);
    serialInput->setFont (juce::Font (juce::FontOptions (16.0f)));
    addAndMakeVisible (*serialInput);

    activateButton = std::make_unique<juce::TextButton> ("ATIVAR PLUGIN");
    activateButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF00805A));
    activateButton->setColour (juce::TextButton::textColourOffId, colWhite);
    activateButton->onClick = [this] { attemptActivation(); };
    addAndMakeVisible (*activateButton);

    serialMessage = std::make_unique<juce::Label>();
    serialMessage->setJustificationType (juce::Justification::centred);
    serialMessage->setColour (juce::Label::textColourId, juce::Colour (0xFFCC5555));
    serialMessage->setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (*serialMessage);
}

void DelexAudioProcessorEditor::attemptActivation()
{
    auto serial = serialInput->getText();
    if (audioProcessor.tryUnlock (serial))
    {
        unlocked = true;
        serialInput.reset();
        activateButton.reset();
        serialMessage.reset();
        buildMainUI();
        resized();
        repaint();
    }
    else
    {
        serialMessage->setText ("Chave invalida. Verifica e tenta novamente.", juce::dontSendNotification);
    }
}

//==============================================================================
void DelexAudioProcessorEditor::buildMainUI()
{
    setupKnob (timeKnob, "time", "TIME");
    setupKnob (feedbackKnob, "feedback", "FBK");
    setupKnob (mixKnob, "mix", "MIX");
    setupKnob (spreadKnob, "spread", "SPRD");
    setupKnob (hicutKnob, "hicut", "HI");
    setupKnob (locutKnob, "locut", "LO");
    setupKnob (eqLowKnob, "eqLow", "LOW");
    setupKnob (eqMidKnob, "eqMid", "MID");
    setupKnob (eqHighKnob, "eqHigh", "HIGH");
    setupKnob (modRateKnob, "modRate", "RATE");
    setupKnob (modDepthKnob, "modDepth", "DEPTH");
    setupKnob (driveKnob, "drive", "DRIVE");
    setupKnob (lofiKnob, "lofi", "LOFI");

    pingpongButton = std::make_unique<juce::TextButton> ("PING PONG");
    pingpongButton->setClickingTogglesState (true);
    pingpongButton->setColour (juce::TextButton::buttonColourId, colCard);
    pingpongButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00251A));
    pingpongButton->setColour (juce::TextButton::textColourOffId, colWhite);
    pingpongButton->setColour (juce::TextButton::textColourOnId, colGreen);
    addAndMakeVisible (*pingpongButton);
    pingpongAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pingpong", *pingpongButton);

    modWaveButton = std::make_unique<juce::TextButton> ("SINE");
    modWaveButton->setClickingTogglesState (true);
    modWaveButton->setColour (juce::TextButton::buttonColourId, colCard);
    modWaveButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00251A));
    modWaveButton->setColour (juce::TextButton::textColourOffId, colWhite);
    modWaveButton->setColour (juce::TextButton::textColourOnId, colGreen);
    modWaveButton->onClick = [this]
    {
        modWaveButton->setButtonText (modWaveButton->getToggleState() ? "TRI" : "SINE");
    };
    addAndMakeVisible (*modWaveButton);
    modWaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "modWave", *modWaveButton);

    tapButton = std::make_unique<juce::TextButton> ("TAP");
    tapButton->setColour (juce::TextButton::buttonColourId, colCard);
    tapButton->setColour (juce::TextButton::textColourOffId, colGreen);
    tapButton->onClick = [this] { audioProcessor.registerTap(); };
    addAndMakeVisible (*tapButton);

    const auto& presets = DelexAudioProcessor::getFactoryPresets();
    static const char* shortNames[] = { "Studio", "Slap", "Pong", "Tape", "Dark", "Shim" };
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton> (shortNames[i]);
        btn->setColour (juce::TextButton::buttonColourId, colCard);
        btn->setColour (juce::TextButton::textColourOffId, colWhite);
        int idx = i;
        btn->onClick = [this, idx] { audioProcessor.applyPreset (idx); };
        addAndMakeVisible (*btn);
        presetButtons.push_back (std::move (btn));
    }

    aboutButton = std::make_unique<juce::TextButton> ("i");
    aboutButton->setColour (juce::TextButton::buttonColourId, colCard);
    aboutButton->setColour (juce::TextButton::textColourOffId, colGreen);
    aboutButton->onClick = [this]
    {
        showAbout = ! showAbout;
        setControlsVisible (! showAbout);
        repaint();
    };
    addAndMakeVisible (*aboutButton);
}

void DelexAudioProcessorEditor::setControlsVisible (bool v)
{
    auto sk = [v] (Knob& k) { if (k.slider) k.slider->setVisible (v); if (k.label) k.label->setVisible (v); };
    sk (timeKnob); sk (feedbackKnob); sk (mixKnob); sk (spreadKnob);
    sk (hicutKnob); sk (locutKnob);
    sk (eqLowKnob); sk (eqMidKnob); sk (eqHighKnob);
    sk (modRateKnob); sk (modDepthKnob);
    sk (driveKnob); sk (lofiKnob);
    if (pingpongButton) pingpongButton->setVisible (v);
    if (modWaveButton) modWaveButton->setVisible (v);
    if (tapButton) tapButton->setVisible (v);
    for (auto& b : presetButtons) if (b) b->setVisible (v);
}

void DelexAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& paramID, const juce::String& labelText)
{
    knob.slider = std::make_unique<juce::Slider>();
    knob.slider->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob.slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (*knob.slider);

    knob.label = std::make_unique<juce::Label>();
    knob.label->setText (labelText, juce::dontSendNotification);
    knob.label->setJustificationType (juce::Justification::centred);
    knob.label->setColour (juce::Label::textColourId, colWhite);
    knob.label->setFont (juce::Font (juce::FontOptions (9.0f)));
    addAndMakeVisible (*knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, paramID, *knob.slider);
}

//==============================================================================
void DelexAudioProcessorEditor::timerCallback()
{
    if (! unlocked) return;
    meterL = meterL * 0.7f + audioProcessor.outputLevelL.load() * 0.3f;
    meterR = meterR * 0.7f + audioProcessor.outputLevelR.load() * 0.3f;
    repaint();
}

//==============================================================================
void DelexAudioProcessorEditor::drawCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (colCard);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (colCardLine);
    g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
    g.setColour (colTeal);
    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
    g.drawText (title.toUpperCase(), r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 12, juce::Justification::left, false);
}

void DelexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colBg);

    // Header
    g.setColour (juce::Colour (0xFF03030A));
    g.fillRect (0, 0, getWidth(), 48);
    g.setColour (colCardLine);
    g.fillRect (0, 48, getWidth(), 1);
    g.setColour (colGreen);
    g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    g.drawText ("DELEX", 18, 12, 160, 26, juce::Justification::left, false);
    g.setColour (colWhite.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("by Piradex", 110, 19, 120, 14, juce::Justification::left, false);
    g.drawText ("v1.0 BETA", getWidth() - 130, 19, 80, 14, juce::Justification::right, false);

    if (! unlocked)
    {
        g.setColour (colWhite);
        g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
        g.drawText ("ATIVACAO NECESSARIA", 0, 150, getWidth(), 26, juce::Justification::centred, false);
        g.setColour (colWhite.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Introduz a tua chave beta para desbloquear", 0, 180, getWidth(), 18, juce::Justification::centred, false);
        return;
    }

    // VU card
    auto vu = juce::Rectangle<int> (14, 58, getWidth() - 28, 52);
    drawCard (g, vu, "VU");
    int vx = vu.getX() + 50, vw = vu.getWidth() - 64;
    int by1 = vu.getY() + 18, by2 = vu.getY() + 32;
    int nbars = 36;
    int bw = vw / nbars;
    int actL = juce::roundToInt (meterL * nbars);
    int actR = juce::roundToInt (meterR * nbars);
    for (int i = 0; i < nbars; ++i)
    {
        juce::Colour c = (i > 30) ? juce::Colour (0xFFFF4444) : (i > 24) ? juce::Colour (0xFFFFD400) : colGreen;
        g.setColour (i < actL ? c : juce::Colour (0x3315151F));
        g.fillRect (vx + i * bw, by1, bw - 2, 8);
        g.setColour (i < actR ? c : juce::Colour (0x3315151F));
        g.fillRect (vx + i * bw, by2, bw - 2, 8);
    }

    // Cards de secções (títulos desenhados aqui; knobs posicionados em resized)
    drawCard (g, juce::Rectangle<int> (14, 118, 324, 130), "Delay Core");
    drawCard (g, juce::Rectangle<int> (346, 118, 320, 130), "Modulation");
    drawCard (g, juce::Rectangle<int> (14, 256, 215, 120), "Filter");
    drawCard (g, juce::Rectangle<int> (237, 256, 215, 120), "Analog");
    drawCard (g, juce::Rectangle<int> (460, 256, 206, 120), "Delay EQ");

    if (showAbout)
        paintAbout (g);
}

void DelexAudioProcessorEditor::paintAbout (juce::Graphics& g)
{
    g.setColour (colBg);
    g.fillRect (0, 49, getWidth(), getHeight() - 49);

    auto box = juce::Rectangle<int> (getWidth() / 2 - 180, getHeight() / 2 - 130, 360, 250);
    g.setColour (colCard);
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (colGreen);
    g.drawRoundedRectangle (box.toFloat(), 8.0f, 1.0f);

    g.setColour (colGreen);
    g.setFont (juce::Font (juce::FontOptions (28.0f).withStyle ("Bold")));
    g.drawText ("DELEX", box.getX(), box.getY() + 20, box.getWidth(), 32, juce::Justification::centred, false);

    g.setColour (colWhite);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("Analog Delay Engine", box.getX(), box.getY() + 56, box.getWidth(), 18, juce::Justification::centred, false);

    g.setColour (colWhite.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    int ty = box.getY() + 92;
    g.drawText ("Criador:  Piradex", box.getX() + 30, ty, box.getWidth() - 60, 16, juce::Justification::left, false);
    g.drawText ("Versao:  1.0 BETA", box.getX() + 30, ty + 22, box.getWidth() - 60, 16, juce::Justification::left, false);
    g.drawText ("Lancamento:  Junho 2026", box.getX() + 30, ty + 44, box.getWidth() - 60, 16, juce::Justification::left, false);

    g.setColour (colWhite.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawFittedText ("Delay estereo com filtros, EQ de 3 bandas, modulacao, saturacao analogica, lofi, tap tempo e presets. Build beta para testes.",
                      box.getX() + 30, ty + 72, box.getWidth() - 60, 50, juce::Justification::centredLeft, 4);

    g.setColour (colGreen);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("(clica em i para fechar)", box.getX(), box.getBottom() - 26, box.getWidth(), 16, juce::Justification::centred, false);
}

//==============================================================================
void DelexAudioProcessorEditor::resized()
{
    if (! unlocked)
    {
        if (serialInput)    serialInput->setBounds (getWidth() / 2 - 140, 220, 280, 36);
        if (activateButton) activateButton->setBounds (getWidth() / 2 - 90, 268, 180, 34);
        if (serialMessage)  serialMessage->setBounds (getWidth() / 2 - 180, 312, 360, 20);
        return;
    }

    auto placeKnob = [&] (Knob& k, int cx, int cy, int size)
    {
        if (k.slider == nullptr || k.label == nullptr) return;
        k.slider->setBounds (cx - size / 2, cy - size / 2, size, size);
        k.label->setBounds (cx - 30, cy + size / 2 - 2, 60, 12);
    };

    // Delay Core (4 knobs) — card em (14,118,324,130)
    int coreY = 175;
    placeKnob (timeKnob,     14 + 50,  coreY, 50);
    placeKnob (feedbackKnob, 14 + 130, coreY, 50);
    placeKnob (mixKnob,      14 + 205, coreY, 50);
    placeKnob (spreadKnob,   14 + 280, coreY, 50);
    if (tapButton) tapButton->setBounds (14 + 90, coreY + 42, 150, 22);

    // Modulation (2 knobs + wave button) — card em (346,118,320,130)
    placeKnob (modRateKnob,  346 + 70,  coreY, 50);
    placeKnob (modDepthKnob, 346 + 160, coreY, 50);
    if (modWaveButton) modWaveButton->setBounds (346 + 210, coreY - 14, 80, 28);

    // Filter (2 knobs + ping pong) — card em (14,256,215,120)
    int fY = 308;
    placeKnob (hicutKnob, 14 + 60, fY, 46);
    placeKnob (locutKnob, 14 + 140, fY, 46);
    if (pingpongButton) pingpongButton->setBounds (14 + 30, fY + 38, 155, 22);

    // Analog (drive + lofi) — card em (237,256,215,120)
    placeKnob (driveKnob, 237 + 65, fY, 46);
    placeKnob (lofiKnob,  237 + 150, fY, 46);

    // Delay EQ (3 knobs) — card em (460,256,206,120)
    placeKnob (eqLowKnob,  460 + 45,  fY, 44);
    placeKnob (eqMidKnob,  460 + 103, fY, 44);
    placeKnob (eqHighKnob, 460 + 161, fY, 44);

    // Presets (6 botões)
    int px = 14, pw = 104, pgap = 5;
    for (auto& btn : presetButtons)
    {
        btn->setBounds (px, 392, pw, 28);
        px += pw + pgap;
    }

    if (aboutButton) aboutButton->setBounds (getWidth() - 42, 14, 22, 22);
}
