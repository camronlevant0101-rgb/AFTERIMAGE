#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GranularEngine.h"
#include "Theme.h"

// Real-time grain display. Reads only the engine's lock-free VizData.
class Visualizer : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Visualizer (afterimage::GranularEngine& e, juce::AudioProcessorValueTreeState& a) : engine (e), apvts (a)
    {
        posParam = apvts.getRawParameterValue ("position");
        setTooltip ("Captured buffer (left = oldest, right = now). Bars show live grains: width = grain size, brightness = envelope, lane stacking = overlap.");
        startTimerHz (30);
    }
    ~Visualizer() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        using namespace afterimage;
        auto b = getLocalBounds().toFloat();
        const auto& v = engine.viz;
        const bool frozen = v.frozen.load (std::memory_order_relaxed);
        const auto accent = frozen ? ice : violet;

        g.setGradientFill (juce::ColourGradient (panel.brighter (0.05f), 0.f, 0.f, bg, 0.f, b.getHeight(), false));
        g.fillRoundedRectangle (b, 10.f);
        g.setColour (frozen ? ice.withAlpha (0.8f) : edge); g.drawRoundedRectangle (b.reduced (0.5f), 10.f, frozen ? 2.f : 1.f);

        auto area = b.reduced (12.f, 10.f);
        g.setColour (edge.withAlpha (0.5f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / 8.f), area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

        // buffer waveform
        const int vis = juce::jmax (1, v.visibleBins.load (std::memory_order_relaxed));
        const int wb = v.writeBin.load (std::memory_order_relaxed);
        juce::Path wave;
        const int cols = juce::jmax (2, (int) area.getWidth() / 2);
        for (int i = 0; i < cols; ++i)
        {
            const float u = (float) i / (float) (cols - 1);
            const int bin = ((wb - (int) ((1.f - u) * (float) (vis - 1))) % kVizBins + kVizBins) % kVizBins;
            const float pk = std::sqrt (juce::jlimit (0.f, 1.f, v.peaks[bin].load (std::memory_order_relaxed) * 1.6f));
            const float x = area.getX() + u * area.getWidth(), hh = pk * area.getHeight() * 0.42f;
            wave.addRectangle (x, area.getCentreY() - hh, 1.5f, juce::jmax (1.f, hh * 2.f));
        }
        g.setColour (purple.withAlpha (frozen ? 0.75f : 0.5f)); g.fillPath (wave);

        // position marker
        const float pos = posParam != nullptr ? posParam->load() : 0.f;
        const float mx = area.getX() + (1.f - pos) * area.getWidth();
        g.setColour (ice.withAlpha (0.55f)); g.drawVerticalLine ((int) mx, area.getY(), area.getBottom());
        g.setFont (juce::FontOptions (10.f, juce::Font::bold)); g.drawText ("POS", (int) mx - 14, (int) area.getY(), 28, 12, juce::Justification::centred);

        // grains: lane stacking shows overlap
        constexpr int lanes = 12;
        const float laneH = area.getHeight() / (float) lanes;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            const auto& gr = v.grains[i];
            const float env = gr.env.load (std::memory_order_relaxed);
            if (env <= 0.001f) continue;
            const float dn = gr.delayNorm.load (std::memory_order_relaxed), ln = gr.lenNorm.load (std::memory_order_relaxed);
            const float pan = gr.pan.load (std::memory_order_relaxed);
            const float x = area.getX() + (1.f - dn) * area.getWidth();
            const float w = juce::jmax (3.f, ln * area.getWidth());
            const float y = area.getY() + (float) (i % lanes) * laneH;
            const auto c = violet.interpolatedWith (ice, juce::jlimit (0.f, 1.f, 0.5f + 0.5f * pan));
            g.setColour (c.withAlpha (env * 0.9f)); g.fillRoundedRectangle (x, y + 1.f, juce::jmin (w, area.getRight() - x), laneH - 2.f, 2.f);
            g.setColour (juce::Colours::white.withAlpha (env * 0.9f)); g.fillRect (x, y + 1.f, 1.5f, laneH - 2.f); // play-head
        }

        // write head
        g.setColour (frozen ? ice.withAlpha (0.2f) : accent.withAlpha (0.9f));
        g.fillRect (area.getRight() - 2.f, area.getY(), 2.f, area.getHeight());

        // readouts
        g.setColour (dim); g.setFont (juce::FontOptions (11.f));
        g.drawText (juce::String::formatted ("GRAINS %d   OVERLAP %.1fx", v.activeGrains.load(), (double) v.overlap.load()),
                    area.toNearestInt().removeFromBottom (14), juce::Justification::centredLeft);
        g.drawText ("4 s BUFFER  <  OLDEST ...... NOW  >", area.toNearestInt().removeFromBottom (14), juce::Justification::centredRight);
        if (frozen)
        {
            g.setColour (ice); g.setFont (juce::FontOptions (18.f, juce::Font::bold));
            g.drawText ("FROZEN", area.toNearestInt().removeFromTop (24), juce::Justification::centredRight);
        }
    }

private:
    void timerCallback() override { repaint(); }
    afterimage::GranularEngine& engine;
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* posParam = nullptr;
};
