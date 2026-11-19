#pragma once
#include "Theme.h"

class AfterimageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AfterimageLookAndFeel()
    {
        using namespace theme;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::ComboBox::backgroundColourId, charcoal);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, edge);
        setColour (juce::ComboBox::arrowColourId, violet);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, purple);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, dim);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, charcoal);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, purple);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::TextEditor::backgroundColourId, charcoal);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, edge);
    }

    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (15.f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        using namespace theme;
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f, trackR = r - 2.f, thick = juce::jmax (2.5f, d * 0.075f);
        const float ang = a0 + pos * (a1 - a0);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const bool active = s.isMouseOverOrDragging() || s.hasKeyboardFocus (false);

        juce::Path track; track.addCentredArc (c.x, c.y, trackR, trackR, 0.f, a0, a1, true);
        g.setColour (edge); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? a0 + 0.5f * (a1 - a0) : a0;
        juce::Path val; val.addCentredArc (c.x, c.y, trackR, trackR, 0.f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        const auto col = (bool) s.getProperties().getWithDefault ("ice", false) ? ice : violet;
        g.setColour (col.withAlpha (active ? 0.30f : 0.18f)); g.strokePath (val, juce::PathStrokeType (thick * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col); g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float bodyR = trackR - thick - 3.f;
        juce::ColourGradient grad (charcoal.brighter (0.25f), c.x - bodyR * 0.5f, c.y - bodyR * 0.7f, bg, c.x + bodyR * 0.6f, c.y + bodyR, true);
        g.setGradientFill (grad); g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f);
        g.setColour (edge.brighter (0.3f)); g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f, 1.f);

        juce::Path ptr; ptr.addRoundedRectangle (-1.6f, -bodyR + 3.f, 3.2f, bodyR * 0.45f, 1.5f);
        g.setColour (col.brighter (0.3f)); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) override
    {
        using namespace theme;
        const auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        const auto col = (bool) b.getProperties().getWithDefault ("ice", false) ? ice : violet;
        if (on) { g.setColour (col.withAlpha (0.22f)); g.fillRoundedRectangle (r.expanded (2.f), 8.f); }
        g.setColour (on ? col.withAlpha (0.35f) : charcoal); g.fillRoundedRectangle (r, 6.f);
        g.setColour (on ? col : (hl ? purple : edge)); g.drawRoundedRectangle (r, 6.f, 1.4f);
        g.setColour (on ? juce::Colours::white : dim);
        g.setFont (juce::FontOptions (juce::jmax (9.f, r.getHeight() * 0.38f), juce::Font::bold));
        g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down) override
    {
        using namespace theme;
        const auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? purple.withAlpha (0.55f) : (down ? edge : charcoal)); g.fillRoundedRectangle (r, 5.f);
        g.setColour (on ? violet : (hl ? purple : edge)); g.drawRoundedRectangle (r, 5.f, on ? 1.5f : 1.f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace theme;
        const auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (1.f);
        g.setColour (charcoal); g.fillRoundedRectangle (r, 5.f);
        g.setColour (edge); g.drawRoundedRectangle (r, 5.f, 1.f);
        juce::Path p; const float cx = (float) w - 12.f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour (violet); g.fillPath (p);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        l.setFont (juce::FontOptions (juce::jmax (9.f, (float) box.getHeight() * 0.42f)));
    }
};
