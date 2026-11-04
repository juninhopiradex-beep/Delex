#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace delex;

DelexProcessor::DelexProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "DELEX", createLayout())
{
    juce::PropertiesFile::Options o;
    o.applicationName = "DELEX";
    o.folderName = "Piradex";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (o);
    lockMask = settings->getIntValue ("lockMask", 0);
    for (auto& m : ccMap) m.store (-1);
    lic::License::get();   // carrega o estado da licença na thread de mensagens
}

// ---------------------------------------------------------------------------- áudio
void DelexProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    (void) divisions(); (void) modDivisions();   // tabelas estáticas criadas fora da thread de áudio
    engine.prepare (sampleRate, samplesPerBlock);
    work.setSize (4, juce::jmax (1, samplesPerBlock), false, true, false);
    scopeFifo.reset();
}

void DelexProcessor::reset()
{
    engine.reset();
}

bool DelexProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)   // sidechain: desligado, mono ou estéreo
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return in.size() <= out.size();   // mono→mono, mono→estéreo, estéreo→estéreo
}

void DelexProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)  { handleMidi (m); processAny (b); }
void DelexProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { handleMidi (m); processAny (b); }

// ---------------------------------------------------------------------------- MIDI learn
void DelexProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const int learn = learnTarget.load();
            if (learn >= 0)
            {
                for (auto& m : ccMap) if (m.load() == learn) m.store (-1);   // um parâmetro ↔ um CC
                ccMap[(size_t) cc].store (learn);
                learnTarget.store (-1);
                lastLearnedCc.store (cc);
            }
            const int target = ccMap[(size_t) cc].load();
            if (target >= 0)
                if (auto* prm = raw.parameter (target))
                    prm->setValueNotifyingHost ((float) msg.getControllerValue() / 127.0f);
        }
        handleMidiNote (msg);
    }
}

void DelexProcessor::startMidiLearn (const juce::String& pid) { learnTarget.store (delex::RawParams::indexOf (pid)); }
void DelexProcessor::forgetMidi (const juce::String& pid)
{
    const int idx = delex::RawParams::indexOf (pid);
    for (auto& m : ccMap) if (m.load() == idx) m.store (-1);
}
int DelexProcessor::midiCcFor (const juce::String& pid) const
{
    const int idx = delex::RawParams::indexOf (pid);
    for (size_t c = 0; c < ccMap.size(); ++c) if (ccMap[c].load() == idx) return (int) c;
    return -1;
}
bool DelexProcessor::isLearning (const juce::String& pid) const { return learnTarget.load() == delex::RawParams::indexOf (pid); }

void DelexProcessor::setLockMask (int m)
{
    lockMask = m;
    settings->setValue ("lockMask", m);
    settings->saveIfNeeded();
}

