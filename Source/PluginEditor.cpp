#include "PluginEditor.h"

using namespace juce;
using elysian::makeFont;

namespace
{
    constexpr float refW = 1774.0f, refH = 1075.0f;

    float levelToUi (float peak)
    {
        if (peak <= 1.0e-5f) return 0.0f;
        return jlimit (0.0f, 1.0f, (20.0f * std::log10 (peak) + 60.0f) / 60.0f);
    }
}

// ------------------------------------------------------------------ construction
DelayVerbEditor::DelayVerbEditor (DelayVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      echoView (p.apvts.getRawParameterValue ("delayFeedback"), p.apvts.getRawParameterValue ("delayOn")),
      decayView (p.apvts.getRawParameterValue ("revDecay"), p.apvts.getRawParameterValue ("revPre"),
                 p.apvts.getRawParameterValue ("revOn"))
{
    setLookAndFeel (&lnf);

    const auto dc = elysian::delayAccent, rc = elysian::reverbAccent;

    setupKnob (dTime,     "delayTime",     "TIME",       dc);
    setupKnob (dFeedback, "delayFeedback", "FEEDBACK",   dc);
    setupKnob (dMix,      "delayMix",      "MIX",        dc);
    setupKnob (dSat,      "delaySat",      "SATURATION", dc);
    setupKnob (dTone,     "delayTone",     "TONE",       dc);
    setupKnob (dMod,      "delayMod",      "MOD",        dc);
    setupKnob (dWow,      "delayWow",      "WOW/FLUTTER", dc);
    setupKnob (dStereo,   "delayStereo",   "PING-PONG",  dc);
    setupKnob (dLow,      "delayLowCut",   "LOW CUT",    dc);
    setupKnob (dHigh,     "delayHighCut",  "HIGH CUT",   dc);
    setupKnob (dDuck,     "delayDuck",     "DUCK",       dc);
    setupToggle (dPower,   "delayOn",      "power",  dc);
    setupToggle (dSync,    "delaySync",    "switch", dc);
    setupToggle (dTriplet, "delayTriplet", "switch", dc);
    setupToggle (dDotted,  "delayDotted",  "switch", dc);
    dTriplet.b.onClick = [this] { if (dTriplet.b.getToggleState()) dDotted.b.setToggleState (false, sendNotificationSync); };
    dDotted.b.onClick  = [this] { if (dDotted.b.getToggleState()) dTriplet.b.setToggleState (false, sendNotificationSync); };
    setupCombo (dDiv,  "delayDiv",  { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" });
    setupCombo (dMode, "delayMode", { "Digital", "Tape", "Analog" });

    setupKnob (rDecay, "revDecay", "DECAY",     rc);
    setupKnob (rPre,   "revPre",   "PRE-DELAY", rc);
    setupKnob (rMix,   "revMix",   "MIX",       rc);
    setupKnob (rSize,  "revSize",  "SIZE",      rc);
    setupKnob (rDamp,  "revDamp",  "DAMPING",   rc);
    setupKnob (rWidth, "revWidth", "WIDTH",     rc);
    setupKnob (rLow,  "revLowCut",  "LOW CUT",  rc);
    setupKnob (rHigh, "revHighCut", "HIGH CUT", rc);
    setupKnob (rDuck, "revDuck",    "DUCK",     rc);
    setupToggle (rPower, "revOn", "power", rc);
    setupToggle (rFreeze, "revFreeze", "switch", rc);
    setupCombo (rType, "revType", { "Room", "Plate", "Hall", "Cathedral", "Smooth" });
    setupCombo (rRouting, "routing", { "Delay > Reverb", "Reverb > Delay", "Parallel" });

    gainKnob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    gainKnob.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    gainKnob.setPopupDisplayEnabled (true, true, this);
    elysian::setAccent (gainKnob, Colour (0xffd9d9dc));
    addAndMakeVisible (gainKnob);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, "outGain", gainKnob);

    addAndMakeVisible (tapButton);
    tapButton.setTooltip ("Tap tempo");
    tapButton.onClick = [this] { tapTempo(); };

    addAndMakeVisible (echoView);
    addAndMakeVisible (decayView);

    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (presetBtn);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (moreBtn);
    addAndMakeVisible (gearBtn);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setColour (TextButton::buttonColourId, Colours::transparentBlack);
    presetBtn.setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    saveBtn.onClick = [this] { promptSavePreset(); };
    moreBtn.onClick = [this] { showMoreMenu(); };
    gearBtn.onClick = [this] { showSettingsMenu(); };

    setResizable (true, true);
    setResizeLimits (600, 300, 2400, 1200);
    getConstrainer()->setFixedAspectRatio ((double) refW / (double) refH);
    {
        const int w = jlimit (600, 2400, (int) proc.apvts.state.getProperty ("uiWidth", 1000));
        setSize (w, roundToInt ((float) w * refH / refW));
    }

    startTimerHz (30);
}

DelayVerbEditor::~DelayVerbEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DelayVerbEditor::setupKnob (Knob& k, const String& id, const String& title, Colour accent)
{
    k.s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.s.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 22);
    elysian::setAccent (k.s, accent);

    k.title.setText (title, dontSendNotification);
    k.title.setJustificationType (Justification::centred);
    k.title.setColour (Label::textColourId, accent == elysian::delayAccent ? Colour (0xffd9cfc0) : Colour (0xffc9dcea));
    k.title.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.s);
    addAndMakeVisible (k.title);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.s);
}

