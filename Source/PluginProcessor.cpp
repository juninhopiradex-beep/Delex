#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static const juce::StringArray kValidSerials {
    "DLX-HWGQ-496R-AFHU", "DLX-G6A9-WP3S-YY2E", "DLX-XFUU-7KVB-ZZ75",
    "DLX-F39H-593S-48BQ", "DLX-JDG5-8K8Q-H78S", "DLX-3V83-5NUT-ACK7",
    "DLX-9ZNS-9795-CUW7", "DLX-G6S6-M58E-T9ZG", "DLX-N4K6-AFA6-FNRH",
    "DLX-CTYK-AW9J-Q8JU", "DLX-F4LJ-V42K-N2AG", "DLX-2EDY-YBB8-WV6W",
    "DLX-H97L-KP5X-7QE3", "DLX-CNMC-D6N8-BJPJ", "DLX-LM2T-54F2-VMYM",
    "DLX-S794-U7MW-86HT", "DLX-GPQA-F7X4-MBYL", "DLX-YPHT-PUTJ-GSG5",
    "DLX-LGDY-MGWT-EUWL", "DLX-SSEA-4S4A-DGRB"
};

//==============================================================================
const std::vector<DelexPreset>& DelexAudioProcessor::getFactoryPresets()
{
    static const std::vector<DelexPreset> presets = {
        // name          time   fb    mix   spr   hicut  locut  low   mid   high  rate depth drive lofi  pp
        { "Studio Room", 380.f, 0.45f, 0.50f, 0.50f, 8000.f, 100.f, 0.f, 0.f, 0.f, 0.3f, 0.0f, 0.1f, 0.0f, false },
        { "Slapback",    120.f, 0.20f, 0.35f, 0.20f, 6000.f, 200.f, 0.f, 2.f, -2.f, 0.0f, 0.0f, 0.2f, 0.0f, false },
        { "Ping Pong",   300.f, 0.55f, 0.45f, 0.80f, 9000.f, 120.f, 0.f, 0.f, 1.f, 0.4f, 0.1f, 0.1f, 0.0f, true },
        { "Tape Echo",   420.f, 0.60f, 0.40f, 0.40f, 4500.f, 250.f, 2.f, 1.f, -4.f, 0.5f, 0.3f, 0.4f, 0.3f, false },
        { "Dark Space",  650.f, 0.70f, 0.55f, 0.60f, 2800.f, 80.f, 3.f, -2.f, -8.f, 0.2f, 0.2f, 0.3f, 0.5f, false },
        { "Shimmer",     500.f, 0.65f, 0.50f, 0.75f, 12000.f, 300.f, -2.f, 0.f, 4.f, 0.6f, 0.4f, 0.2f, 0.1f, true }
    };
    return presets;
}

