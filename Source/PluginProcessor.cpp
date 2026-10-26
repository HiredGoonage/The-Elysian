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
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v * 0.001f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto fromText = [] (const String& t) { return t.getFloatValue(); };
    auto fromHz = [] (const String& t) { const float v = t.getFloatValue(); return t.containsIgnoreCase ("k") ? v * 1000.0f : v; };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt, std::function<float (const String&)> parse)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, r, def,
            Attr().withStringFromValueFunction (fmt).withValueFromStringFunction (parse)));
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
    addF ("delayTime",     "Delay Time",     { 10.0f, 2000.0f, 1.0f, 0.4f }, 400.0f, ms, fromText);
    addF ("delayFeedback", "Delay Feedback", { 0.0f, 95.0f, 0.1f },           35.0f, pct, fromText);
    addF ("delayMix",      "Delay Mix",      { 0.0f, 100.0f, 0.1f },          25.0f, pct, fromText);
    addB ("delaySync",     "Delay Sync", false);
    addC ("delayDiv",      "Delay Division", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);
    addB ("delayTriplet",  "Delay Triplet", false);
    addB ("delayDotted",   "Delay Dotted", false);
    addC ("delayMode",     "Delay Mode", { "Digital", "Tape", "Analog" }, 0);
    addF ("delaySat",      "Delay Saturation", { 0.0f, 100.0f, 0.1f },         0.0f, pct, fromText);
    addF ("delayTone",     "Delay Tone", { -12.0f, 12.0f, 0.1f },              0.0f, db, fromText);
    addF ("delayMod",      "Delay Mod",  { 0.0f, 100.0f, 0.1f },              10.0f, pct, fromText);
    addF ("delayWow",      "Delay Wow/Flutter", { 0.0f, 100.0f, 0.1f },        5.0f, pct, fromText);
    addF ("delayStereo",   "Delay Ping-Pong", { 0.0f, 100.0f, 0.1f },          0.0f, pct, fromText);
    addF ("delayLowCut",   "Delay Low Cut",  { 20.0f, 1000.0f, 1.0f, 0.35f },  20.0f, hz, fromHz);
    addF ("delayHighCut",  "Delay High Cut", { 1000.0f, 20000.0f, 1.0f, 0.35f }, 20000.0f, hz, fromHz);
    addF ("delayDuck",     "Delay Duck",     { 0.0f, 100.0f, 0.1f },           0.0f, pct, fromText);

    // Reverb
    addB ("revOn",    "Reverb On", true);
    addF ("revDecay", "Reverb Decay",     { 0.2f, 20.0f, 0.01f, 0.45f }, 2.8f, sec, fromText);
    addF ("revPre",   "Reverb Pre-Delay", { 0.0f, 200.0f, 1.0f },        30.0f, ms, fromText);
    addF ("revMix",   "Reverb Mix",       { 0.0f, 100.0f, 0.1f },        30.0f, pct, fromText);
    addC ("revType",  "Reverb Type", { "Room", "Plate", "Hall", "Cathedral", "Smooth" }, 1);
    addF ("revSize",  "Reverb Size",      { 0.0f, 100.0f, 0.1f },        80.0f, pct, fromText);
    addF ("revDamp",  "Reverb Damping",   { 0.0f, 100.0f, 0.1f },        35.0f, pct, fromText);
    addF ("revWidth", "Reverb Width",     { 0.0f, 100.0f, 0.1f },       100.0f, pct, fromText);
    addF ("revLowCut",  "Reverb Low Cut",  { 20.0f, 1000.0f, 1.0f, 0.35f },  20.0f, hz, fromHz);
    addF ("revHighCut", "Reverb High Cut", { 1000.0f, 20000.0f, 1.0f, 0.35f }, 20000.0f, hz, fromHz);
    addF ("revDuck",    "Reverb Duck",     { 0.0f, 100.0f, 0.1f },           0.0f, pct, fromText);
    addB ("revFreeze",  "Reverb Freeze", false);

    // Global
    addC ("routing", "Routing", { "Delay > Reverb", "Reverb > Delay", "Parallel" }, 0);
    addF ("outGain", "Output Gain", { -24.0f, 12.0f, 0.1f }, 0.0f, db, fromText);

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
    pTriplet = get ("delayTriplet"); pDotted = get ("delayDotted"); pMode = get ("delayMode");
    pSat = get ("delaySat"); pTone = get ("delayTone");    pMod = get ("delayMod");          pWow = get ("delayWow");
    pStereo = get ("delayStereo"); pDLow = get ("delayLowCut");     pDHigh = get ("delayHighCut");
    pDDuck = get ("delayDuck");
    pRevOn = get ("revOn");       pDecay = get ("revDecay");        pPre = get ("revPre");
    pRevMix = get ("revMix");     pType = get ("revType");          pSize = get ("revSize");
    pRevDamp = get ("revDamp");   pWidth = get ("revWidth");
    pRLow = get ("revLowCut");    pRHigh = get ("revHighCut");      pRDuck = get ("revDuck");
    pFreeze = get ("revFreeze");  pRouting = get ("routing");       pOutGain = get ("outGain");
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
    for (int c = 0; c < 2; ++c) lpState[c] = hcState[c] = hpState[c] = rHpState[c] = rLpState[c] = 0.0f;
    phMod = phWow = phFlut = 0.0;
    duckEnv = 0.0f;

    fdn.prepare (sampleRate);
    lex.prepare (sampleRate);
    engine = bWantedEngine = (pType->load() > 3.5f) ? 0 : 1;
    sEngine.reset (sampleRate, 0.04); sEngine.setCurrentAndTargetValue (1.0f);

    sTime.reset (sampleRate, 0.12);   sTime.setCurrentAndTargetValue (pDelayTime->load());
    sFb.reset (sampleRate, 0.03);     sFb.setCurrentAndTargetValue (pFeedback->load() * 0.01f);
    sDMix.reset (sampleRate, 0.03);   sDMix.setCurrentAndTargetValue (pDelayMix->load() * 0.01f);
    sDGate.reset (sampleRate, 0.03);  sDGate.setCurrentAndTargetValue (pDelayOn->load() > 0.5f ? 1.0f : 0.0f);
    sStereo.reset (sampleRate, 0.05); sStereo.setCurrentAndTargetValue (pStereo->load() * 0.01f);
    sRMix.reset (sampleRate, 0.03);   sRMix.setCurrentAndTargetValue (pRevMix->load() * 0.01f);
    sRGate.reset (sampleRate, 0.03);  sRGate.setCurrentAndTargetValue (pRevOn->load() > 0.5f ? 1.0f : 0.0f);
    sWidth.reset (sampleRate, 0.03);  sWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    sOut.reset (sampleRate, 0.03);    sOut.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutGain->load()));
}

