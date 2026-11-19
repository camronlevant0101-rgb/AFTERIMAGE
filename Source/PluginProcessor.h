#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GranularEngine.h"
#include "ParamTable.h"
#include "PresetIO.h"
#include "Presets.h"
#include <array>

class AfterimageProcessor : public juce::AudioProcessor
{
public:
    struct PresetEntry { juce::String name, category; bool factory = false; int factoryIndex = -1; juce::File file; };

    AfterimageProcessor();
    ~AfterimageProcessor() override = default;

    // --- juce::AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    const juce::String getName() const override { return "AFTERIMAGE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- presets (message thread only) ---
    static juce::File userPresetDirectory();
    void rescanPresets();
    const std::vector<PresetEntry>& presets() const { return presetList; }
    int currentPresetIndex() const;
    juce::String currentPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }
    void loadPreset (int index);
    void stepPreset (int direction);
    bool loadPresetFile (const juce::File& f);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    afterimage::Params currentParams() const;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    afterimage::GranularEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void runEngine (juce::AudioBuffer<float>&, bool bypassed);
    void applyParams (const afterimage::Params&, const juce::String& name);

    std::array<std::atomic<float>*, afterimage::kNumParams> raw {};
    std::vector<PresetEntry> presetList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterimageProcessor)
};
