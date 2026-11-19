#include "PluginEditor.h"

using namespace afterimage;

static constexpr float kW = 1000.f, kH = 640.f;

// ------------------------------------------------------------------ ModeSelector
ModeSelector::ModeSelector (juce::AudioProcessorValueTreeState& apvts, juce::UndoManager* um)
{
    static const char* names[N] = { "NORMAL", "TEXTURE", "STUTTER", "PITCH", "REVERSE", "TIME", "RANDOM" };
    static const char* tips[N] = {
        "NORMAL: musically related granular resynthesis.",
        "TEXTURE: long, smooth, heavily overlapping grains for atmospheres.",
        "STUTTER: short rhythmic grains that repeat a captured slice (glitch / repeat).",
        "PITCH: short, dense grains - a granular pitch shifter. Spray adds detune.",
        "REVERSE: all grains play backwards.",
        "TIME: pitch-preserving time-stretch. Speed moves the playhead through the buffer.",
        "RANDOM: experimental - random consonant pitch intervals, reversals, sizes and positions." };
    auto* prm = apvts.getParameter ("mode");
    attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
    {
        const int idx = (int) (v + 0.5f);
        for (int i = 0; i < N; ++i) buttons[i].setToggleState (i == idx, juce::dontSendNotification);
    }, um);
    for (int i = 0; i < N; ++i)
    {
        buttons[i].setButtonText (names[i]);
        buttons[i].setTooltip (tips[i]);
        buttons[i].setClickingTogglesState (true);
        buttons[i].setRadioGroupId (4242);
        buttons[i].onClick = [this, i] { if (buttons[i].getToggleState()) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (buttons[i]);
    }
    attachment->sendInitialUpdate();
}

void ModeSelector::resized()
{
    auto b = getLocalBounds();
    const int w = b.getWidth() / N;
    for (int i = 0; i < N; ++i) buttons[i].setBounds (b.removeFromLeft (w).reduced (2, 0));
}

// ------------------------------------------------------------------ LFO strip
AfterimageEditor::LfoStrip::LfoStrip (juce::AudioProcessorValueTreeState& a, const juce::String& p)
    : rate (a, (p + "Rate").toRawUTF8(), true), amount (a, (p + "Amount").toRawUTF8(), true)
{
    aShape = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, p + "Shape", shape);
    aDest  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, p + "Dest", dest);
    aDiv   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, p + "Div", div);
    aSync  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, p + "Sync", sync);
}

// ------------------------------------------------------------------ Editor
void AfterimageEditor::setupCombo (juce::ComboBox& c, const char* paramId, const juce::String& tip)
{
    c.addItemList (juce::StringArray::fromTokens (kParams[findParam (paramId)].choices, "|", ""), 1);
    c.setTooltip (tip);
    addAndMakeVisible (c);
}