void DelayVerbEditor::setupToggle (Toggle& t, const String& id, const char* kind, Colour accent)
{
    t.b.setClickingTogglesState (true);
    t.b.getProperties().set ("kind", kind);
    elysian::setAccent (t.b, accent);
    addAndMakeVisible (t.b);
    t.att = std::make_unique<ButtonAtt> (proc.apvts, id, t.b);
}

void DelayVerbEditor::setupCombo (Combo& c, const String& id, const StringArray& items)
{
    c.c.addItemList (items, 1);
    addAndMakeVisible (c.c);
    c.att = std::make_unique<ComboAtt> (proc.apvts, id, c.c);
}

// ------------------------------------------------------------------ painting
void DelayVerbEditor::paint (Graphics& g)
{
    const float k = (float) getWidth() / refW;
    auto R = [k] (float x, float y, float w, float h) { return Rectangle<float> (x * k, y * k, w * k, h * k); };
    auto text = [&] (const String& s, Rectangle<float> r, Font f, Colour c, Justification j)
    {
        g.setFont (f);
        g.setColour (c);
        g.drawText (s, r, j, false);
    };

    g.fillAll (Colour (0xff08090a));

    // ---- header
    {
        auto r = R (5, 18, 1765, 107);
        g.setGradientFill (ColourGradient (Colour (0xff18191c), r.getX(), r.getY(), Colour (0xff0e0f11), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 12 * k);
        g.setColour (Colour (0xff25272b));
        g.drawRoundedRectangle (r, 12 * k, 1.3f);

        text ("THE ELYSIAN", R (45, 38, 520, 56), makeFont (42 * k, false, 0.32f), Colour (0xffe2e2e2), Justification::centredLeft);
        text ("DELAY  /  REVERB", R (77, 80, 420, 26), makeFont (19 * k, false, 0.30f), Colour (0xff8c8c90), Justification::centredLeft);

        // preset box
        auto pb = R (600, 47, 487, 50);
        g.setColour (Colour (0xff131417));
        g.fillRoundedRectangle (pb, 8 * k);
        g.setColour (Colour (0xff2c2e33));
        g.drawRoundedRectangle (pb, 8 * k, 1.3f);
        g.drawLine (643 * k, 47 * k, 643 * k, 97 * k, 1.0f);
        g.drawLine (1043 * k, 47 * k, 1043 * k, 97 * k, 1.0f);

        // meters
        auto meter = [&] (const String& label, float lx, float bx, float bw, float level)
        {
            text (label, R (lx, 58, 60, 30), makeFont (20 * k, false, 0.10f), Colour (0xffb4b4b8), Justification::centredLeft);
            auto bar = R (bx, 66, bw, 14);
            g.setColour (Colour (0xff1c1e21));
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
            auto fill = bar.withWidth (bar.getWidth() * level);
            if (fill.getWidth() > 1.0f)
            {
                const auto green = Colour (0xff35e0a0);
                g.setColour (green.withAlpha (0.18f));
                g.fillRoundedRectangle (fill.expanded (0.0f, 3.0f * k), bar.getHeight());
                g.setGradientFill (ColourGradient (green.darker (0.2f), fill.getX(), 0.0f, green.brighter (0.4f), fill.getRight(), 0.0f, false));
                g.fillRoundedRectangle (fill, bar.getHeight() * 0.5f);
            }
        };
        meter ("IN",  1300, 1338, 120, inDisplay);
        meter ("OUT", 1496, 1548, 117, outDisplay);
    }

    // ---- panels
    auto dp = R (5, 130, 865, 925), rp = R (905, 130, 865, 925);
    g.setGradientFill (ColourGradient (Colour (0xff1d1712), dp.getX(), dp.getY(), Colour (0xff0d0b0a), dp.getX(), dp.getBottom(), false));
    g.fillRoundedRectangle (dp, 14 * k);
    g.setColour (elysian::delayAccent.withAlpha (0.10f));
    g.drawRoundedRectangle (dp, 14 * k, 1.3f);

    g.setGradientFill (ColourGradient (Colour (0xff0b1c28), rp.getX(), rp.getY(), Colour (0xff090e12), rp.getX(), rp.getBottom(), false));
    g.fillRoundedRectangle (rp, 14 * k);
    g.setColour (elysian::reverbAccent.withAlpha (0.12f));
    g.drawRoundedRectangle (rp, 14 * k, 1.3f);

    // divider + signal-flow arrow
    g.setColour (Colour (0xff2a2b2e));
    g.drawLine (887 * k, 155 * k, 887 * k, 408 * k, 1.5f);
    g.drawLine (887 * k, 470 * k, 887 * k, 1030 * k, 1.5f);
    g.setColour (Colour (0xff17181b));
    g.fillEllipse (R (860, 411, 54, 54));
    g.setColour (Colour (0xff303236));
    g.drawEllipse (R (860, 411, 54, 54), 1.3f);
    g.setColour (Colour (0xffe6e6e6));
    g.drawArrow (Line<float> (875 * k, 438 * k, 900 * k, 438 * k), 2.2f, 11 * k, 10 * k);

    // headings + glowing rules
    auto rule = [&] (float x1, float x2, float y, Colour c)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.05f * (float) (4 - i)));
            g.drawLine (x1 * k, y * k, x2 * k, y * k, (2.0f + 3.0f * (float) i) * k);
        }
        g.setColour (c);
        g.drawLine (x1 * k, y * k, x2 * k, y * k, jmax (1.2f, 2.0f * k));
    };
    text ("DELAY",  R (57, 155, 300, 52),  makeFont (38 * k, true, 0.38f), Colour (0xffe9e1d3), Justification::centredLeft);
    text ("REVERB", R (975, 155, 300, 52), makeFont (38 * k, true, 0.38f), Colour (0xff62d2ff), Justification::centredLeft);
    rule (240, 755, 180, elysian::delayAccent);
    rule (1225, 1660, 180, elysian::reverbAccent);

    // delay: sync column
    const auto labelCol = Colour (0xffcfc7ba);
    g.setColour (Colour (0xff3a342d));
    g.drawLine (664 * k, 262 * k, 664 * k, 458 * k, 1.3f);
    text ("SYNC",    R (686, 255, 120, 30), makeFont (21 * k, false, 0.10f), labelCol, Justification::centredLeft);
    text ("TRIPLET", R (686, 394, 100, 30), makeFont (19 * k, false, 0.05f), labelCol, Justification::centredLeft);
    text ("DOTTED",  R (686, 428, 100, 30), makeFont (19 * k, false, 0.05f), labelCol, Justification::centredLeft);
    text ("MODE",    R (65, 516, 120, 30),  makeFont (21 * k, false, 0.10f), labelCol, Justification::centredLeft);
    text ("TYPE",    R (968, 516, 120, 30), makeFont (21 * k, false, 0.10f), Colour (0xffc9dcea), Justification::centredLeft);
    text ("ROUTING", R (968, 676, 140, 30), makeFont (21 * k, false, 0.10f), Colour (0xffc9dcea), Justification::centredLeft);
    text ("FREEZE",  R (968, 786, 110, 30), makeFont (21 * k, false, 0.10f), Colour (0xffc9dcea), Justification::centredLeft);
    text ("GAIN",    R (1190, 98, 92, 22), makeFont (17 * k, false, 0.10f), Colour (0xff8c8c90), Justification::centred);
}

