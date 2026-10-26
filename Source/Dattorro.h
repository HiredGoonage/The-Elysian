#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Plate/hall reverb after J. Dattorro, "Effect Design Part 1" (JAES 1997), the open descendant of the
// Griesinger/Lexicon designs: pre-delay -> input diffusion (4 allpasses) -> figure-8 tank of two
// cross-coupled branches (modulated allpass, delay, damping, decay gain, allpass, delay) -> multi-tap output.
// The delay lengths are scaled per type (Room / Plate / Hall / Cathedral) and by the Size knob.
class DattorroReverb
{
public:
    static constexpr int numTypes = 4;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        srScale = (float) (sr / 29761.0);
        const float maxScale = 4.6f;
        auto cap = [&] (float ref) { return (int) (ref * srScale * maxScale) + 64; };

        for (int i = 0; i < 8; ++i) inAp[i].line.init (cap (inLen[i] * 2.0f));
        for (int b = 0; b < 2; ++b)
        {
            modAp[b].line.init (cap (modLen[b]));
            d1[b].init (cap (d1Len[b]));
            ap2[b].line.init (cap (ap2Len[b]));
            d2[b].init (cap (d2Len[b]));
        }
        pre.init ((int) (sr * 0.26) + 16);

        scaleSm.reset (sr, 0.15);
        inScaleSm.reset (sr, 0.15);
        preSm.reset (sr, 0.05);
        freezeSm.reset (sr, 0.08);