void DelexAudioProcessor::applyPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    const auto& p = presets[(size_t) index];

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (apvts.getParameterRange (id).convertTo0to1 (value));
    };

    set ("time", p.time);
    set ("feedback", p.feedback);
    set ("mix", p.mix);
    set ("spread", p.spread);
    set ("hicut", p.hicut);
    set ("locut", p.locut);
    set ("eqLow", p.eqLow);
    set ("eqMid", p.eqMid);
    set ("eqHigh", p.eqHigh);
    set ("modRate", p.modRate);
    set ("modDepth", p.modDepth);
    set ("drive", p.drive);
    set ("lofi", p.lofi);

    if (auto* param = apvts.getParameter ("pingpong"))
        param->setValueNotifyingHost (p.pingpong ? 1.0f : 0.0f);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DelexAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("time", "Time", juce::NormalisableRange<float> (10.0f, 2000.0f, 1.0f), 380.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback", juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spread", "Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hicut", "Hi Cut", juce::NormalisableRange<float> (500.0f, 18000.0f, 1.0f, 0.3f), 8000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("locut", "Lo Cut", juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqLow", "EQ Low", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqMid", "EQ Mid", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHigh", "EQ High", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("modRate", "Mod Rate", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("modDepth", "Mod Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("modWave", "Mod Wave", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lofi", "Lofi", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("pingpong", "Ping Pong", false));

    return { params.begin(), params.end() };
}

//==============================================================================
DelexAudioProcessor::DelexAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

DelexAudioProcessor::~DelexAudioProcessor() {}

//==============================================================================
void DelexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 1;

    delayLineL.prepare (spec);
    delayLineR.prepare (spec);
    delayLineL.reset();
    delayLineR.reset();

    juce::dsp::ProcessSpec stereoSpec;
    stereoSpec.sampleRate = sampleRate;
    stereoSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    stereoSpec.numChannels = 2;

    hiCutFilter.prepare (stereoSpec);
    hiCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    loCutFilter.prepare (stereoSpec);
    loCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    monoSpec.numChannels = 1;

    eqLowL.prepare (monoSpec);  eqLowR.prepare (monoSpec);
    eqMidL.prepare (monoSpec);  eqMidR.prepare (monoSpec);
    eqHighL.prepare (monoSpec); eqHighR.prepare (monoSpec);

    lfoPhase = 0.0f;
    updateEQ();
}

void DelexAudioProcessor::releaseResources() {}

void DelexAudioProcessor::updateEQ()
{
    float low  = apvts.getRawParameterValue ("eqLow")->load();
    float mid  = apvts.getRawParameterValue ("eqMid")->load();
    float high = apvts.getRawParameterValue ("eqHigh")->load();

    auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, 200.0f, 0.7f, juce::Decibels::decibelsToGain (low));
    auto midCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 1000.0f, 0.7f, juce::Decibels::decibelsToGain (mid));
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 5000.0f, 0.7f, juce::Decibels::decibelsToGain (high));

    *eqLowL.coefficients  = *lowCoeffs;   *eqLowR.coefficients  = *lowCoeffs;
    *eqMidL.coefficients  = *midCoeffs;   *eqMidR.coefficients  = *midCoeffs;
    *eqHighL.coefficients = *highCoeffs;  *eqHighR.coefficients = *highCoeffs;
}

//==============================================================================
// Saturação analógica (soft clipping com tanh)
float DelexAudioProcessor::saturate (float x, float drive)
{
    if (drive <= 0.0001f) return x;
    float amount = 1.0f + drive * 8.0f;
    return std::tanh (x * amount) / std::tanh (amount);
}

// Lofi: redução de sample rate (sample & hold) + bit crush leve
float DelexAudioProcessor::applyLofi (float x, float amount, int channel)
{
    if (amount <= 0.0001f) return x;

    int holdSamples = 1 + static_cast<int> (amount * 16.0f);

    float& hold = (channel == 0) ? lofiHoldL : lofiHoldR;
    int& counter = (channel == 0) ? lofiCounterL : lofiCounterR;

    if (counter <= 0)
    {
        hold = x;
        counter = holdSamples;
    }
    counter--;

    // Bit crush leve
    float bits = 16.0f - amount * 10.0f;
    float step = std::pow (2.0f, bits);
    float crushed = std::round (hold * step) / step;

    return crushed * amount + x * (1.0f - amount);
}