// ------------------------------------------------------------------ delay stage
void DelayVerbProcessor::runDelay (float& l, float& r, float duckGain)
{
    const float t = sTime.getNextValue(), fb = sFb.getNextValue();
    const float dm = sDMix.getNextValue(), gate = sDGate.getNextValue(), st = sStereo.getNextValue();
    const double sr = currentSampleRate;

    phMod += bIncMod;   if (phMod >= 1.0) phMod -= 1.0;
    phWow += bIncWow;   if (phWow >= 1.0) phWow -= 1.0;
    phFlut += bIncFlut; if (phFlut >= 1.0) phFlut -= 1.0;

    const float wowSin = std::sin ((float) (MathConstants<double>::twoPi * phWow));
    const float flutSin = std::sin ((float) (MathConstants<double>::twoPi * phFlut));
    const float baseSamples = t * 0.001f * (float) sr;

    float delayed[2], f[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        const float modSin = std::sin ((float) (MathConstants<double>::twoPi * (phMod + 0.25 * ch)));
        const float modMs = bModAmt * 2.5f * modSin + bWowAmt * (1.2f * wowSin + 0.12f * flutSin);
        const float dSamples = jlimit (2.0f, bMaxDelay, baseSamples + modMs * 0.001f * (float) sr);

        delayed[ch] = delayLine.popSample (ch, dSamples);

        // feedback path: mode filter -> high cut -> low cut
        lpState[ch] += bLpA * (delayed[ch] - lpState[ch]);
        hcState[ch] += bHcA * (lpState[ch] - hcState[ch]);
        hpState[ch] += bHpA * (hcState[ch] - hpState[ch]);
        f[ch] = hcState[ch] - hpState[ch];
    }

    // ping-pong: blend straight feedback into cross-fed feedback, and sum the input to the left line
    const float mono = 0.5f * (l + r);
    const float injL = l * (1.0f - st) + mono * st;
    const float injR = r * (1.0f - st);
    float wL = injL * gate + fb * (f[0] * (1.0f - st) + f[1] * st);
    float wR = injR * gate + fb * (f[1] * (1.0f - st) + f[0] * st);

    // saturation on everything written to the line (input + repeats), unity gain for small signals
    if (bSatA > 1.001f)
    {
        wL = std::tanh (bSatA * wL) / bSatA;
        wR = std::tanh (bSatA * wR) / bSatA;
    }
    delayLine.pushSample (0, wL);
    delayLine.pushSample (1, wR);

    const float wetG = dm * gate * duckGain * bMakeup;
    l = l + tone[0].process (delayed[0]) * wetG;
    r = r + tone[1].process (delayed[1]) * wetG;
}