        reset();
        setParams (2.8f, 0.35f, 0.5f, 1, 0.0f, false, true);
    }

    void reset()
    {
        for (auto& a : inAp) a.line.clear();
        for (int b = 0; b < 2; ++b) { modAp[b].line.clear(); d1[b].clear(); ap2[b].line.clear(); d2[b].clear(); lp[b] = 0.0f; }
        pre.clear();
        bwState = 0.0f;
        phase[0] = 0.0f; phase[1] = 0.37f;
    }

    // type: 0 Room, 1 Plate, 2 Hall, 3 Cathedral
    void setParams (float decaySec, float damp01, float size01, int type, float preMs, bool freeze, bool snap = false)
    {
        struct TP { float base, inScale, g1, g2, gin1, gin2, gin3, modMs, modHz; };
        static const TP tp[numTypes] = {
            { 0.30f, 0.80f, 0.70f, 0.50f, 0.75f, 0.625f, 0.55f, 0.50f, 0.90f },   // Room
            { 1.00f, 1.00f, 0.70f, 0.50f, 0.75f, 0.625f, 0.55f, 0.50f, 1.00f },   // Plate
            { 1.20f, 1.40f, 0.68f, 0.55f, 0.75f, 0.625f, 0.65f, 0.80f, 0.60f },   // Hall
            { 1.80f, 1.80f, 0.66f, 0.55f, 0.75f, 0.625f, 0.70f, 1.20f, 0.40f }    // Cathedral
        };

        type = juce::jlimit (0, numTypes - 1, type);
        const auto& t = tp[type];
        decaySec = std::max (0.1f, decaySec);
        const float scale = t.base * (0.5f + juce::jlimit (0.0f, 1.0f, size01));

        if (snap) { scaleSm.setCurrentAndTargetValue (scale); inScaleSm.setCurrentAndTargetValue (t.inScale); }
        else      { scaleSm.setTargetValue (scale);           inScaleSm.setTargetValue (t.inScale); }

        for (int i = 0; i < 8; ++i) inAp[i].g = i < 2 ? t.gin1 : (i < 4 ? t.gin2 : t.gin3);
        for (int b = 0; b < 2; ++b) { modAp[b].g = t.g1; ap2[b].g = t.g2; }

        // each branch loses 'g' per pass; the loop goes through both branches
        for (int b = 0; b < 2; ++b)
        {
            const float refSum = modLen[b] + d1Len[b] + ap2Len[b] + d2Len[b];
            const float tSec = refSum * scale / 29761.0f;
            gain[b] = std::pow (10.0f, -3.0f * tSec / (decaySec * decayCorr[type]));
        }

        const float fc = std::min (20000.0f * std::pow (0.075f, juce::jlimit (0.0f, 1.0f, damp01)),
                                   0.45f * (float) sr);
        lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);

        modDepth = t.modMs * 0.001f * (float) sr;
        modInc = t.modHz / (float) sr;

        const float p = juce::jlimit (0.0f, 250.0f, preMs) * 0.001f * (float) sr;
        if (snap) preSm.setCurrentAndTargetValue (p); else preSm.setTargetValue (p);
        if (snap) freezeSm.setCurrentAndTargetValue (freeze ? 1.0f : 0.0f);
        else      freezeSm.setTargetValue (freeze ? 1.0f : 0.0f);

        outLevel = outScale * typeNorm[type] * std::pow (2.8f / decaySec, 0.3f);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        const float fz = freezeSm.getNextValue();
        const float s = scaleSm.getNextValue() * srScale;
        const float is = inScaleSm.getNextValue() * srScale;
        auto D = [] (float v) { return std::max (1.0f, v); };

        // pre-delay, bandwidth, input diffusion
        const float p = D (preSm.getNextValue());
        float x = pre.read (p);
        pre.write (0.5f * (inL + inR));
        bwState += 0.55f * (x - bwState);
        x = bwState;
        for (int i = 0; i < 8; ++i) x = inAp[i].process (x, D (inLen[i] * is));
        x *= (1.0f - fz);

        // tank
        const float lEnd = d2[0].read (D (d2Len[0] * s));
        const float rEnd = d2[1].read (D (d2Len[1] * s));
        const float lpEff = lpA + (1.0f - lpA) * fz;

        for (int b = 0; b < 2; ++b)
        {
            phase[b] += modInc;
            if (phase[b] >= 1.0f) phase[b] -= 1.0f;
            const float lfo = std::sin (juce::MathConstants<float>::twoPi * phase[b]) * modDepth;

            const float in = x + (b == 0 ? rEnd : lEnd);
            const float y1 = modAp[b].process (in, D (modLen[b] * s + lfo));
            const float dv = d1[b].read (D (d1Len[b] * s));
            d1[b].write (y1);

            lp[b] += lpEff * (dv - lp[b]);
            const float g = gain[b] + (1.0f - gain[b]) * fz;
            const float y2 = ap2[b].process (lp[b] * g, D (ap2Len[b] * s));
            d2[b].write (y2);
        }

        auto T = [&] (const Line& l, float off) { return l.read (D (off * s)); };
        const float oL =  T (d1[1], 266) + T (d1[1], 2974) - T (ap2[1].line, 1913) + T (d2[1], 1996)
                        - T (d1[0], 1990) - T (ap2[0].line, 187) - T (d2[0], 1066);
        const float oR =  T (d1[0], 353) + T (d1[0], 3627) - T (ap2[0].line, 1228) + T (d2[0], 2673)
                        - T (d1[1], 2111) - T (ap2[1].line, 335) - T (d2[1], 121);
        // extra taps for a denser early field
        const float eL =  T (d1[0], 761) - T (d2[1], 2297) + T (ap2[1].line, 1031) - T (d1[1], 3433) + T (d2[0], 3137) - T (modAp[0].line, 421);
        const float eR =  T (d1[1], 887) - T (d2[0], 2437) + T (ap2[0].line, 1523) - T (d1[0], 3181) + T (d2[1], 2851) - T (modAp[1].line, 547);
        outL = (oL + 0.7f * eL) * outLevel;
        outR = (oR + 0.7f * eR) * outLevel;
    }

    float outScale = 0.6f;
    float decayCorr[numTypes] = { 1.0532f, 0.9937f, 0.9775f, 1.0066f };   // measured RT60 corrections
    float typeNorm[numTypes]  = { 0.863f, 1.374f, 1.472f, 1.578f };   // per-type level calibration

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void init (int size) { int s = 1; while (s < size) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        float read (float d) const
        {
            const float rp = (float) w - d;
            const int i = (int) std::floor (rp);
            const float f = rp - (float) i;
            const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
            return a + f * (b - a);
        }
        void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    };

    struct Allpass
    {
        Line line;
        float g = 0.6f;
        float process (float x, float delay)
        {
            const float d = line.read (delay);
            const float v = x - g * d;
            line.write (v);
            return d + g * v;
        }
    };

    using Smooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    // reference lengths in samples at 29761 Hz (Dattorro)
    static constexpr float inLen[8]  = { 142.0f, 107.0f, 379.0f, 277.0f, 89.0f, 211.0f, 331.0f, 449.0f };
    static constexpr float modLen[2] = { 672.0f, 908.0f };
    static constexpr float d1Len[2]  = { 4453.0f, 4217.0f };
    static constexpr float ap2Len[2] = { 1800.0f, 2656.0f };
    static constexpr float d2Len[2]  = { 3720.0f, 3163.0f };

    double sr = 44100.0;
    float srScale = 1.0f;
    Allpass inAp[8], modAp[2], ap2[2];
    Line d1[2], d2[2], pre;
    Smooth scaleSm, inScaleSm, preSm, freezeSm;
    float lp[2] {}, gain[2] { 0.9f, 0.9f }, phase[2] { 0.0f, 0.37f };
    float bwState = 0.0f, lpA = 0.5f, modDepth = 0.0f, modInc = 0.0f, outLevel = 0.6f;
};
