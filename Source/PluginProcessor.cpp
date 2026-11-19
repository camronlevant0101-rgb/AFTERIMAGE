#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace afterimage;

juce::AudioProcessorValueTreeState::ParameterLayout AfterimageProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& pi = kParams[i];
        const juce::ParameterID pid { pi.id, 1 };
        if (pi.kind == Kind::Float)
        {
            juce::NormalisableRange<float> range (pi.min, pi.max, 0.f);
            if (pi.centre > 0.f && pi.centre > pi.min && pi.centre < pi.max) range.setSkewForCentre (pi.centre);
            const float scale = pi.scale; const int dec = pi.decimals; const juce::String unit (pi.unit);
            auto attrs = juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([scale, dec, unit] (float v, int) { return juce::String (v * scale, dec) + " " + unit; })
                .withValueFromStringFunction ([scale] (const juce::String& t) { return t.getFloatValue() / scale; });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, pi.name, range, pi.def, attrs));
        }
        else if (pi.kind == Kind::Bool)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, pi.name, pi.def > 0.5f));
        else
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, pi.name, juce::StringArray::fromTokens (pi.choices, "|", ""), (int) pi.def));
    }
    return layout;
}

AfterimageProcessor::AfterimageProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "AFTERIMAGE", createLayout())
{
    for (int i = 0; i < kNumParams; ++i) raw[(size_t) i] = apvts.getRawParameterValue (kParams[i].id);
    apvts.state.setProperty ("presetName", "Init", nullptr);
    rescanPresets();
}

bool AfterimageProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void AfterimageProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock); // allocates here, never in processBlock
}

afterimage::Params AfterimageProcessor::currentParams() const
{
    Params p;
    for (int i = 0; i < kNumParams; ++i) p.*(kParams[i].member) = raw[(size_t) i]->load (std::memory_order_relaxed);
    return p;
}

void AfterimageProcessor::runEngine (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < nch; ++c) buffer.clear (c, 0, n);

    HostInfo h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { h.bpmValid = *bpm > 0.0; h.bpm = *bpm; }
            h.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { h.ppqValid = true; h.ppq = *ppq; }
        }

    engine.setParams (currentParams());
    engine.setBypassed (bypassed);
    if (nch >= 2)      engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, h);
    else if (nch == 1) engine.processMono (buffer.getWritePointer (0), n, h);
}

void AfterimageProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)          { runEngine (b, false); }
void AfterimageProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { runEngine (b, true); }

juce::AudioProcessorEditor* AfterimageProcessor::createEditor() { return new AfterimageEditor (*this); }

void AfterimageProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AfterimageProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (! apvts.state.hasProperty ("presetName")) apvts.state.setProperty ("presetName", "Init", nullptr);
        }
}

// ------------------------------------------------------------------------------------
// Presets
// ------------------------------------------------------------------------------------
juce::File AfterimageProcessor::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AFTERIMAGE").getChildFile ("Presets");
}

void AfterimageProcessor::rescanPresets()
{
    presetList.clear();
    for (int i = 0; i < kNumFactoryPresets; ++i)
        presetList.push_back ({ kFactoryPresets[i].name, kCategoryNames[(int) kFactoryPresets[i].category], true, i, {} });

    auto dir = userPresetDirectory();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.aipreset"))
        {
            PresetData d;
            if (parsePreset (f.loadFileAsString().toStdString(), d) && ! isFactoryName (d.name.c_str()))
                presetList.push_back ({ d.name, "User", false, -1, f });
        }
}

int AfterimageProcessor::currentPresetIndex() const
{
    const auto n = currentPresetName();
    for (size_t i = 0; i < presetList.size(); ++i) if (presetList[i].name == n) return (int) i;
    return -1;
}

void AfterimageProcessor::applyParams (const Params& p, const juce::String& name)
{
    undoManager.beginNewTransaction ("Load preset");
    for (int i = 0; i < kNumParams; ++i)
        if (auto* prm = apvts.getParameter (kParams[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(kParams[i].member)));
    apvts.state.setProperty ("presetName", name, nullptr);
}

void AfterimageProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presetList.size()) return;
    const auto& e = presetList[(size_t) index];
    if (e.factory) { applyParams (kFactoryPresets[e.factoryIndex].p, e.name); return; }
    loadPresetFile (e.file);
}

void AfterimageProcessor::stepPreset (int direction)
{
    if (presetList.empty()) return;
    const int n = (int) presetList.size();
    int cur = currentPresetIndex();
    cur = cur < 0 ? (direction > 0 ? 0 : n - 1) : (cur + direction + n) % n;
    loadPreset (cur);
}

bool AfterimageProcessor::loadPresetFile (const juce::File& f)
{
    PresetData d;
    if (! f.existsAsFile() || ! parsePreset (f.loadFileAsString().toStdString(), d)) return false;
    applyParams (d.p, juce::String (d.name));
    return true;
}

bool AfterimageProcessor::saveUserPreset (const juce::String& nameIn, juce::String& error)
{
    const auto name = nameIn.trim();
    if (name.isEmpty()) { error = "Please enter a preset name."; return false; }
    if (isFactoryName (name.toRawUTF8())) { error = "\"" + name + "\" is a factory preset and cannot be overwritten. Choose another name."; return false; }
    auto dir = userPresetDirectory();
    if (! dir.createDirectory().wasOk()) { error = "Could not create " + dir.getFullPathName(); return false; }
    PresetData d; d.name = name.toStdString(); d.category = "User"; d.p = currentParams();
    auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".aipreset");
    if (! file.replaceWithText (juce::String (serializePreset (d)))) { error = "Could not write " + file.getFullPathName(); return false; }
    apvts.state.setProperty ("presetName", name, nullptr);
    rescanPresets();
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AfterimageProcessor(); }
