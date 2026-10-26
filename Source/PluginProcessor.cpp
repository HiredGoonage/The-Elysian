#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------------ parameters
AudioProcessorValueTreeState::ParameterLayout DelayVerbProcessor::createLayout()
{
    using Attr = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto ms  = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto sec = [] (float v, int) { return String (v, 1) + " s"; };
    auto fromText = [] (const String& t) { return t.getFloatValue(); };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, r, def,
            Attr().withStringFromValueFunction (fmt).withValueFromStringFunction (fromText)));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addC = [&] (const char* id, const char* name, StringArray items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // Delay
    addB ("delayOn",       "Delay On", true);
    addF ("delayTime",     "Delay Time",     { 10.0f, 2000.0f, 1.0f, 0.4f }, 400.0f, ms);
    addF ("delayFeedback", "Delay Feedback", { 0.0f, 95.0f, 0.1f },           35.0f, pct);
    addF ("delayMix",      "Delay Mix",      { 0.0f, 100.0f, 0.1f },          25.0f, pct);
    addB ("delaySync",     "Delay Sync", false);
    addC ("delayDiv",      "Delay Division", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);
    addB ("delayTriplet",  "Delay Triplet", false);
    addC ("delayMode",     "Delay Mode", { "Digital", "Tape", "Analog" }, 0);
    addF ("delayTone",     "Delay Tone", { -12.0f, 12.0f, 0.1f },              0.0f, db);
    addF ("delayMod",      "Delay Mod",  { 0.0f, 100.0f, 0.1f },              10.0f, pct);
    addF ("delayWow",      "Delay Wow/Flutter", { 0.0f, 100.0f, 0.1f },        5.0f, pct);

    // Reverb
    addB ("revOn",    "Reverb On", true);
    addF ("revDecay", "Reverb Decay",     { 0.2f, 20.0f, 0.01f, 0.45f }, 2.8f, sec);
    addF ("revPre",   "Reverb Pre-Delay", { 0.0f, 200.0f, 1.0f },        30.0f, ms);
    addF ("revMix",   "Reverb Mix",       { 0.0f, 100.0f, 0.1f },        30.0f, pct);
    addC ("revType",  "Reverb Type", { "Room", "Plate", "Hall" }, 1);
    addF ("revSize",  "Reverb Size",      { 0.0f, 100.0f, 0.1f },        80.0f, pct);
    addF ("revDamp",  "Reverb Damping",   { 0.0f, 100.0f, 0.1f },        35.0f, pct);
    addF ("revWidth", "Reverb Width",     { 0.0f, 100.0f, 0.1f },       100.0f, pct);

    return layout;
}

DelayVerbProcessor::DelayVerbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pDelayOn = get ("delayOn");   pDelayTime = get ("delayTime");   pFeedback = get ("delayFeedback");
    pDelayMix = get ("delayMix"); pSync = get ("delaySync");        pDiv = get ("delayDiv");
    pTriplet = get ("delayTriplet"); pMode = get ("delayMode");     pTone = get ("delayTone");
    pMod = get ("delayMod");      pWow = get ("delayWow");
    pRevOn = get ("revOn");       pDecay = get ("revDecay");        pPre = get ("revPre");
    pRevMix = get ("revMix");     pType = get ("revType");          pSize = get ("revSize");
    pRevDamp = get ("revDamp");   pWidth = get ("revWidth");
}

bool DelayVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

// ------------------------------------------------------------------ DSP helpers
void DelayVerbProcessor::Biquad::setHighShelf (double sr, float freq, float gainDb)
{
    const double A = std::pow (10.0, (double) gainDb / 40.0);
    const double w0 = MathConstants<double>::twoPi * (double) freq / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / std::sqrt (2.0);            // shelf slope S = 1
    const double tsa = 2.0 * std::sqrt (A) * alpha;

    const double nb0 =        A * ((A + 1) + (A - 1) * cw + tsa);
    const double nb1 = -2.0 * A * ((A - 1) + (A + 1) * cw);
    const double nb2 =        A * ((A + 1) + (A - 1) * cw - tsa);
    const double na0 =            (A + 1) - (A - 1) * cw + tsa;
    const double na1 =  2.0 *    ((A - 1) - (A + 1) * cw);
    const double na2 =            (A + 1) - (A - 1) * cw - tsa;

    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}

void DelayVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };
    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 5.0) + 8);
    delayLine.reset();

    lastToneDb = 1.0e9f;
    for (auto& t : tone) t.clear();
    lpState[0] = lpState[1] = 0.0f;
    phMod = phWow = phFlut = 0.0;

    fdn.prepare (sampleRate);

    sTime.reset (sampleRate, 0.12);  sTime.setCurrentAndTargetValue (pDelayTime->load());
    sFb.reset (sampleRate, 0.03);    sFb.setCurrentAndTargetValue (pFeedback->load() * 0.01f);
    sDMix.reset (sampleRate, 0.03);  sDMix.setCurrentAndTargetValue (pDelayMix->load() * 0.01f);
    sDGate.reset (sampleRate, 0.03); sDGate.setCurrentAndTargetValue (pDelayOn->load() > 0.5f ? 1.0f : 0.0f);
    sRMix.reset (sampleRate, 0.03);  sRMix.setCurrentAndTargetValue (pRevMix->load() * 0.01f);
    sRGate.reset (sampleRate, 0.03); sRGate.setCurrentAndTargetValue (pRevOn->load() > 0.5f ? 1.0f : 0.0f);
    sWidth.reset (sampleRate, 0.03); sWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
}

// ------------------------------------------------------------------ processing
void DelayVerbProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (buffer.getNumChannels() < 2) return;

    const double sr = currentSampleRate;

    {   // input meter
        const float p = jmax (buffer.getMagnitude (0, 0, numSamples), buffer.getMagnitude (1, 0, numSamples));
        if (p > inPeak.load()) inPeak.store (p);
    }

    // ---- delay params
    const bool dOn = pDelayOn->load() > 0.5f;
    float timeMs = pDelayTime->load();
    if (pSync->load() > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    if (*b > 20.0) bpm = *b;

        static const double beats[6] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        const int div = jlimit (0, 5, (int) pDiv->load());
        timeMs = (float) (60000.0 / bpm * beats[div] * (pTriplet->load() > 0.5f ? 2.0 / 3.0 : 1.0));
    }
    timeMs = jlimit (10.0f, 4500.0f, timeMs);

    sTime.setTargetValue (timeMs);
    sFb.setTargetValue (pFeedback->load() * 0.01f);
    sDMix.setTargetValue (pDelayMix->load() * 0.01f);
    sDGate.setTargetValue (dOn ? 1.0f : 0.0f);

    const int mode = jlimit (0, 2, (int) pMode->load());
    const float cutoff = mode == 0 ? 18000.0f : (mode == 1 ? 4500.0f : 3000.0f);
    const float drive  = mode == 0 ? 0.0f     : (mode == 1 ? 1.8f    : 1.2f);
    const float lpA = 1.0f - std::exp (-MathConstants<float>::twoPi * jmin (cutoff, 0.45f * (float) sr) / (float) sr);

    const float toneDb = pTone->load();
    if (toneDb != lastToneDb)
    {
        for (auto& t : tone) t.setHighShelf (sr, 2500.0f, toneDb);
        lastToneDb = toneDb;
    }

    const float modAmt = pMod->load() * 0.01f;
    const float wowAmt = pWow->load() * 0.01f;
    const double incMod = 0.7 / sr, incWow = 0.55 / sr, incFlut = 6.5 / sr;
    const float maxDelay = (float) delayLine.getMaximumDelayInSamples() - 4.0f;

    // ---- reverb params
    sRMix.setTargetValue (pRevMix->load() * 0.01f);
    sRGate.setTargetValue (pRevOn->load() > 0.5f ? 1.0f : 0.0f);
    sWidth.setTargetValue (pWidth->load() * 0.01f);
    fdn.setParams (pDecay->load(), pRevDamp->load() * 0.01f, pSize->load() * 0.01f,
                   (int) pType->load(), pPre->load());

    auto* chL = buffer.getWritePointer (0);
    auto* chR = buffer.getWritePointer (1);
    float* data[2] = { chL, chR };

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = sTime.getNextValue(), fb = sFb.getNextValue();
        const float dm = sDMix.getNextValue(), gate = sDGate.getNextValue();

        phMod += incMod;  if (phMod >= 1.0) phMod -= 1.0;
        phWow += incWow;  if (phWow >= 1.0) phWow -= 1.0;
        phFlut += incFlut; if (phFlut >= 1.0) phFlut -= 1.0;

        const float wowSin = std::sin ((float) (MathConstants<double>::twoPi * phWow));
        const float flutSin = std::sin ((float) (MathConstants<double>::twoPi * phFlut));
        const float baseSamples = t * 0.001f * (float) sr;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float modSin = std::sin ((float) (MathConstants<double>::twoPi * (phMod + 0.25 * ch)));
            const float modMs = modAmt * 2.5f * modSin + wowAmt * (1.2f * wowSin + 0.12f * flutSin);
            const float dSamples = jlimit (2.0f, maxDelay, baseSamples + modMs * 0.001f * (float) sr);

            const float delayed = delayLine.popSample (ch, dSamples);

            lpState[ch] += lpA * (delayed - lpState[ch]);
            float f = lpState[ch];
            if (drive > 0.0f) f = std::tanh (f * drive) / drive;

            const float in = data[ch][i];
            delayLine.pushSample (ch, in * gate + f * fb);

            data[ch][i] = in + tone[ch].process (delayed) * dm * gate;
        }

        // reverb is fed by the delay output
        const float rGate = sRGate.getNextValue(), rMix = sRMix.getNextValue(), width = sWidth.getNextValue();
        const float l = chL[i], r = chR[i];
        float wl, wr;
        fdn.process (l * rGate, r * rGate, wl, wr);

        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        wl = mid + side;
        wr = mid - side;

        const float dryG = std::cos (rMix * MathConstants<float>::halfPi);
        const float wetG = std::sin (rMix * MathConstants<float>::halfPi) * rGate;
        chL[i] = l * dryG + wl * wetG;
        chR[i] = r * dryG + wr * wetG;
    }

    {   // output meter
        const float p = jmax (buffer.getMagnitude (0, 0, numSamples), buffer.getMagnitude (1, 0, numSamples));
        if (p > outPeak.load()) outPeak.store (p);
    }
}

