#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Fdn.h"

class DelayVerbProcessor : public juce::AudioProcessor
{
public:
    DelayVerbProcessor();
    ~DelayVerbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DelayVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- presets (internal to the UI; hosts see a single program) ----
    juce::StringArray getPresetNames() const;      // factory (numbered) first, then user presets
    int  getNumFactoryPresets() const;
    int  getCurrentPresetIndex() const { return currentPresetIndex; }
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    void saveUserPreset (const juce::String& name);
    void resetToDefaults();
    juce::File getPresetFolder() const;

    // ---- meters (peak since last read; the editor resets them) ----
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);

    struct Biquad   // RBJ high shelf, transposed direct form II
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void setHighShelf (double sr, float freq, float gainDb);
        void clear() { z1 = z2 = 0.0f; }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 4096 };
    Biquad tone[2];
    float lpState[2] { 0.0f, 0.0f };
    float lastToneDb = 1.0e9f;
    double phMod = 0.0, phWow = 0.0, phFlut = 0.0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sTime, sFb, sDMix, sDGate;

    // reverb
    FdnReverb fdn;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sRMix, sRGate, sWidth;

    double currentSampleRate = 44100.0;

    juce::String presetName { "01 - Ambient Space" };
    int currentPresetIndex = 0;

    std::atomic<float> *pDelayOn, *pDelayTime, *pFeedback, *pDelayMix, *pSync, *pDiv, *pTriplet,
                       *pMode, *pTone, *pMod, *pWow,
                       *pRevOn, *pDecay, *pPre, *pRevMix, *pType, *pSize, *pRevDamp, *pWidth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayVerbProcessor)
};