// ------------------------------------------------------------------ reverb stage
void DelayVerbProcessor::runReverb (float& l, float& r, float duckGain)
{
    const float rGate = sRGate.getNextValue(), rMix = sRMix.getNextValue(), width = sWidth.getNextValue();

    float in[2] = { l * rGate, r * rGate };
    for (int ch = 0; ch < 2; ++ch)
    {
        rHpState[ch] += bRHpA * (in[ch] - rHpState[ch]);      // low cut
        in[ch] -= rHpState[ch];
        rLpState[ch] += bRLpA * (in[ch] - rLpState[ch]);      // high cut
        in[ch] = rLpState[ch];
    }

    // switching between the FDN and Dattorro engines fades the wet signal out, swaps, and fades back in
    if (engine != bWantedEngine && sEngine.getCurrentValue() <= 0.0005f)
    {
        engine = bWantedEngine;
        if (engine == 0) fdn.reset(); else lex.reset();
        sEngine.setTargetValue (1.0f);
    }
    const float eg = sEngine.getNextValue();

    float wl, wr;
    if (engine == 0) fdn.process (in[0], in[1], wl, wr);
    else             lex.process (in[0], in[1], wl, wr);

    const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
    wl = mid + side;
    wr = mid - side;

    const float dryG = std::cos (rMix * MathConstants<float>::halfPi);
    const float wetG = std::sin (rMix * MathConstants<float>::halfPi) * rGate * duckGain * eg;
    l = l * dryG + wl * wetG;
    r = r * dryG + wr * wetG;
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
    auto onePole = [sr] (float fc) { return 1.0f - std::exp (-MathConstants<float>::twoPi * jmin (fc, 0.45f * (float) sr) / (float) sr); };

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
        double feel = 1.0;
        if (pDotted->load() > 0.5f) feel = 1.5;
        else if (pTriplet->load() > 0.5f) feel = 2.0 / 3.0;
        timeMs = (float) (60000.0 / bpm * beats[div] * feel);
    }
    timeMs = jlimit (10.0f, 4500.0f, timeMs);

    sTime.setTargetValue (timeMs);
    sFb.setTargetValue (pFeedback->load() * 0.01f);
    sDMix.setTargetValue (pDelayMix->load() * 0.01f);
    sDGate.setTargetValue (dOn ? 1.0f : 0.0f);
    sStereo.setTargetValue (pStereo->load() * 0.01f);

    const int mode = jlimit (0, 2, (int) pMode->load());
    const float modeCut = mode == 0 ? 18000.0f : (mode == 1 ? 4500.0f : 3000.0f);
    const float modeDrive = mode == 0 ? 1.0f : (mode == 1 ? 1.8f : 1.2f);
    bSatA = modeDrive * (1.0f + 7.0f * pSat->load() * 0.01f);
    bMakeup = 1.0f + 0.065f * (bSatA - 1.0f);        // keeps level roughly constant as saturation compresses
    bLpA = onePole (modeCut);
    bHcA = onePole (pDHigh->load());
    bHpA = onePole (pDLow->load());

    const float toneDb = pTone->load();
    if (toneDb != lastToneDb)
    {
        for (auto& t : tone) t.setHighShelf (sr, 2500.0f, toneDb);
        lastToneDb = toneDb;
    }

    bModAmt = pMod->load() * 0.01f;
    bWowAmt = pWow->load() * 0.01f;
    bIncMod = 0.7 / sr; bIncWow = 0.55 / sr; bIncFlut = 6.5 / sr;
    bMaxDelay = (float) delayLine.getMaximumDelayInSamples() - 4.0f;

    // ---- reverb params
    sRMix.setTargetValue (pRevMix->load() * 0.01f);
    sRGate.setTargetValue (pRevOn->load() > 0.5f ? 1.0f : 0.0f);
    sWidth.setTargetValue (pWidth->load() * 0.01f);
    bRHpA = onePole (pRLow->load());
    bRLpA = onePole (pRHigh->load());
    {
        const int type = jlimit (0, 4, (int) pType->load());
        const bool freeze = pFreeze->load() > 0.5f;
        bWantedEngine = type == 4 ? 0 : 1;
        sEngine.setTargetValue (engine == bWantedEngine ? 1.0f : 0.0f);
        fdn.setParams (pDecay->load(), pRevDamp->load() * 0.01f, pSize->load() * 0.01f, 2, pPre->load(), freeze);
        lex.setParams (pDecay->load(), pRevDamp->load() * 0.01f, pSize->load() * 0.01f, jmin (type, 3), pPre->load(), freeze);
    }

    // ---- ducking + routing + output gain
    const float dDuck = pDDuck->load() * 0.01f, rDuck = pRDuck->load() * 0.01f;
    const int routing = jlimit (0, 2, (int) pRouting->load());
    const float attack = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
    const float release = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
    sOut.setTargetValue (Decibels::decibelsToGain (pOutGain->load()));

    auto* chL = buffer.getWritePointer (0);
    auto* chR = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        float l = chL[i], r = chR[i];

        const float level = jmax (std::abs (l), std::abs (r));
        duckEnv += (level - duckEnv) * (level > duckEnv ? attack : release);
        const float duckAmt = jlimit (0.0f, 1.0f, duckEnv * 5.0f);
        const float dg = 1.0f - dDuck * duckAmt, rg = 1.0f - rDuck * duckAmt;

        if (routing == 0)      { runDelay (l, r, dg); runReverb (l, r, rg); }
        else if (routing == 1) { runReverb (l, r, rg); runDelay (l, r, dg); }
        else
        {
            float dl = l, dr = r;
            runDelay (dl, dr, dg);                 // dl - l = delay wet
            float rl = l, rr = r;
            runReverb (rl, rr, rg);                // reverb with its own dry
            l = rl + (dl - l);
            r = rr + (dr - r);
        }

        const float g = sOut.getNextValue();
        chL[i] = l * g;
        chR[i] = r * g;
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
            { "Cathedral",        { { "delayMix", 15 }, { "revType", 3 }, { "revDecay", 9.0f }, { "revSize", 100 },
                                    { "revDamp", 25 }, { "revMix", 45 }, { "revPre", 40 } } },
            { "Dotted Ping-Pong", { { "delaySync", 1 }, { "delayDiv", 3 }, { "delayDotted", 1 }, { "delayStereo", 100 },
                                    { "delayFeedback", 45 }, { "delayLowCut", 200 }, { "delayHighCut", 6000 },
                                    { "delayMix", 35 }, { "revMix", 20 }, { "revDecay", 2.0f } } },
            { "Ducked Vocal",     { { "delayTime", 360 }, { "delayFeedback", 40 }, { "delayMix", 40 }, { "delayDuck", 70 },
                                    { "revDecay", 2.5f }, { "revMix", 30 }, { "revDuck", 50 }, { "revPre", 45 } } },
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