template <typename Sample>
void DelexProcessor::processAny (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    auto mainIn = getBusBuffer (buffer, true, 0);
    const int numIn = mainIn.getNumChannels();
    const int numOut = getMainBusNumOutputChannels();

    if (work.getNumSamples() < n) work.setSize (4, n, false, false, true);
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);

    const Sample* inL = numIn > 0 ? mainIn.getReadPointer (0) : nullptr;
    const Sample* inR = numIn > 1 ? mainIn.getReadPointer (1) : inL;
    float pkIn[2] { 0, 0 };
    for (int i = 0; i < n; ++i)
    {
        L[i] = inL != nullptr ? (float) inL[i] : 0.0f;
        R[i] = inR != nullptr ? (float) inR[i] : 0.0f;
        pkIn[0] = juce::jmax (pkIn[0], std::abs (L[i]));
        pkIn[1] = juce::jmax (pkIn[1], std::abs (R[i]));
    }

    // sem sessão iniciada: o plugin é transparente (não estraga a sessão do DAW)
    if (! lic::License::get().isUnlocked())
    {
        for (int c = 0; c < numOut; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            const float* src = (c == 0 || numOut == 1) ? L : R;
            for (int i = 0; i < n; ++i) o[i] = (Sample) (numOut == 1 ? 0.5f * (L[i] + R[i]) : src[i]);
        }
        return;
    }

    // tempo do host
    double bpm = hostBpm.load();
    bool hasBpm = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hb = pos->getBpm()) { bpm = *hb; hasBpm = true; }
    if (! hasBpm && tapBpm > 0.0) bpm = tapBpm;
    hostBpm = bpm; hostHasBpm = hasBpm;

    // chave de sidechain (ducker, gate…): só se escolhida e ligada no DAW
    const float* keyL = nullptr; const float* keyR = nullptr;
    {
        const Values v0 = raw.load();
        const bool wantSc = v0.key_src > 0.5f;
        bool scConnected = false;
        if (wantSc && getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                float* kL = work.getWritePointer (2); float* kR = work.getWritePointer (3);
                const Sample* sL = sc.getReadPointer (0);
                const Sample* sR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : sL;
                for (int i = 0; i < n; ++i) { kL[i] = (float) sL[i]; kR[i] = (float) sR[i]; }
                keyL = kL; keyR = kR; scConnected = true;
            }
        }
        sidechainActive = scConnected;
    }

    // processa em pedaços curtos: valores (e, nas próximas fases, modulação) actualizados a cada pedaço
    for (int pos = 0; pos < n; pos += kChunk)
    {
        const int len = juce::jmin (kChunk, n - pos);
        const Values v = valuesForChunk (bpm);
        engine.setParams (toEngine (v, bpm, getSampleRate()));
        engine.process (L + pos, R + pos, len, keyL != nullptr ? keyL + pos : nullptr, keyR != nullptr ? keyR + pos : nullptr);
    }

    // medidores
    float pkOut[2] { 0, 0 };
    float xy = 0, xx = 0, yy = 0;
    for (int i = 0; i < n; ++i)
    {
        pkOut[0] = juce::jmax (pkOut[0], std::abs (L[i]));
        pkOut[1] = juce::jmax (pkOut[1], std::abs (R[i]));
        xy += L[i] * R[i]; xx += L[i] * L[i]; yy += R[i] * R[i];
    }
    const float decay = std::exp (-(float) n / (0.25f * (float) getSampleRate()));
    corrXY = corrXY * decay + xy; corrXX = corrXX * decay + xx; corrYY = corrYY * decay + yy;
    const float den = std::sqrt (corrXX * corrYY);
    correlation = den > 1.0e-9f ? juce::jlimit (-1.0f, 1.0f, corrXY / den) : 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        inPeak[c] = juce::jmax (pkIn[c], inPeak[c].load() * 0.92f);
        outPeak[c] = juce::jmax (pkOut[c], outPeak[c].load() * 0.92f);
    }
    duckGr = engine.duckReductionDb();
    loopLimit = engine.loopLimiterDb();
    pushScope (L, R, n);

    // saída
    auto mainOut = getBusBuffer (buffer, false, 0);
    if (numOut == 1)
    {
        auto* o = mainOut.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = (Sample) (0.5f * (L[i] + R[i]));
    }
    else if (numOut >= 2)
    {
        auto* oL = mainOut.getWritePointer (0);
        auto* oR = mainOut.getWritePointer (1);
        for (int i = 0; i < n; ++i) { oL[i] = (Sample) L[i]; oR[i] = (Sample) R[i]; }
    }
    for (int c = numOut; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

// Valores a usar num pedaço de áudio (ponto de entrada para morph, modulação e pads)
delex::Values DelexProcessor::valuesForChunk (double /*bpm*/)
{
    return raw.load();
}

void DelexProcessor::pushScope (const float* l, const float* r, int n)
{
    const int want = n / 2;
    if (want <= 0 || scopeFifo.getFreeSpace() < want) return;
    int s1, n1, s2, n2;
    scopeFifo.prepareToWrite (want, s1, n1, s2, n2);
    int k = 0;
    for (int i = 0; i < n1; ++i, k += 2) { scopeData[(size_t) (s1 + i) * 2] = l[k]; scopeData[(size_t) (s1 + i) * 2 + 1] = r[k]; }
    for (int i = 0; i < n2; ++i, k += 2) { scopeData[(size_t) (s2 + i) * 2] = l[k]; scopeData[(size_t) (s2 + i) * 2 + 1] = r[k]; }
    scopeFifo.finishedWrite (n1 + n2);
}

// ---------------------------------------------------------------------------- presets
void DelexProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms())) loadFactoryPreset (index);
}