AfterimageEditor::AfterimageEditor (AfterimageProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      visualizer (p.engine, p.apvts),
      size (p.apvts, "size"), density (p.apvts, "density"), position (p.apvts, "position"), pitch (p.apvts, "pitch"),
      speed (p.apvts, "speed"), spray (p.apvts, "spray"), mix (p.apvts, "mix", false, true), width (p.apvts, "width"),
      feedback (p.apvts, "feedback"), output (p.apvts, "output"), envelope (p.apvts, "shape", true), overlap (p.apvts, "overlap", true),
      modeSelector (p.apvts, &p.undoManager), lfo1 (p.apvts, "lfo1"), lfo2 (p.apvts, "lfo2")
{
    setLookAndFeel (&laf);
    for (Component* c : std::initializer_list<Component*> { &visualizer, &size, &density, &position, &pitch, &speed, &spray, &mix, &width, &feedback,
                                                            &output, &envelope, &overlap, &modeSelector })
        addAndMakeVisible (c);

    freezeButton.getProperties().set ("ice", true);
    freezeButton.setTooltip (kParams[findParam ("freeze")].tip);
    reverseButton.setTooltip (kParams[findParam ("reverse")].tip);
    syncButton.setTooltip (kParams[findParam ("sync")].tip);
    aFreeze  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "freeze", freezeButton);
    aReverse = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "reverse", reverseButton);
    aSync    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "sync", syncButton);
    for (auto* b : { &freezeButton, &reverseButton, &syncButton }) addAndMakeVisible (b);

    setupCombo (divisionBox, "division", "Grain trigger division when TEMPO SYNC is on.");
    aDivision = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "division", divisionBox);

    for (auto* l : { &lfo1, &lfo2 })
    {
        setupCombo (l->shape, "lfo1Shape", "LFO waveform: Sine, Triangle, Square or Random (sample & hold).");
        setupCombo (l->dest, "lfo1Dest", "Modulation target. Pick which parameter this LFO controls; AMT sets the depth.");
        setupCombo (l->div, "lfo1Div", "LFO cycle length when SYNC is on (locked to host tempo).");
        l->sync.setButtonText ("SYNC"); l->sync.setTooltip ("Lock this LFO to host tempo (uses free rate if no tempo is available).");
        addAndMakeVisible (l->sync); addAndMakeVisible (l->rate); addAndMakeVisible (l->amount);
    }

    presetButton.setTooltip ("Click to browse presets. Factory presets are protected; your own presets appear under User.");
    prevButton.setTooltip ("Previous preset"); nextButton.setTooltip ("Next preset");
    saveButton.setTooltip ("Save the current sound as a user preset"); loadButton.setTooltip ("Load a preset file (.aipreset)");
    undoButton.setTooltip ("Undo last parameter change"); redoButton.setTooltip ("Redo");
    settingsButton.setTooltip ("Settings and utilities");
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton, &loadButton, &undoButton, &redoButton, &settingsButton }) addAndMakeVisible (b);

    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { proc.stepPreset (-1); refreshPresetLabel(); };
    nextButton.onClick = [this] { proc.stepPreset (+1); refreshPresetLabel(); };
    saveButton.onClick = [this] { showSaveDialog(); };
    loadButton.onClick = [this] { showLoadDialog(); };
    undoButton.onClick = [this] { proc.undoManager.undo(); refreshPresetLabel(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); refreshPresetLabel(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    refreshPresetLabel();
    setResizable (true, true);
    setResizeLimits (760, 486, 2000, 1280);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1000, 640);
    startTimerHz (10);
}

AfterimageEditor::~AfterimageEditor() { stopTimer(); setLookAndFeel (nullptr); }

juce::Rectangle<int> AfterimageEditor::R (float x, float y, float w, float h) const
{
    const float sx = (float) getWidth() / kW, sy = (float) getHeight() / kH;
    return { juce::roundToInt (x * sx), juce::roundToInt (y * sy), juce::roundToInt (w * sx), juce::roundToInt (h * sy) };
}

void AfterimageEditor::resized()
{
    logoArea = R (16, 6, 250, 48);
    prevButton.setBounds (R (280, 12, 30, 32));
    presetButton.setBounds (R (312, 12, 250, 32));
    nextButton.setBounds (R (564, 12, 30, 32));
    saveButton.setBounds (R (602, 12, 56, 32));
    loadButton.setBounds (R (662, 12, 56, 32));
    undoButton.setBounds (R (730, 12, 52, 32));
    redoButton.setBounds (R (786, 12, 52, 32));
    limitArea = R (852, 14, 50, 28);
    settingsButton.setBounds (R (906, 12, 78, 32));

    visualizer.setBounds (R (16, 62, 968, 232));

    Knob* row[] = { &size, &density, &position, &pitch, &speed, &spray, &mix, &width, &feedback, &output };
    for (int i = 0; i < 10; ++i) row[i]->setBounds (R (16.f + 96.8f * (float) i, 302, 96.8f, 148));

    modeSelector.setBounds (R (16, 464, 480, 30));
    freezeButton.setBounds (R (16, 502, 112, 36));
    reverseButton.setBounds (R (132, 502, 112, 36));
    syncButton.setBounds (R (248, 502, 112, 36));
    divisionBox.setBounds (R (366, 506, 130, 28));
    envelope.setBounds (R (16, 548, 90, 84));
    overlap.setBounds (R (110, 548, 90, 84));

    modArea = R (512, 464, 472, 168);
    float y = 492;
    for (auto* l : { &lfo1, &lfo2 })
    {
        l->shape.setBounds (R (552, y + 18, 78, 28));
        l->dest.setBounds (R (634, y + 18, 92, 28));
        l->rate.setBounds (R (730, y, 56, 64));
        l->amount.setBounds (R (788, y, 56, 64));
        l->sync.setBounds (R (848, y + 18, 50, 28));
        l->div.setBounds (R (902, y + 18, 78, 28));
        y += 70;
    }
}

