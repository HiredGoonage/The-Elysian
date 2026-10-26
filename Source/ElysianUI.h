#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace elysian
{
    static const juce::Colour delayAccent { 0xfff5a742 };
    static const juce::Colour reverbAccent { 0xff4cc3ff };

    inline juce::Font makeFont (float height, bool bold = false, float kerning = 0.0f)
    {
        juce::Font f { juce::FontOptions (height) };
        if (bold) f = f.boldened();
        if (kerning != 0.0f) f = f.withExtraKerningFactor (kerning);
        return f;
    }

    inline juce::Colour accentOf (juce::Component& c)
    {
        return juce::Colour ((juce::uint32) (juce::int64) c.getProperties().getWithDefault ("accent", (juce::int64) 0xffffffff));
    }

    inline void setAccent (juce::Component& c, juce::Colour col)
    {
        c.getProperties().set ("accent", (juce::int64) col.getARGB());
    }

    // ------------------------------------------------------------------ look & feel
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff0ece4));
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6e6e6));
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17181b));
            setColour (juce::PopupMenu::textColourId, juce::Colour (0xffdddddd));
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2b2d33));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0e10));
            setColour (juce::TextEditor::textColourId, juce::Colours::white);
            setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff555a62));
            setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8e4dc));
            setColour (juce::TextButton::textColourOnId, juce::Colour (0xffe8e4dc));
        }

        // knob -------------------------------------------------------------
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startA, float endA, juce::Slider& s) override
        {
            using namespace juce;
            const auto accent = accentOf (s);
            const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float d = jmin (area.getWidth(), area.getHeight());
            const auto c = area.getCentre();

            const float arcR = d * 0.5f - d * 0.04f;
            const float arcW = d * 0.045f;
            const float bodyR = arcR - arcW * 2.0f;
            const float angle = startA + pos * (endA - startA);

            // soft backing disc
            g.setColour (Colour (0xff060607).withAlpha (0.6f));
            g.fillEllipse (c.x - arcR - arcW, c.y - arcR - arcW, (arcR + arcW) * 2.0f, (arcR + arcW) * 2.0f);

            Path track;
            track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
            g.setColour (accent.withAlpha (0.16f));
            g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

            if (pos > 0.002f)
            {
                Path val;
                val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, angle, true);
                const float widths[3] = { 3.4f, 2.3f, 1.5f };
                const float alphas[3] = { 0.07f, 0.12f, 0.22f };
                for (int i = 0; i < 3; ++i)
                {
                    g.setColour (accent.withAlpha (alphas[i]));
                    g.strokePath (val, PathStrokeType (arcW * widths[i], PathStrokeType::curved, PathStrokeType::rounded));
                }
                g.setColour (accent);
                g.strokePath (val, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
            }

            // body
            ColourGradient body (Colour (0xff35353a), c.x - bodyR * 0.3f, c.y - bodyR * 0.45f,
                                 Colour (0xff0b0b0d), c.x - bodyR * 0.3f + bodyR * 1.5f, c.y - bodyR * 0.45f, true);
            g.setGradientFill (body);
            g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (Colour (0xff55565c).withAlpha (0.55f));
            g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

            const float faceR = bodyR * 0.78f;
            ColourGradient face (Colour (0xff222225), c.x, c.y - faceR, Colour (0xff131315), c.x, c.y + faceR, false);
            g.setGradientFill (face);
            g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.drawEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

            // pointer
            const float r1 = bodyR * 0.50f, r2 = bodyR * 0.90f;
            Path p;
            p.startNewSubPath (c.x + r1 * std::sin (angle), c.y - r1 * std::cos (angle));
            p.lineTo (c.x + r2 * std::sin (angle), c.y - r2 * std::cos (angle));
            g.setColour (accent.interpolatedWith (Colours::white, 0.55f));
            g.strokePath (p, PathStrokeType (jmax (2.0f, d * 0.022f), PathStrokeType::curved, PathStrokeType::rounded));
        }

        // value box --------------------------------------------------------
        void drawLabel (juce::Graphics& g, juce::Label& l) override
        {
            using namespace juce;
            auto b = l.getLocalBounds().toFloat();
            if (dynamic_cast<Slider*> (l.getParentComponent()) != nullptr)
            {
                g.setColour (Colour (0xff0e0f11).withAlpha (0.9f));
                g.fillRoundedRectangle (b.reduced (0.0f, 1.0f), b.getHeight() * 0.28f);
            }
            if (! l.isBeingEdited())
            {
                g.setColour (l.findColour (Label::textColourId));
                g.setFont (getLabelFont (l));
                g.drawText (l.getText(), l.getLocalBounds().reduced (2, 0), l.getJustificationType(), false);
            }
        }

        juce::Font getLabelFont (juce::Label& l) override
        {
            if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
                return makeFont ((float) l.getHeight() * 0.56f);
            return l.getFont();
        }

        // switches / power buttons -----------------------------------------
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
        {
            using namespace juce;
            const auto accent = accentOf (b);
            const bool on = b.getToggleState();
            const auto bounds = b.getLocalBounds().toFloat();

            if (b.getProperties().getWithDefault ("kind", "switch").toString() == "power")
            {
                const auto c = bounds.getCentre();
                const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.30f;
                Path icon;
                icon.addCentredArc (c.x, c.y, r, r, 0.0f, 0.55f, MathConstants<float>::twoPi - 0.55f, true);
                icon.startNewSubPath (c.x, c.y - r * 1.15f);
                icon.lineTo (c.x, c.y - r * 0.08f);

                const float w = jmax (2.0f, r * 0.20f);
                if (on)
                    for (int i = 3; i >= 1; --i)
                    {
                        g.setColour (accent.withAlpha (0.07f * (float) (4 - i)));
                        g.strokePath (icon, PathStrokeType (w * (1.0f + 0.9f * (float) i), PathStrokeType::curved, PathStrokeType::rounded));
                    }
                g.setColour (on ? accent : Colour (0xff6a6c70));
                g.strokePath (icon, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
                return;
            }

            // pill switch
            const auto track = bounds.reduced (1.0f);
            const float rad = track.getHeight() * 0.5f;
            g.setColour (Colour (0xff141518));
            g.fillRoundedRectangle (track, rad);
            g.setColour (on ? accent.withAlpha (0.9f) : Colour (0xff3b3d42));
            g.drawRoundedRectangle (track, rad, jmax (1.5f, track.getHeight() * 0.07f));

            const float td = track.getHeight() * 0.58f;
            const float pad = (track.getHeight() - td) * 0.5f;
            const float tx = on ? track.getRight() - pad - td : track.getX() + pad;
            const float ty = track.getY() + pad;
            if (on)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.fillEllipse (tx - td * 0.25f, ty - td * 0.25f, td * 1.5f, td * 1.5f);
            }
            g.setColour (on ? accent : Colour (0xff75777c));
            g.fillEllipse (tx, ty, td, td);
        }

        // combo box --------------------------------------------------------
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            using namespace juce;
            const auto b = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.0f);
            g.setColour (Colour (0xff141519));
            g.fillRoundedRectangle (b, (float) h * 0.14f);
            g.setColour (Colour (0xff34363b));
            g.drawRoundedRectangle (b, (float) h * 0.14f, 1.3f);

            const float ax = (float) w - (float) h * 0.5f, ay = (float) h * 0.5f, s = (float) h * 0.12f;
            Path p;
            p.addTriangle (ax - s, ay - s * 0.6f, ax + s, ay - s * 0.6f, ax, ay + s * 0.8f);
            g.setColour (Colour (0xffcfcfcf));
            g.fillPath (p);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds ((int) (box.getHeight() * 0.3f), 1, box.getWidth() - (int) (box.getHeight() * 1.0f), box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override { return makeFont ((float) box.getHeight() * 0.40f); }
        juce::Font getPopupMenuFont() override { return makeFont (15.0f); }
    };

    // ------------------------------------------------------------------ icon buttons
    class IconButton : public juce::Button
    {
    public:
        enum Kind { Save, Dots, Gear, Tap };

        explicit IconButton (Kind k, juce::Colour icon = juce::Colour (0xffd8d8d8))
            : juce::Button ("icon"), kind (k), iconColour (icon) {}

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            using namespace juce;
            auto b = getLocalBounds().toFloat();
            const float s = jmin (b.getWidth(), b.getHeight());
            const auto c = b.getCentre();

            if (kind != Gear)
            {
                g.setColour (Colour (0xff131417));
                g.fillRoundedRectangle (b.reduced (1.0f), s * 0.14f);
                g.setColour (over ? Colour (0xff4a4d54) : Colour (0xff2c2e33));
                g.drawRoundedRectangle (b.reduced (1.0f), s * 0.14f, 1.3f);
            }

            auto col = iconColour;
            if (down) col = col.darker (0.35f);
            else if (over) col = col.brighter (0.25f);
            g.setColour (col);

            switch (kind)
            {
                case Save:
                {
                    const float lw = jmax (1.4f, s * 0.045f);
                    g.drawRoundedRectangle (c.x - s * 0.27f, c.y - s * 0.27f, s * 0.54f, s * 0.54f, s * 0.05f, lw);
                    g.drawRect (c.x - s * 0.14f, c.y - s * 0.27f, s * 0.28f, s * 0.16f, lw);
                    g.drawRect (c.x - s * 0.17f, c.y + s * 0.02f, s * 0.34f, s * 0.25f, lw);
                    break;
                }
                case Dots:
                    for (int i = -1; i <= 1; ++i)
                        g.fillEllipse (c.x - s * 0.045f, c.y + (float) i * s * 0.17f - s * 0.045f, s * 0.09f, s * 0.09f);
                    break;
                case Gear:
                {
                    Path star;
                    star.addStar (c, 8, s * 0.22f, s * 0.33f, 0.2f);
                    g.fillPath (star);
                    g.fillEllipse (c.x - s * 0.26f, c.y - s * 0.26f, s * 0.52f, s * 0.52f);
                    g.setColour (Colour (0xff101113));
                    g.fillEllipse (c.x - s * 0.11f, c.y - s * 0.11f, s * 0.22f, s * 0.22f);
                    break;
                }
                case Tap:
                {
                    const float lw = jmax (1.8f, s * 0.06f);
                    const float hx = c.x - s * 0.08f, hy = c.y + s * 0.17f;
                    Path head;
                    head.addEllipse (hx - s * 0.12f, hy - s * 0.08f, s * 0.24f, s * 0.16f);
                    g.fillPath (head, AffineTransform::rotation (-0.35f, hx, hy));
                    const float sx = hx + s * 0.11f;
                    Path stem;
                    stem.startNewSubPath (sx, hy - s * 0.02f);
                    stem.lineTo (sx, c.y - s * 0.28f);
                    stem.quadraticTo (sx + s * 0.04f, c.y - s * 0.16f, sx + s * 0.20f, c.y - s * 0.10f);
                    g.strokePath (stem, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));
                    break;
                }
            }
        }

    private:
        Kind kind;
        juce::Colour iconColour;
    };

    // ------------------------------------------------------------------ visualizers
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour accent)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101012), b.getX(), b.getY(),
                                                 juce::Colour (0xff0a0a0b), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (accent.withAlpha (0.16f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
    }

    // Echo taps: height of each repeat in dB, from the feedback setting.
    class EchoView : public juce::Component
    {
    public:
        EchoView (std::atomic<float>* feedbackPct, std::atomic<float>* onFlag)
            : fb (feedbackPct), on (onFlag) { setInterceptsMouseClicks (false, false); }

        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            const auto b = getLocalBounds().toFloat();
            drawPanel (g, b, delayAccent);

            const float amount = on->load() > 0.5f ? 1.0f : 0.35f;
            const float f = jlimit (0.0f, 0.95f, fb->load() * 0.01f);
            const float baseY = b.getBottom() - b.getHeight() * 0.10f;
            const float maxH = b.getHeight() * 0.74f;
            const float barW = jmax (2.0f, b.getHeight() * 0.030f);

            for (int i = 0; i < 14; ++i)
            {
                const float x = b.getX() + b.getWidth() * (0.06f + (float) i * 0.0735f);
                const float lvl = i == 0 ? 1.0f : std::pow (f, (float) i);
                const float dB = 20.0f * std::log10 (jmax (lvl, 1.0e-5f));
                const float h = jlimit (0.0f, 1.0f, 1.0f + dB / 42.0f) * maxH;

                if (h > 2.0f)
                {
                    const auto bar = Rectangle<float> (x - barW * 0.5f, baseY - h, barW, h);
                    g.setColour (delayAccent.withAlpha (0.12f * amount));
                    g.fillRoundedRectangle (bar.expanded (barW * 0.9f, 0.0f), barW);
                    g.setGradientFill (ColourGradient (delayAccent.brighter (0.3f).withAlpha (amount), x, bar.getY(),
                                                       delayAccent.darker (0.3f).withAlpha (amount), x, bar.getBottom(), false));
                    g.fillRoundedRectangle (bar, barW * 0.5f);
                }
                else
                {
                    g.setColour (delayAccent.withAlpha (0.5f * amount));
                    g.fillEllipse (x - barW * 0.4f, baseY - barW * 0.8f, barW * 0.8f, barW * 0.8f);
                }

                if (i > 0)
                {
                    g.setColour (delayAccent.withAlpha (0.30f * amount));
                    for (float y = baseY - h - 10.0f; y > b.getY() + b.getHeight() * 0.12f; y -= 13.0f)
                        g.fillEllipse (x - 1.0f, y, 2.0f, 2.0f);
                }
            }
        }

    private:
        std::atomic<float>* fb;
        std::atomic<float>* on;
    };

    // Reverb tail: decay envelope over time, offset by pre-delay.
    class DecayView : public juce::Component
    {
    public:
        DecayView (std::atomic<float>* decaySec, std::atomic<float>* preMs, std::atomic<float>* onFlag)
            : decay (decaySec), pre (preMs), on (onFlag) { setInterceptsMouseClicks (false, false); }

        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            const auto b = getLocalBounds().toFloat();
            drawPanel (g, b, reverbAccent);

            const float amount = on->load() > 0.5f ? 1.0f : 0.35f;
            const float rt = jmax (0.1f, decay->load());
            const float preSec = pre->load() * 0.001f;
            const float window = jmax (0.5f, rt * 1.3f + preSec);

            const auto area = b.reduced (b.getWidth() * 0.02f, b.getHeight() * 0.10f);
            const int cols = 230;
            const float colW = area.getWidth() / (float) cols;

            for (int i = 0; i < cols; ++i)
            {
                const float t = window * (float) i / (float) cols - preSec;
                float env = 0.0f;
                if (t >= 0.0f) env = std::exp (-6.9078f * t / rt);

                const float s = std::sin ((float) i * 12.9898f) * 43758.5453f;
                const float n = s - std::floor (s);
                const float h = env * (0.35f + 0.65f * n) * area.getHeight();
                if (h < 0.6f) continue;

                const float x = area.getX() + (float) i * colW;
                g.setGradientFill (ColourGradient (reverbAccent.brighter (0.2f).withAlpha (amount), x, area.getBottom() - h,
                                                   reverbAccent.darker (0.5f).withAlpha (0.8f * amount), x, area.getBottom(), false));
                g.fillRect (x, area.getBottom() - h, jmax (1.0f, colW * 0.7f), h);
            }
        }

    private:
        std::atomic<float>* decay;
        std::atomic<float>* pre;
        std::atomic<float>* on;
    };
}