const juce::String DelexProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String::fromUTF8 (p[(size_t) index].name) : juce::String();
}

void DelexProcessor::loadFactoryPreset (int index)
{
    const auto& p = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size())) return;
    undo.beginNewTransaction();
    applyData (apvts, p[(size_t) index].data, false, lockMask);
    currentPreset = index;
    presetName = juce::String::fromUTF8 (p[(size_t) index].name);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void DelexProcessor::applyStyle (int index)
{
    const auto& s = styles();
    if (! juce::isPositiveAndBelow (index, (int) s.size())) return;
    undo.beginNewTransaction();
    applyData (apvts, s[(size_t) index].data, true, lockMask);
}

juce::File DelexProcessor::userPresetFolder() const
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return base.getChildFile ("Piradex").getChildFile ("DELEX").getChildFile ("Presets");
}

bool DelexProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto dir = userPresetFolder();
    if (! dir.createDirectory()) return false;
    auto st = apvts.copyState();
    st.setProperty ("presetName", name.trim(), nullptr);
    st.setProperty ("version", kStateVersion, nullptr);
    if (auto xml = st.createXml())
        if (xml->writeTo (dir.getChildFile (clean + ".dlxpreset")))
        {
            presetName = name.trim();
            currentPreset = -1;
            return true;
        }
    return false;
}

void DelexProcessor::loadUserPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
    {
        auto st = juce::ValueTree::fromXml (*xml);
        if (st.hasType (apvts.state.getType()))
        {
            undo.beginNewTransaction();
            for (auto* prm : getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    if (presetIgnores (r->getParameterID()) || (lockGroupOf (r->getParameterID()) & lockMask) != 0) continue;
                    auto child = st.getChildWithProperty ("id", r->getParameterID());
                    const float val = child.isValid() ? (float) child.getProperty ("value") : r->convertFrom0to1 (r->getDefaultValue());
                    r->beginChangeGesture(); r->setValueNotifyingHost (r->convertTo0to1 (val)); r->endChangeGesture();
                }
            presetName = st.getProperty ("presetName", f.getFileNameWithoutExtension()).toString();
            currentPreset = -1;
        }
    }
}

bool DelexProcessor::isFavourite (const juce::String& name) const
{
    return juce::StringArray::fromLines (settings->getValue ("favourites")).contains (name);
}

void DelexProcessor::toggleFavourite (const juce::String& name)
{
    auto fav = juce::StringArray::fromLines (settings->getValue ("favourites"));
    fav.removeEmptyStrings();
    if (fav.contains (name)) fav.removeString (name); else fav.add (name);
    settings->setValue ("favourites", fav.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

// ---------------------------------------------------------------------------- A/B
void DelexProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == slotIndex) return;
    slots[slotIndex] = apvts.copyState();
    if (slots[slot].isValid()) apvts.replaceState (slots[slot].createCopy());
    else slots[slot] = apvts.copyState();
    slotIndex = slot;
}

void DelexProcessor::copyToOtherSlot()
{
    slots[1 - slotIndex] = apvts.copyState();
}