void AfterimageEditor::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (purple.withAlpha (0.18f), 0.f, 0.f, bg.withAlpha (0.f), (float) getWidth() * 0.6f, (float) getHeight() * 0.5f, true));
    g.fillRect (getLocalBounds());
    const float s = (float) getWidth() / kW;

    // top bar
    g.setColour (panel); g.fillRect (0, 0, getWidth(), juce::roundToInt (56.f * s));
    g.setColour (edge);  g.drawHorizontalLine (juce::roundToInt (56.f * s), 0.f, (float) getWidth());

    // logo: an "echo" mark (three offset rings, each a fading afterimage of the last) + wordmark
    auto la = logoArea.toFloat();
    const float rr = la.getHeight() * 0.30f, cy = la.getCentreY() - la.getHeight() * 0.05f;
    for (int i = 2; i >= 0; --i)
    {
        g.setColour ((i == 0 ? violet : (i == 1 ? purple : ice)).withAlpha (i == 0 ? 1.f : (i == 1 ? 0.7f : 0.45f)));
        g.drawEllipse (la.getX() + rr + (float) i * rr * 0.55f, cy - rr, rr * 2.f, rr * 2.f, juce::jmax (1.5f, 2.f * s));
    }
    const float fh = la.getHeight() * 0.50f, adv = fh * 0.80f, x0 = la.getX() + rr * 4.1f;
    g.setFont (juce::FontOptions (fh, juce::Font::bold));
    const juce::String word ("AFTERIMAGE");
    for (int i = 0; i < word.length(); ++i)
    {
        g.setColour (i < 5 ? text : text.interpolatedWith (violet, 0.55f));
        g.drawText (word.substring (i, i + 1), juce::Rectangle<float> (x0 + adv * (float) i, la.getY(), adv, la.getHeight() * 0.62f), juce::Justification::centred);
    }
    g.setColour (dim); g.setFont (juce::FontOptions (la.getHeight() * 0.2f));
    g.drawText ("Turn any sound into something unheard.", juce::Rectangle<float> (x0, la.getY() + la.getHeight() * 0.62f, adv * 10.5f, la.getHeight() * 0.3f), juce::Justification::centredLeft);

    // limiter LED
    auto la2 = limitArea.toFloat();
    g.setColour (limiterLit ? warn : edge); g.fillEllipse (la2.getX(), la2.getCentreY() - 5.f * s, 10.f * s, 10.f * s);
    g.setColour (limiterLit ? warn : dim); g.setFont (juce::FontOptions (10.f * s, juce::Font::bold));
    g.drawText ("LIMIT", la2.withTrimmedLeft (14.f * s), juce::Justification::centredLeft);

    // modulation panel
    g.setColour (panel); g.fillRoundedRectangle (modArea.toFloat(), 10.f * s);
    g.setColour (edge);  g.drawRoundedRectangle (modArea.toFloat().reduced (0.5f), 10.f * s, 1.f);
    g.setColour (dim);   g.setFont (juce::FontOptions (10.f * s, juce::Font::bold));
    g.drawText ("MODULATION", modArea.withTrimmedLeft (int (12 * s)).removeFromTop (int (22 * s)), juce::Justification::centredLeft);
    const float ys[2] = { 492.f, 562.f };
    for (int i = 0; i < 2; ++i)
    {
        g.setColour (violet); g.setFont (juce::FontOptions (13.f * s, juce::Font::bold));
        g.drawText ("LFO" + juce::String (i + 1), R (520, ys[i] + 18, 30, 28), juce::Justification::centredLeft);
    }
    g.setColour (dim); g.setFont (juce::FontOptions (9.f * s));
    g.drawText ("SHAPE", R (552, 484 + 0.f, 78, 12), juce::Justification::centred);
    g.drawText ("TARGET", R (634, 484, 92, 12), juce::Justification::centred);
    g.drawText ("TEMPO", R (848, 484, 50, 12), juce::Justification::centred);
    g.drawText ("DIVISION", R (902, 484, 78, 12), juce::Justification::centred);
}