// ------------------------------------------------------------------ presets
namespace
{
    struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> values; };

    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> p = {
            { "Ambient Space", {} },   // = parameter defaults
            { "Slapback Plate",   { { "delayTime", 110 }, { "delayFeedback", 12 }, { "delayMix", 30 }, { "delayMode", 1 },
                                    { "revDecay", 1.2f }, { "revMix", 15 }, { "revPre", 10 } } },
            { "Dotted Dream",     { { "delaySync", 1 }, { "delayDiv", 3 }, { "delayFeedback", 50 }, { "delayMix", 35 },
                                    { "delayMod", 25 }, { "revDecay", 4.0f }, { "revMix", 35 } } },
            { "Tape Echo Hall",   { { "delayMode", 1 }, { "delayTime", 520 }, { "delayFeedback", 48 }, { "delayTone", -3 },
                                    { "delayWow", 25 }, { "revType", 2 }, { "revDecay", 4.5f }, { "revMix", 25 } } },
            { "Cathedral",        { { "delayMix", 15 }, { "revType", 2 }, { "revDecay", 9.0f }, { "revSize", 100 },
                                    { "revDamp", 25 }, { "revMix", 45 }, { "revPre", 40 } } },
            { "Small Room",       { { "delayOn", 0 }, { "revType", 0 }, { "revDecay", 0.6f }, { "revSize", 40 },
                                    { "revMix", 18 }, { "revPre", 0 } } },
        };
        return p;
    }
}

int DelayVerbProcessor::getNumFactoryPresets() const { return (int) factoryPresets().size(); }

File DelayVerbProcessor::getPresetFolder() const
{
    auto dir = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("DelayVerb/Presets");
    dir.createDirectory();
    return dir;
}

StringArray DelayVerbProcessor::getPresetNames() const
{
    StringArray names;
    int n = 1;
    for (auto& p : factoryPresets())
        names.add (String::formatted ("%02d - ", n++) + p.name);

    Array<File> files;
    getPresetFolder().findChildFiles (files, File::findFiles, false, "*.xml");
    files.sort();
    for (auto& f : files) names.add (f.getFileNameWithoutExtension());
    return names;
}

void DelayVerbProcessor::setParam (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void DelayVerbProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());
}

void DelayVerbProcessor::loadPreset (int index)
{
    auto names = getPresetNames();
    if (! isPositiveAndBelow (index, names.size())) return;

    const int nf = getNumFactoryPresets();
    if (index < nf)
    {
        resetToDefaults();
        for (auto& kv : factoryPresets()[(size_t) index].values)
            setParam (kv.first, kv.second);
    }
    else
    {
        auto file = getPresetFolder().getChildFile (names[index] + ".xml");
        if (auto xml = parseXML (file))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (ValueTree::fromXml (*xml));
    }

    presetName = names[index];
    currentPresetIndex = index;
}

void DelayVerbProcessor::saveUserPreset (const String& name)
{
    auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return;

    apvts.state.setProperty ("presetName", clean, nullptr);
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (getPresetFolder().getChildFile (clean + ".xml"));

    presetName = clean;
    currentPresetIndex = getPresetNames().indexOf (clean);
}

// ------------------------------------------------------------------ state / editor
void DelayVerbProcessor::getStateInformation (MemoryBlock& destData)
{
    apvts.state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DelayVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            presetName = apvts.state.getProperty ("presetName", "01 - Ambient Space").toString();
            currentPresetIndex = getPresetNames().indexOf (presetName);
        }
}

AudioProcessorEditor* DelayVerbProcessor::createEditor()
{
    return new DelayVerbEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayVerbProcessor();
}