// ------------------------------------------------------------------ layout
void DelayVerbEditor::placeKnob (Knob& kn, float k, float cx, float cy, float dial, float boxW, float boxH, float titleY)
{
    const float sw = dial + 30.0f;
    kn.s.setBounds (Rectangle<float> ((cx - sw * 0.5f) * k, (cy - dial * 0.5f) * k, sw * k, (dial + boxH) * k).toNearestInt());
    kn.s.setTextBoxStyle (Slider::TextBoxBelow, false, roundToInt (boxW * k), roundToInt (boxH * k));
    kn.title.setBounds (Rectangle<float> ((cx - 90.0f) * k, (titleY - 15.0f) * k, 180.0f * k, 30.0f * k).toNearestInt());
    kn.title.setFont (makeFont (21.0f * k, false, 0.10f));
}

void DelayVerbEditor::resized()
{
    const float k = (float) getWidth() / refW;
    auto R = [k] (float x, float y, float w, float h)
    {
        return Rectangle<float> (x * k, y * k, w * k, h * k).toNearestInt();
    };
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);

    // delay
    placeKnob (dTime,     k, 155, 343, 150, 100, 34, 249);
    placeKnob (dFeedback, k, 360, 341, 140, 100, 30, 249);
    placeKnob (dMix,      k, 560, 341, 140, 100, 30, 249);
    placeKnob (dSat,      k, 338, 573, 105,  90, 30, 506);
    placeKnob (dTone,     k, 484, 573, 105,  90, 30, 506);
    placeKnob (dMod,      k, 630, 573, 105,  90, 30, 506);
    placeKnob (dWow,      k, 775, 573, 105,  90, 30, 506);

    placeKnob (dStereo, k, 155, 770, 105, 90, 30, 705);
    placeKnob (dLow,    k, 348, 770, 105, 90, 30, 705);
    placeKnob (dHigh,   k, 527, 770, 105, 90, 30, 705);
    placeKnob (dDuck,   k, 733, 770, 105, 90, 30, 705);

    dPower.b.setBounds (R (771, 153, 56, 56));
    dSync.b.setBounds (R (687, 292, 66, 30));
    dDiv.c.setBounds (R (687, 341, 148, 42));
    dTriplet.b.setBounds (R (771, 396, 44, 27));
    dDotted.b.setBounds (R (771, 430, 44, 27));
    dMode.c.setBounds (R (65, 552, 160, 45));
    tapButton.setBounds (R (211, 417, 38, 38));

    // reverb
    placeKnob (rDecay, k, 1089, 343, 150, 100, 34, 249);
    placeKnob (rPre,   k, 1325, 343, 145, 100, 30, 249);
    placeKnob (rMix,   k, 1571, 343, 145, 100, 30, 249);
    placeKnob (rSize,  k, 1265, 573, 105,  90, 30, 496);
    placeKnob (rDamp,  k, 1443, 573, 105,  90, 30, 496);
    placeKnob (rWidth, k, 1627, 573, 105,  90, 30, 496);

    placeKnob (rLow,  k, 1265, 770, 105, 90, 30, 705);
    placeKnob (rHigh, k, 1443, 770, 105, 90, 30, 705);
    placeKnob (rDuck, k, 1627, 770, 105, 90, 30, 705);

    rPower.b.setBounds (R (1677, 153, 56, 56));
    rType.c.setBounds (R (968, 552, 197, 45));
    rRouting.c.setBounds (R (968, 710, 197, 45));
    rFreeze.b.setBounds (R (1100, 784, 66, 30));

    gainKnob.setBounds (R (1209, 38, 62, 62));

    echoView.setBounds (R (45, 880, 798, 150));
    decayView.setBounds (R (939, 880, 789, 150));

    // header
    prevBtn.setBounds (R (610, 62, 22, 20).withSizeKeepingCentre (roundToInt (16 * k), roundToInt (22 * k)));
    nextBtn.setBounds (R (1054, 62, 22, 20).withSizeKeepingCentre (roundToInt (16 * k), roundToInt (22 * k)));
    presetBtn.setBounds (R (645, 47, 396, 50));
    saveBtn.setBounds (R (1095, 47, 46, 50));
    moreBtn.setBounds (R (1143, 47, 32, 50));
    gearBtn.setBounds (R (1690, 44, 54, 54));
}