//==============================================================================
void DelexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (numChannels < 2) return;

    float timeMs = apvts.getRawParameterValue ("time")->load();
    float feedback = apvts.getRawParameterValue ("feedback")->load();
    float mix = apvts.getRawParameterValue ("mix")->load();
    float spread = apvts.getRawParameterValue ("spread")->load();
    float hicut = apvts.getRawParameterValue ("hicut")->load();
    float locut = apvts.getRawParameterValue ("locut")->load();
    float modRate = apvts.getRawParameterValue ("modRate")->load();
    float modDepth = apvts.getRawParameterValue ("modDepth")->load();
    int   modWave = static_cast<int> (apvts.getRawParameterValue ("modWave")->load());
    float drive = apvts.getRawParameterValue ("drive")->load();
    float lofi = apvts.getRawParameterValue ("lofi")->load();
    bool  pingpong = apvts.getRawParameterValue ("pingpong")->load() > 0.5f;

    updateEQ();

    float baseDelayL = (timeMs / 1000.0f) * static_cast<float> (currentSampleRate);
    float baseDelayR = baseDelayL * (1.0f + spread * 0.3f);

    // LFO de modulação: 0.05Hz a ~6Hz
    float lfoFreq = 0.05f + modRate * 6.0f;
    float lfoInc = lfoFreq / static_cast<float> (currentSampleRate);
    float modSamples = modDepth * 0.005f * static_cast<float> (currentSampleRate); // até 5ms de variação

    hiCutFilter.setCutoffFrequency (hicut);
    loCutFilter.setCutoffFrequency (locut);

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    float peakL = 0.0f;
    float peakR = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // LFO
        float lfoValue;
        if (modWave == 0)
            lfoValue = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
        else
            lfoValue = 2.0f * std::abs (2.0f * (lfoPhase - std::floor (lfoPhase + 0.5f))) - 1.0f;

        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        float dL = juce::jlimit (1.0f, 191000.0f, baseDelayL + lfoValue * modSamples);
        float dR = juce::jlimit (1.0f, 191000.0f, baseDelayR + lfoValue * modSamples);

        float dryL = left[n];
        float dryR = right[n];

        float delayedL = delayLineL.popSample (0, dL);
        float delayedR = delayLineR.popSample (0, dR);

        // Filtros
        float fL = hiCutFilter.processSample (0, delayedL);
        fL = loCutFilter.processSample (0, fL);
        float fR = hiCutFilter.processSample (1, delayedR);
        fR = loCutFilter.processSample (1, fR);

        // EQ
        fL = eqLowL.processSample (fL); fL = eqMidL.processSample (fL); fL = eqHighL.processSample (fL);
        fR = eqLowR.processSample (fR); fR = eqMidR.processSample (fR); fR = eqHighR.processSample (fR);

        // Analog: saturação + lofi
        fL = saturate (fL, drive);
        fR = saturate (fR, drive);
        fL = applyLofi (fL, lofi, 0);
        fR = applyLofi (fR, lofi, 1);

        if (pingpong)
        {
            delayLineL.pushSample (0, dryL + fR * feedback);
            delayLineR.pushSample (0, dryR + fL * feedback);
        }
        else
        {
            delayLineL.pushSample (0, dryL + fL * feedback);
            delayLineR.pushSample (0, dryR + fR * feedback);
        }

        left[n] = dryL * (1.0f - mix) + fL * mix;
        right[n] = dryR * (1.0f - mix) + fR * mix;

        peakL = juce::jmax (peakL, std::abs (left[n]));
        peakR = juce::jmax (peakR, std::abs (right[n]));
    }

    outputLevelL.store (peakL);
    outputLevelR.store (peakR);
}

//==============================================================================
// Tap tempo
void DelexAudioProcessor::registerTap()
{
    auto now = juce::Time::getMillisecondCounter();

    if (lastTapTime > 0)
    {
        auto interval = now - lastTapTime;
        if (interval > 100 && interval < 3000)
        {
            float bpm = 60000.0f / static_cast<float> (interval);
            currentBPM.store (bpm);

            // Converte para ms (uma batida) e define o time
            float ms = static_cast<float> (interval);
            ms = juce::jlimit (10.0f, 2000.0f, ms);
            if (auto* param = apvts.getParameter ("time"))
                param->setValueNotifyingHost (apvts.getParameterRange ("time").convertTo0to1 (ms));
        }
    }
    lastTapTime = now;
}

//==============================================================================
bool DelexAudioProcessor::isValidSerial (const juce::String& serial)
{
    return kValidSerials.contains (serial.trim().toUpperCase());
}

juce::File DelexAudioProcessor::getLicenseFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Piradex").getChildFile ("Delex");
    dir.createDirectory();
    return dir.getChildFile ("license.dat");
}

bool DelexAudioProcessor::isUnlocked()
{
    if (unlockChecked) return unlockedState;
    auto file = getLicenseFile();
    if (file.existsAsFile())
    {
        auto stored = file.loadFileAsString().trim();
        unlockedState = isValidSerial (stored);
    }
    unlockChecked = true;
    return unlockedState;
}

bool DelexAudioProcessor::tryUnlock (const juce::String& serial)
{
    if (isValidSerial (serial))
    {
        getLicenseFile().replaceWithText (serial.trim().toUpperCase());
        unlockedState = true;
        unlockChecked = true;
        return true;
    }
    return false;
}

//==============================================================================
void DelexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DelexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* DelexAudioProcessor::createEditor()
{
    return new DelexAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelexAudioProcessor();
}
