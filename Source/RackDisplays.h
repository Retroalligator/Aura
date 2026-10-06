// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "SpectralVisualizer.h"
class RackSpectrum final : public juce::Component
{
public:
    explicit RackSpectrum(SpectralVisualizer& v) : visualizer(v)
    {
        addAndMakeVisible(pre); addAndMakeVisible(post); post.setToggleState(true, juce::dontSendNotification);
        pre.onClick = [this] { showPre = true; pre.setToggleState(true, juce::dontSendNotification); post.setToggleState(false, juce::dontSendNotification); repaint(); };
        post.onClick = [this] { showPre = false; pre.setToggleState(false, juce::dontSendNotification); post.setToggleState(true, juce::dontSendNotification); repaint(); };
        pre.setTitle("Analyze input"); post.setTitle("Analyze final output");
        post.setTooltip("FFT of the actual final audio, after Mix, output gain, Solo and Mute");
    }
    void update()
    {
        const auto& frame = showPre ? visualizer.inputFrame() : visualizer.postFrame();
        const auto size = juce::jmax(1, frame.analysisFftSize), bins = juce::jlimit(0, aura::binCount, frame.validBins);
        if (std::abs(rate - frame.sampleRate) > 0.5f || analysisFftSize != size || validBins != bins) values.fill(0);
        rate = frame.sampleRate; analysisFftSize = size; validBins = bins;
        const auto& source = showPre ? frame.input : frame.output;
        for (std::size_t k = 0; k < values.size(); ++k)
        { const auto target = k < static_cast<std::size_t>(validBins) ? source[k] : 0.0f; values[k] += (target - values[k]) * (target > values[k] ? 0.6f : 0.12f); }
        repaint();
    }
    void resized() override { pre.setBounds(getWidth() - 111, 8, 45, 22); post.setBounds(getWidth() - 61, 8, 49, 22); }
    void paint(juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(aura::panel); g.fillRoundedRectangle(full, 6); g.setColour(aura::line); g.drawRoundedRectangle(full, 6, 1);
        g.setColour(aura::text); g.setFont(juce::Font(juce::FontOptions(13))); g.drawText("SPECTRUM", 12, 8, 120, 22, juce::Justification::centredLeft);
        const auto r = full.withTrimmedTop(40).withTrimmedBottom(22).withTrimmedLeft(29).withTrimmedRight(13);
        g.setFont(10);
        for (int db = 0; db >= -60; db -= 12)
        { const auto y = r.getBottom() - r.getHeight() * static_cast<float>(db + 60) / 60; g.setColour(aura::line.withAlpha(0.6f)); g.drawHorizontalLine(juce::roundToInt(y), r.getX(), r.getRight()); g.setColour(aura::muted); g.drawText(juce::String(db), juce::Rectangle<float>(1, y - 6, 25, 12), juce::Justification::centredRight); }
        for (auto hz : { 50.0f, 200.0f, 1000.0f, 5000.0f, 20000.0f })
        { const auto x = r.getX() + r.getWidth() * std::log(hz / 20) / std::log(1000.0f); g.setColour(aura::line); g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom()); g.setColour(aura::muted); g.drawText(hz >= 1000 ? juce::String(hz / 1000, 0) + "k" : juce::String(hz, 0), juce::Rectangle<float>(x - 14, r.getBottom() + 3, 28, 14), juce::Justification::centred); }
        juce::Path path; bool first = true;
        for (int k = 1; k < validBins; ++k)
        { const auto hz = static_cast<float>(k) * rate / static_cast<float>(analysisFftSize); if (hz < 20 || hz > 20000 || hz > rate * 0.5f) continue; const auto x = r.getX() + r.getWidth() * std::log(hz / 20) / std::log(1000.0f); const auto db = juce::Decibels::gainToDecibels(values[static_cast<std::size_t>(k)], -60.0f); const auto y = r.getBottom() - r.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 60) / 60); if (first) { path.startNewSubPath(x, y); first = false; } else path.lineTo(x, y); }
        juce::ColourGradient gradient(aura::accent, r.getX(), 0, aura::violet, r.getRight(), 0, false);
        auto glow = gradient; glow.multiplyOpacity(0.16f); g.setGradientFill(glow); g.strokePath(path, juce::PathStrokeType(5));
        g.setGradientFill(gradient); g.strokePath(path, juce::PathStrokeType(1.2f));
    }