// ------------------------------------------------------------------ behaviour
void DelayVerbEditor::timerCallback()
{
    inDisplay  = jmax (levelToUi (proc.inPeak.exchange (0.0f)),  inDisplay * 0.86f);
    outDisplay = jmax (levelToUi (proc.outPeak.exchange (0.0f)), outDisplay * 0.86f);

    const float k = (float) getWidth() / refW;
    repaint (Rectangle<float> (1290 * k, 50 * k, 390 * k, 46 * k).toNearestInt());

    echoView.repaint();
    decayView.repaint();

    const bool sync = dSync.b.getToggleState();
    if (sync != lastSync)
    {
        lastSync = sync;
        dTime.s.setEnabled (! sync);
        dTime.s.setAlpha (sync ? 0.4f : 1.0f);
    }

    const auto name = proc.getPresetName();
    if (name != lastPresetText)
    {
        lastPresetText = name;
        presetBtn.setButtonText (name);
    }
}

void DelayVerbEditor::tapTempo()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (! tapTimes.empty() && now - tapTimes.back() > 2500.0) tapTimes.clear();
    tapTimes.push_back (now);
    if (tapTimes.size() > 5) tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 2)
    {
        const double avg = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
        if (auto* sync = proc.apvts.getParameter ("delaySync")) sync->setValueNotifyingHost (0.0f);
        if (auto* t = proc.apvts.getParameter ("delayTime"))
            t->setValueNotifyingHost (t->convertTo0to1 ((float) jlimit (10.0, 2000.0, avg)));
    }
}