// ---------------------------------------------------------------------------- tap tempo
void DelexProcessor::tap()
{
    const auto now = (juce::int64) juce::Time::getMillisecondCounterHiRes();
    const double dt = (double) (now - lastTapMs);
    lastTapMs = now;
    if (dt > 2000.0 || dt < 100.0) { tapIntervals.clear(); return; }
    tapIntervals.add (dt);
    while (tapIntervals.size() > 4) tapIntervals.remove (0);
    double avg = 0; for (auto x : tapIntervals) avg += x; avg /= tapIntervals.size();
    tapBpm = 60000.0 / avg;

    // define o tempo em ms (sem sync) = semínima batida × divisão actual
    const auto& d = divisions();
    const int di = juce::jlimit (0, (int) d.size() - 1, (int) std::lround (raw.load().div));
    if (auto* s = apvts.getParameter (id::sync)) { s->beginChangeGesture(); s->setValueNotifyingHost (0.0f); s->endChangeGesture(); }
    if (auto* t = apvts.getParameter (id::time))
    {
        t->beginChangeGesture();
        t->setValueNotifyingHost (t->convertTo0to1 ((float) juce::jlimit (1.0, 8000.0, avg * d[(size_t) di].quarters)));
        t->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------- estado
void DelexProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DELEX_STATE");
    root.setProperty ("version", kStateVersion, nullptr);
    root.setProperty ("preset", currentPreset, nullptr);
    root.setProperty ("presetName", presetName, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("advanced", advancedView, nullptr);
    root.setProperty ("tab", advancedTab, nullptr);
    root.setProperty ("slot", slotIndex, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    {
        juce::ValueTree midi ("MIDI");
        for (size_t c = 0; c < ccMap.size(); ++c)
            if (const int t = ccMap[c].load(); t >= 0)
            {
                juce::ValueTree m ("CC"); m.setProperty ("cc", (int) c, nullptr); m.setProperty ("param", juce::String (delex::paramIds()[(size_t) t]), nullptr);
                midi.appendChild (m, nullptr);
            }
        root.appendChild (midi, nullptr);
    }
    for (int s = 0; s < 2; ++s)
        if (slots[s].isValid() && s != slotIndex)
        {
            juce::ValueTree holder ("SLOT"); holder.setProperty ("index", s, nullptr);
            holder.appendChild (slots[s].createCopy(), nullptr);
            root.appendChild (holder, nullptr);
        }
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void DelexProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (root.hasType ("DELEX_STATE"))
    {
        // versão 2: formato actual (futuras migrações entram aqui)
        auto params = root.getChildWithName (apvts.state.getType());
        if (params.isValid()) apvts.replaceState (params);
        currentPreset = root.getProperty ("preset", 0);
        presetName = root.getProperty ("presetName", "Init").toString();
        uiScale = juce::jlimit (0.75f, 2.0f, (float) root.getProperty ("uiScale", 1.0f));
        advancedView = root.getProperty ("advanced", false);
        advancedTab = root.getProperty ("tab", 2);
        slotIndex = juce::jlimit (0, 1, (int) root.getProperty ("slot", 0));
        slots[0] = slots[1] = {};
        for (auto& m : ccMap) m.store (-1);
        if (auto midi = root.getChildWithName ("MIDI"); midi.isValid())
            for (auto m : midi)
            {
                const int cc = m.getProperty ("cc", -1), idx = delex::RawParams::indexOf (m.getProperty ("param").toString());
                if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) ccMap[(size_t) cc].store (idx);
            }
        for (auto c : root)
            if (c.hasType ("SLOT") && c.getNumChildren() > 0)
                slots[juce::jlimit (0, 1, (int) c.getProperty ("index"))] = c.getChild (0).createCopy();
    }
    else if (root.hasType (apvts.state.getType()))
        apvts.replaceState (root);   // estado simples (compatibilidade)
    undo.clearUndoHistory();
}

juce::AudioProcessorEditor* DelexProcessor::createEditor() { return new DelexEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelexProcessor(); }
