#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

// 16-line feedback delay network reverb:
//   pre-delay -> early reflections (tapped from the pre-delayed input)
//             -> input diffusion -> FDN (Hadamard mix, per-line damping, RT60 decay, modulated lines)
// plus a freeze mode that holds the tail indefinitely.
class FdnReverb
{
public:
    static constexpr int N = 16;
    static constexpr int numER = 6;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxLine = (int) (sr * 0.34) + 16;
        for (auto& l : lines) l.init (maxLine);
        preL.init ((int) (sr * 0.45) + 16);
        preR.init ((int) (sr * 0.45) + 16);

        const float apMsL[2] = { 4.7f, 3.1f }, apMsR[2] = { 5.3f, 3.7f };
        for (int i = 0; i < 2; ++i)
        {
            apL[i].init ((int) (sr * apMsL[i] * 0.001) + 1);
            apR[i].init ((int) (sr * apMsR[i] * 0.001) + 1);
        }

        for (auto& s : lenSmooth) s.reset (sr, 0.10);
        for (auto& s : erSmooth) s.reset (sr, 0.10);
        preSmooth.reset (sr, 0.05);
        freezeSm.reset (sr, 0.08);

        for (int i = 0; i < N; ++i)
        {
            modInc[i] = (0.09f + 0.031f * (float) i) / (float) sr;
            modPhase[i] = (float) i / (float) N;
        }
        modDepth = 0.0004f * (float) sr;

