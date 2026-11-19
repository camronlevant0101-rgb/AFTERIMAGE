#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "ParamTable.h"

// Slider that intercepts right-click for exact value entry.
class ValueSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { ignoring = true; if (onRightClick) onRightClick(); return; } ignoring = false; juce::Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! ignoring) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! ignoring) juce::Slider::mouseUp (e); ignoring = false; }
private:
    bool ignoring = false;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, bool smallKnob = false, bool iceColour = false)
        : small (smallKnob), param (apvts.getParameter (paramId))
    {
        const auto& pi = afterimage::kParams[afterimage::findParam (paramId)];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
        slider.setTooltip (juce::String (pi.name).toUpperCase() + ": " + pi.tip + "  (Double-click: reset. Right-click: type a value.)");
        if (pi.min < 0.f && pi.max > 0.f && pi.min == -pi.max) slider.getProperties().set ("bipolar", true);
        if (iceColour) slider.getProperties().set ("ice", true);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        slider.setDoubleClickReturnValue (true, (double) pi.def);
        slider.onValueChange = [this] { refreshText(); };
        slider.onRightClick = [this] { askForValue(); };

        name.setText (juce::String (small ? shortName (pi.name) : pi.name).toUpperCase(), juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred); name.setInterceptsMouseClicks (false, false);
        name.setColour (juce::Label::textColourId, theme::dim);
        value.setJustificationType (juce::Justification::centred); value.setInterceptsMouseClicks (false, false);
        value.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (name); addAndMakeVisible (value);
        refreshText();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int lh = juce::jmax (10, (int) (b.getHeight() * (small ? 0.2f : 0.14f)));
        name.setBounds (b.removeFromTop (lh)); value.setBounds (b.removeFromBottom (lh));
        slider.setBounds (b);
        name.setFont (juce::FontOptions ((float) lh * 0.82f, juce::Font::bold));
        value.setFont (juce::FontOptions ((float) lh * 0.88f));
    }

private:
    static juce::String shortName (const char* n) { juce::String s (n); return s.startsWith ("LFO") ? s.substring (5) : s; }

    void refreshText()
    {
        if (param != nullptr)
            value.setText (param->getText (param->convertTo0to1 ((float) slider.getValue()), 16), juce::dontSendNotification);
    }

    void askForValue()
    {
        auto* aw = new juce::AlertWindow (name.getText(), "Enter an exact value", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("v", value.getText());
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Knob> safe (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r)
        {
            if (r == 1 && safe != nullptr && safe->param != nullptr)
            {
                const float norm = safe->param->getValueForText (aw->getTextEditorContents ("v"));
                safe->param->beginChangeGesture();
                safe->param->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
                safe->param->endChangeGesture();
            }
        }), true);
    }

    bool small;
    juce::RangedAudioParameter* param;
    ValueSlider slider;
    juce::Label name, value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
