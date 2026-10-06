// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_opengl/juce_opengl.h>
#include "SpectralVisualizer.h"
#include "KeyboardSelector.h"
#include "RackDisplays.h"
#include "AccessibleControls.h"
#include "SettingsPanel.h"
class AuraAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AuraAudioProcessorEditor(AuraAudioProcessor&);
    ~AuraAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void chooseCustom(bool);
    void showSettings(bool);
    AuraAudioProcessor& auraProcessor;
    aura::LookAndFeel look;
    SpectralVisualizer visualizer;
    KeyboardSelector keyboard;
    RackSpectrum rackSpectrum;
    RackMeter rackMeter;
    SettingsPanel settings;
    aura::ChoiceBox scales, tonic, factoryPresets;
    std::array<juce::Slider, 10> sliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment, tonicAttachment;
    juce::TextButton transientBypass { "BYPASS" }, mute { "M" }, solo { "S" }, bypass { "BYPASS" }, power, menu { "⚙" }, oversampling { "1x" }, scaleButton { "SCALE" }, customButton { "CUSTOM" };
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 5> buttonAttachments;
    std::array<juce::Rectangle<int>, 5> pods;
    juce::Rectangle<int> outputPod, headerPreset;
    int lastPreset = 0, displayedProgram = -1; bool displayedModified = false;
    juce::TooltipWindow tooltips { this, 700 };
    juce::OpenGLContext openGL;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessorEditor)
};