        reset();
        setParams (2.8f, 0.35f, 0.8f, 1, 30.0f, false, true);
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        preL.clear(); preR.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (int i = 0; i < N; ++i) lp[i] = 0.0f;
    }

    // type: 0 Room, 1 Plate, 2 Hall
    void setParams (float decaySec, float damp01, float size01, int type, float preMs,
                    bool freeze, bool snap = false)
    {
        static const float lo[3] = { 7.0f, 16.0f, 32.0f };
        static const float hi[3] = { 34.0f, 62.0f, 118.0f };
        static const float jit[N] = { 1.00f, 1.02f, 0.98f, 1.03f, 0.99f, 1.01f, 0.97f, 1.02f,
                                      1.00f, 0.98f, 1.03f, 0.99f, 1.01f, 0.97f, 1.02f, 0.98f };
        static const float er[3][numER][2] = {
            { { 5.3f, 0.70f }, { 8.9f, 0.60f }, { 12.7f, 0.50f }, { 17.3f, 0.40f }, { 23.1f, 0.30f }, { 29.9f, 0.25f } },
            { { 7.1f, 0.60f }, { 11.3f, 0.55f }, { 16.7f, 0.50f }, { 23.9f, 0.40f }, { 31.3f, 0.30f }, { 41.0f, 0.25f } },
            { { 11.0f, 0.60f }, { 19.0f, 0.55f }, { 29.0f, 0.50f }, { 41.0f, 0.45f }, { 57.0f, 0.35f }, { 73.0f, 0.30f } }
        };
        static const float apGain[3] = { 0.50f, 0.70f, 0.60f };

        type = juce::jlimit (0, 2, type);
        decaySec = std::max (0.1f, decaySec);
        const float scale = 0.5f + juce::jlimit (0.0f, 1.0f, size01);

        for (int i = 0; i < N; ++i)
        {
            const float lenMs = lo[type] * std::pow (hi[type] / lo[type], (float) i / (float) (N - 1)) * jit[i] * scale;
            const float lenSamples = lenMs * 0.001f * (float) sr;
            if (snap) lenSmooth[i].setCurrentAndTargetValue (lenSamples);
            else      lenSmooth[i].setTargetValue (lenSamples);
            gain[i] = std::pow (10.0f, -3.0f * (lenMs * 0.001f) / (decaySec * 1.17f));   // 1.17 = measured correction so RT60 matches the knob
        }

        for (int e = 0; e < numER; ++e)
        {
            const float t = er[type][e][0] * scale * 0.001f * (float) sr;
            erGain[e] = er[type][e][1];
            if (snap) erSmooth[e].setCurrentAndTargetValue (t);
            else      erSmooth[e].setTargetValue (t);
        }

        const float fc = std::min (20000.0f * std::pow (0.075f, juce::jlimit (0.0f, 1.0f, damp01)),
                                   0.45f * (float) sr);
        lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);

        for (auto& a : apL) a.g = apGain[type];
        for (auto& a : apR) a.g = apGain[type];

        const float pre = juce::jlimit (0.0f, 250.0f, preMs) * 0.001f * (float) sr;
        if (snap) preSmooth.setCurrentAndTargetValue (pre);
        else      preSmooth.setTargetValue (pre);

        if (snap) freezeSm.setCurrentAndTargetValue (freeze ? 1.0f : 0.0f);
        else      freezeSm.setTargetValue (freeze ? 1.0f : 0.0f);

        // keep the wet level similar across decay times and types
        outLevel = outScale * typeNorm[type] * std::pow (2.8f / decaySec, 0.3f);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        const float fz = freezeSm.getNextValue();
        const float pd = std::max (1.0f, preSmooth.getNextValue());

        // pre-delayed input (read before write)
        const float pl = preL.read (pd), pr = preR.read (pd);

        // early reflections tapped from the pre-delay buffers
        float erL = 0.0f, erR = 0.0f;
        for (int e = 0; e < numER; ++e)
        {
            const float d = pd + erSmooth[e].getNextValue();
            const float a = preL.read (d), b = preR.read (d);
            if ((e & 1) == 0) { erL += erGain[e] * a; erR += erGain[e] * b; }
            else              { erL += erGain[e] * b; erR += erGain[e] * a; }
        }

        preL.write (inL);
        preR.write (inR);

        // input diffusion
        const float xl = apL[1].process (apL[0].process (pl));
        const float xr = apR[1].process (apR[0].process (pr));

        // read + damp + decay
        const float lpEff = lpA + (1.0f - lpA) * fz;
        float z[N];
        for (int i = 0; i < N; ++i)
        {
            const float d = lenSmooth[i].getNextValue();
            modPhase[i] += modInc[i];
            if (modPhase[i] >= 1.0f) modPhase[i] -= 1.0f;
            const float m = std::sin (juce::MathConstants<float>::twoPi * modPhase[i]) * modDepth;

            const float y = lines[i].read (std::max (1.0f, d + m));
            lp[i] += lpEff * (y - lp[i]);
            const float g = gain[i] + (1.0f - gain[i]) * fz;       // frozen: no decay
            z[i] = lp[i] * g;
        }

        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < N; i += 2) { sumL += z[i]; sumR += z[i + 1]; }
        const float live = 1.0f - fz;
        outL = outLevel * sumL + erScale * outLevel * live * erL;
        outR = outLevel * sumR + erScale * outLevel * live * erR;

        // Hadamard mixing (orthogonal, energy preserving)
        float h[N];
        for (int i = 0; i < N; ++i) h[i] = z[i];
        for (int step = 1; step < N; step <<= 1)
            for (int i = 0; i < N; i += step * 2)
                for (int j = i; j < i + step; ++j)
                {
                    const float a = h[j], b = h[j + step];
                    h[j] = a + b;
                    h[j + step] = a - b;
                }

        constexpr float norm = 0.25f;   // 1/sqrt(16)
        const float inj = inGain * live;
        for (int i = 0; i < N; ++i)
            lines[i].write (h[i] * norm + ((i & 1) == 0 ? xl : xr) * inj);
    }

    float outScale = 1.0f;
    float typeNorm[3] = { 0.64f, 0.80f, 1.05f };   // per-type level calibration
    float erScale = 0.35f;
    float inGain = 0.35f;

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void init (int size)
        {
            int s = 1;
            while (s < size) s <<= 1;
            buf.assign ((size_t) s, 0.0f);
            mask = s - 1;
            w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        // value written d samples ago (d >= 1), read before write
        float read (float d) const
        {
            const float rp = (float) w - d;
            const int i = (int) std::floor (rp);
            const float f = rp - (float) i;
            const float a = buf[(size_t) (i & mask)];
            const float b = buf[(size_t) ((i + 1) & mask)];
            return a + f * (b - a);
        }
        void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int w = 0;
        float g = 0.6f;

        void init (int len) { buf.assign ((size_t) std::max (1, len), 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        float process (float x)
        {
            const float d = buf[(size_t) w];
            const float v = x - g * d;
            buf[(size_t) w] = v;
            if (++w >= (int) buf.size()) w = 0;
            return d + g * v;
        }
    };

    double sr = 44100.0;
    Line lines[N];
    Line preL, preR;
    Allpass apL[2], apR[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lenSmooth[N], erSmooth[numER], preSmooth, freezeSm;
    float lp[N] {}, gain[N] {}, modPhase[N] {}, modInc[N] {}, erGain[numER] {};
    float lpA = 0.5f, modDepth = 0.0f, outLevel = 1.0f;
};