private:
    SpectralVisualizer& visualizer;
    juce::TextButton pre { "PRE" }, post { "POST" };
    bool showPre = false; float rate = 48000;
    int analysisFftSize = aura::fftSize, validBins = aura::binCount;
    std::array<float, aura::binCount> values {};
};
class RackMeter final : public juce::Component
{
public:
    explicit RackMeter(AuraAudioProcessor& p) : processor(p)
    {
        selector.addItem("LUFS M", 1); selector.addItem("RMS", 2); selector.addItem("SAMPLE PEAK", 3); selector.setSelectedId(1);
        selector.setTitle("Meter display"); selector.setTooltip("Momentary LUFS: K weighted 400 ms. RMS and sample peak are dBFS."); addAndMakeVisible(selector);
    }
    void resized() override { selector.setBounds(12, 34, juce::jmin(145, getWidth() / 2), 25); }
    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f); g.setColour(aura::panel); g.fillRoundedRectangle(r, 6); g.setColour(aura::line); g.drawRoundedRectangle(r, 6, 1);
        const auto lufs = processor.metering.lufs.load(), rms = processor.metering.rms.load(), peak = processor.metering.peakDb.load();
        const auto value = selector.getSelectedId() == 1 ? lufs : selector.getSelectedId() == 2 ? rms : peak;
        g.setColour(aura::text); g.setFont(13); g.drawText("METERING", 12, 10, 160, 18, juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(25))); g.drawText(value < -80 ? "--" : juce::String(value, 1), getWidth() - 107, 12, 92, 29, juce::Justification::centredRight);
        g.setFont(10); g.setColour(aura::muted); g.drawText(selector.getSelectedId() == 1 ? "LUFS MOMENTARY" : "dBFS", getWidth() - 135, 43, 120, 16, juce::Justification::centredRight);
        const auto left = 42.0f, width = static_cast<float>(getWidth()) - 59, cell = width / 32;
        for (int row = 0; row < 2; ++row)
        {
            const auto level = row == 0 ? juce::jlimit(0.0f, 1.0f, (value + 36) / 36) : juce::jlimit(0.0f, 1.0f, (peak + 36) / 36);
            const auto y = 76.0f + static_cast<float>(row) * 15;
            g.setColour(aura::muted); g.setFont(8); g.drawText(row == 0 ? "LEVEL" : "PEAK", juce::Rectangle<float>(4, y, 33, 12), juce::Justification::centredRight);
            for (int i = 0; i < 32; ++i)
            {
                const auto f = (static_cast<float>(i) + 0.5f) / 32;
                const auto colour = f < 0.65f ? juce::Colour(0xff6ee26c) : f < 0.88f ? aura::gold : juce::Colour(0xffef7154);
                g.setColour(f < level ? colour : colour.withAlpha(0.12f)); g.fillRoundedRectangle(left + static_cast<float>(i) * cell, y, cell - 2, 12, 1);
            }
        }
        g.setColour(aura::muted); g.setFont(10);
        for (int i = 0; i < 4; ++i) g.drawText(juce::String(-36 + i * 12), juce::Rectangle<float>(left - 12 + width * static_cast<float>(i) / 3, 109, 25, 15), juce::Justification::centred);
        g.drawText(selector.getSelectedId() == 1 ? "400 ms  /  K-WEIGHTED" : selector.getSelectedId() == 2 ? "400 ms  /  RMS" : "400 ms  /  SAMPLE PEAK", 12, getHeight() - 23, getWidth() - 24, 16, juce::Justification::centredLeft);
    }
private:
    AuraAudioProcessor& processor; juce::ComboBox selector;
};
