#pragma once
#include "../Source/PluginProcessor.h"
#include "LookAndFeel.h"
#include "Knob.h"
#include "Visualizer.h"

class ModeSelector : public juce::Component
{
public:
    ModeSelector (juce::AudioProcessorValueTreeState&, juce::UndoManager*);
    void resized() override;
private:
    static constexpr int N = afterimage::NumModes;
    juce::TextButton buttons[N];
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class AfterimageEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AfterimageEditor (AfterimageProcessor&);
    ~AfterimageEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct LfoStrip
    {
        LfoStrip (juce::AudioProcessorValueTreeState&, const juce::String& prefix);
        juce::ComboBox shape, dest, div;
        juce::ToggleButton sync;
        Knob rate, amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aShape, aDest, aDiv;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aSync;
    };

    void timerCallback() override;
    void showPresetMenu();
    void showSaveDialog();
    void showLoadDialog();
    void showSettingsMenu();
    void refreshPresetLabel();
    void setupCombo (juce::ComboBox&, const char* paramId, const juce::String& tooltip);
    juce::Rectangle<int> R (float x, float y, float w, float h) const;

    AfterimageProcessor& proc;
    AfterimageLookAndFeel laf;

    Visualizer visualizer;
    Knob size, density, position, pitch, speed, spray, mix, width, feedback, output, envelope, overlap;
    ModeSelector modeSelector;
    juce::ToggleButton freezeButton { "FREEZE" }, reverseButton { "REVERSE" }, syncButton { "TEMPO SYNC" };
    juce::ComboBox divisionBox;
    LfoStrip lfo1, lfo2;
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, loadButton { "LOAD" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, settingsButton { "SETTINGS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aFreeze, aReverse, aSync;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aDivision;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltipWindow { this, 500 };
    bool limiterLit = false;
    juce::Rectangle<int> logoArea, limitArea, modArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterimageEditor)
};