void DelayVerbEditor::stepPreset (int delta)
{
    const int n = proc.getPresetNames().size();
    if (n == 0) return;
    const int cur = proc.getCurrentPresetIndex() < 0 ? 0 : proc.getCurrentPresetIndex();
    proc.loadPreset ((cur + delta + n) % n);
}

void DelayVerbEditor::showPresetMenu()
{
    PopupMenu m;
    const auto names = proc.getPresetNames();
    const int nf = proc.getNumFactoryPresets();
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == nf) m.addSeparator();
        m.addItem (i + 1, names[i], true, i == proc.getCurrentPresetIndex());
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn).withMinimumWidth (presetBtn.getWidth()),
                     [safe = Component::SafePointer<DelayVerbEditor> (this)] (int r)
                     {
                         if (safe != nullptr && r > 0) safe->proc.loadPreset (r - 1);
                     });
}

void DelayVerbEditor::showMoreMenu()
{
    PopupMenu m;
    m.addItem (1, "Reset all to defaults");
    m.addItem (2, "Open preset folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&moreBtn),
                     [safe = Component::SafePointer<DelayVerbEditor> (this)] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == 1) safe->proc.resetToDefaults();
                         if (r == 2) safe->proc.getPresetFolder().revealToUser();
                     });
}

void DelayVerbEditor::showSettingsMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Window size");
    const float scales[4] = { 0.75f, 1.0f, 1.25f, 1.5f };
    for (int i = 0; i < 4; ++i)
        m.addItem (i + 1, String (roundToInt (scales[i] * 100.0f)) + " %");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&gearBtn),
                     [safe = Component::SafePointer<DelayVerbEditor> (this), scales] (int r)
                     {
                         if (safe != nullptr && r > 0)
                             safe->setSize (roundToInt (1000.0f * scales[r - 1]), roundToInt (1000.0f * scales[r - 1] * refH / refW));
                     });
}

void DelayVerbEditor::promptSavePreset()
{
    auto* w = new AlertWindow ("Save preset", "Preset name:", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true,
                        ModalCallbackFunction::create ([safe = Component::SafePointer<DelayVerbEditor> (this), w] (int result)
                        {
                            if (result == 1 && safe != nullptr)
                                safe->proc.saveUserPreset (w->getTextEditorContents ("name"));
                        }),
                        true);
}
