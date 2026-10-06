// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "PluginProcessor.h"
#include "AccessibleControls.h"
#include "Theme.h"

class SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel(AuraAudioProcessor& p) : processor(p)
    {
        setTitle("Processing settings");
        for (auto* component : std::array<juce::Component*, 7> { &oversampling, &quality, &reducedMotion, &reset, &done, &status, &description }) addAndMakeVisible(component);
        oversampling.setTitle("Oversampling factor");
        oversampling.setTooltip("1x, 2x or 4x processing. The analysis window and host latency stay constant.");
        for (int i = 0; i < 3; ++i) oversampling.addItem(juce::String(1 << i) + "x", i + 1);
        oversampling.onChange = [this] { if (oversampling.getSelectedId() > 0) processor.setOversamplingMode(oversampling.getSelectedId() - 1); refresh(); };
        quality.setTitle("Resampling quality"); quality.addItem("Standard", 1); quality.addItem("High", 2);
        quality.setTooltip("Standard uses shorter anti-alias filters. High uses steeper filters with greater alias rejection. Quality applies at 2x or 4x.");
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "processingQuality", quality);
        quality.onChange = [this] { refresh(); };
        reducedMotion.setTitle("Reduced motion"); reducedMotion.setClickingTogglesState(true);
        reducedMotion.setTooltip("Disable particle trails and transient flashes; retain spectrum and meters.");
        reducedMotion.onClick = [this] { if (onReducedMotion) onReducedMotion(reducedMotion.getToggleState()); };
        reset.setTitle("Reset processing to Default"); reset.onClick = [this] { processor.setCurrentProgram(0); refresh(); };
        done.setTitle("Close processing settings"); done.onClick = [this] { if (onClose) onClose(); };
        status.setColour(juce::Label::textColourId, aura::gold); status.setFont(juce::Font(juce::FontOptions(12)));
        description.setColour(juce::Label::textColourId, aura::muted); description.setFont(juce::Font(juce::FontOptions(12)));
        refresh();
    }
    void refresh()
    {
        const auto mode = processor.getOversamplingMode();
        oversampling.setSelectedId(mode + 1, juce::dontSendNotification);
        quality.setEnabled(mode != 0);
        const auto detail = mode == 0 ? "Native processing. Filter quality applies at 2x or 4x."
            : processor.getProcessingQuality() == 0 ? "Shorter anti-alias filters for lower CPU use."
            : "Steeper anti-alias filters for greater alias rejection.";
        description.setText(detail, juce::dontSendNotification);
        status.setText(processor.isProcessingChangePending() ? "Applying settings..." : "Ready | " + juce::String(1 << mode) + "x", juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff060a10)); g.fillRoundedRectangle(r, 8);
        juce::ColourGradient finish(juce::Colour(0xff25313d), 0, 0, aura::panel, 0, r.getBottom(), false);
        g.setGradientFill(finish); g.fillRoundedRectangle(r.reduced(2), 7);
        g.setColour(aura::line.brighter(0.35f)); g.drawRoundedRectangle(r.reduced(1), 8, 1);
        g.setColour(aura::text); g.setFont(juce::Font(juce::FontOptions(17).withStyle("Bold")));
        g.drawText("PROCESSING SETTINGS", 20, 15, getWidth() - 40, 25, juce::Justification::centredLeft);
        g.setFont(12); g.setColour(aura::text);
        g.drawText("Oversampling", 20, 63, 135, 28, juce::Justification::centredLeft);
        g.drawText("Filter quality", 20, 104, 135, 28, juce::Justification::centredLeft);
        g.setColour(aura::line); g.drawHorizontalLine(238, 20, static_cast<float>(getWidth() - 20));
        g.setFont(11); g.setColour(aura::muted);
        g.drawText("8192-sample analysis | fixed host latency", 20, 247, getWidth() - 40, 20, juce::Justification::centredLeft);
    }
    void resized() override
    {
        oversampling.setBounds(167, 63, getWidth() - 187, 28);
        quality.setBounds(167, 104, getWidth() - 187, 28);
        description.setBounds(16, 142, getWidth() - 32, 39);
        reducedMotion.setBounds(20, 190, getWidth() - 40, 29);
        status.setBounds(16, 273, 150, 25);
        reset.setBounds(20, 312, 154, 29); done.setBounds(getWidth() - 99, 312, 79, 29);
    }
    std::function<void(bool)> onReducedMotion;
    std::function<void()> onClose;
private:
    AuraAudioProcessor& processor;
    aura::ChoiceBox oversampling, quality;
    juce::TextButton reducedMotion { "Reduced motion" }, reset { "Reset to Default" }, done { "Done" };
    juce::Label status, description;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
};