void AfterimageEditor::timerCallback()
{
    const bool lim = proc.engine.viz.limiting.load (std::memory_order_relaxed);
    if (lim != limiterLit) { limiterLit = lim; repaint (limitArea.expanded (4)); }
    refreshPresetLabel();
    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());
}

void AfterimageEditor::refreshPresetLabel()
{
    const auto n = proc.currentPresetName();
    if (presetButton.getButtonText() != n) presetButton.setButtonText (n);
}

void AfterimageEditor::showPresetMenu()
{
    proc.rescanPresets();
    const auto& list = proc.presets();
    juce::PopupMenu menu;
    const int cur = proc.currentPresetIndex();
    for (int c = 0; c <= (int) Category::User; ++c)
    {
        juce::PopupMenu sub; bool any = false;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].category == kCategoryNames[c]) { sub.addItem ((int) i + 1, list[i].name + (list[i].factory ? "" : "  (user)"), true, (int) i == cur); any = true; }
        if (any) menu.addSubMenu (kCategoryNames[c], sub);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this] (int r) { if (r > 0) { proc.loadPreset (r - 1); refreshPresetLabel(); } });
}

void AfterimageEditor::showSaveDialog()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name your preset. Factory preset names are protected.", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", proc.currentPresetName(), "Preset name:");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AfterimageEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        juce::String err;
        if (! safe->proc.saveUserPreset (aw->getTextEditorContents ("name"), err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Cannot save preset", err);
        safe->refreshPresetLabel();
    }), true);
}

void AfterimageEditor::showLoadDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Load AFTERIMAGE preset", AfterimageProcessor::userPresetDirectory(), "*.aipreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f != juce::File() && ! proc.loadPresetFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Cannot load preset", "That file is not a valid AFTERIMAGE preset.");
        refreshPresetLabel();
    });
}

void AfterimageEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Open user preset folder");
    m.addItem (2, "Rescan presets");
    m.addItem (3, "Reset all parameters to default");
    m.addSeparator();
    m.addItem (4, "About AFTERIMAGE");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this] (int r)
    {
        if (r == 1) { auto d = AfterimageProcessor::userPresetDirectory(); d.createDirectory(); d.revealToUser(); }
        else if (r == 2) proc.rescanPresets();
        else if (r == 3)
        {
            proc.undoManager.beginNewTransaction ("Reset");
            for (int i = 0; i < kNumParams; ++i)
                if (auto* p = proc.apvts.getParameter (kParams[i].id)) p->setValueNotifyingHost (p->getDefaultValue());
            proc.apvts.state.setProperty ("presetName", "Init", nullptr);
        }
        else if (r == 4)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "AFTERIMAGE 1.0.0",
                "Granular audio effect.\nTurn any sound into something unheard.\n\nVST3 / Standalone. Built with JUCE.");
    });
}
