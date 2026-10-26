#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

// 8-line feedback delay network reverb with input diffusion, per-line damping,
// RT60-based decay, modulated lines, and pre-delay.
class FdnReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxLine = (int) (sr * 0.32) + 16;
        for (auto& l : lines) l.init (maxLine);
        preL.init ((int) (sr * 0.26) + 16);
        preR.init ((int) (sr * 0.26) + 16);

        const float apMsL[2] = { 4.7f, 3.1f }, apMsR[2] = { 5.3f, 3.7f };
        for (int i = 0; i < 2; ++i)
        {
            apL[i].init ((int) (sr * apMsL[i] * 0.001) + 1);
            apR[i].init ((int) (sr * apMsR[i] * 0.001) + 1);
        }

        for (auto& s : lenSmooth) s.reset (sr, 0.10);
        preSmooth.reset (sr, 0.05);

        for (int i = 0; i < N; ++i)
        {
            modInc[i] = (0.11f + 0.037f * (float) i) / (float) sr;
            modPhase[i] = (float) i / (float) N;
        }
        modDepth = 0.0004f * (float) sr;

        reset();
        setParams (2.8f, 0.35f, 0.8f, 1, 30.0f, true);
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
    void setParams (float decaySec, float damp01, float size01, int type, float preMs, bool snap = false)
    {
        static const float table[3][N] = {
            {  7.9f,  9.7f, 11.3f, 13.1f, 15.7f, 17.9f, 19.9f, 23.3f },   // Room
            { 18.7f, 22.1f, 25.7f, 29.3f, 33.1f, 37.9f, 42.7f, 47.9f },   // Plate
            { 37.1f, 43.3f, 51.7f, 59.9f, 68.3f, 77.9f, 89.3f, 101.3f }   // Hall
        };
        static const float apGain[3] = { 0.50f, 0.70f, 0.60f };

        type = juce::jlimit (0, 2, type);
        decaySec = std::max (0.1f, decaySec);
        const float scale = 0.5f + juce::jlimit (0.0f, 1.0f, size01);

        for (int i = 0; i < N; ++i)
        {
            const float lenMs = table[type][i] * scale;
            const float lenSamples = lenMs * 0.001f * (float) sr;
            if (snap) lenSmooth[i].setCurrentAndTargetValue (lenSamples);
            else      lenSmooth[i].setTargetValue (lenSamples);
            gain[i] = std::pow (10.0f, -3.0f * (lenMs * 0.001f) / decaySec);
        }

        const float fc = std::min (20000.0f * std::pow (0.075f, juce::jlimit (0.0f, 1.0f, damp01)),
                                   0.45f * (float) sr);
        lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);

        for (auto& a : apL) a.g = apGain[type];
        for (auto& a : apR) a.g = apGain[type];

        const float pre = juce::jlimit (0.0f, 250.0f, preMs) * 0.001f * (float) sr;
        if (snap) preSmooth.setCurrentAndTargetValue (pre);
        else      preSmooth.setTargetValue (pre);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // pre-delay
        const float pd = std::max (1.0f, preSmooth.getNextValue());
        float xl = preL.read (pd), xr = preR.read (pd);
        preL.write (inL); preR.write (inR);

        // input diffusion
        xl = apL[1].process (apL[0].process (xl));
        xr = apR[1].process (apR[0].process (xr));

        // read + damp + decay gain
        float z[N];
        for (int i = 0; i < N; ++i)
        {
            const float d = lenSmooth[i].getNextValue();
            modPhase[i] += modInc[i];
            if (modPhase[i] >= 1.0f) modPhase[i] -= 1.0f;
            const float m = std::sin (juce::MathConstants<float>::twoPi * modPhase[i]) * modDepth;

            const float y = lines[i].read (std::max (1.0f, d + m));
            lp[i] += lpA * (y - lp[i]);
            z[i] = lp[i] * gain[i];
        }

        // outputs: even lines -> L, odd lines -> R
        outL = outScale * (z[0] + z[2] + z[4] + z[6]);
        outR = outScale * (z[1] + z[3] + z[5] + z[7]);

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

        constexpr float norm = 0.35355339f;   // 1/sqrt(8)
        for (int i = 0; i < N; ++i)
            lines[i].write (h[i] * norm + ((i & 1) == 0 ? xl : xr) * inGain);
    }

    float outScale = 0.85f;
    float inGain = 0.5f;

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
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lenSmooth[N], preSmooth;
    float lp[N] {}, gain[N] {}, modPhase[N] {}, modInc[N] {};
    float lpA = 0.5f, modDepth = 0.0f;
};
