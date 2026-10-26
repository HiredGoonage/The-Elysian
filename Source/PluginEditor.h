#pragma once
#include "PluginProcessor.h"
#include "ElysianUI.h"

class DelayVerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DelayVerbEditor (DelayVerbProcessor&);
    ~DelayVerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob   { juce::Slider s; juce::Label title; std::unique_ptr<SliderAtt> att; };
    struct Toggle { juce::ToggleButton b; std::unique_ptr<ButtonAtt> att; };
    struct Combo  { juce::ComboBox c; std::unique_ptr<ComboAtt> att; };

    void setupKnob (Knob&, const juce::String& id, const juce::String& title, juce::Colour accent);
    void setupToggle (Toggle&, const juce::String& id, const char* kind, juce::Colour accent);
    void setupCombo (Combo&, const juce::String& id, const juce::StringArray& items);
    void placeKnob (Knob&, float k, float cx, float cy, float dial, float boxW, float boxH, float titleY);

    void timerCallback() override;
    void tapTempo();
    void stepPreset (int delta);
    void showPresetMenu();
    void showMoreMenu();
    void showSettingsMenu();
    void promptSavePreset();

    DelayVerbProcessor& proc;
    elysian::LookAndFeel lnf;

    // delay
    Knob dTime, dFeedback, dMix, dSat, dTone, dMod, dWow, dStereo, dLow, dHigh, dDuck;
    Toggle dPower, dSync, dTriplet, dDotted;
    Combo dDiv, dMode;
    elysian::IconButton tapButton { elysian::IconButton::Tap, elysian::delayAccent };

    // reverb
    Knob rDecay, rPre, rMix, rSize, rDamp, rWidth, rLow, rHigh, rDuck;
    Toggle rPower, rFreeze;
    Combo rType, rRouting;

    // header
    juce::Slider gainKnob;
    std::unique_ptr<SliderAtt> gainAtt;

    elysian::EchoView echoView;
    elysian::DecayView decayView;

    juce::ArrowButton prevBtn { "prev", 0.5f, juce::Colour (0xffd0d0d0) };
    juce::ArrowButton nextBtn { "next", 0.0f, juce::Colour (0xffd0d0d0) };
    juce::TextButton presetBtn;
    elysian::IconButton saveBtn { elysian::IconButton::Save };
    elysian::IconButton moreBtn { elysian::IconButton::Dots };
    elysian::IconButton gearBtn { elysian::IconButton::Gear };

    float inDisplay = 0.0f, outDisplay = 0.0f;
    bool lastSync = false;
    juce::String lastPresetText;
    std::vector<double> tapTimes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayVerbEditor)
};
