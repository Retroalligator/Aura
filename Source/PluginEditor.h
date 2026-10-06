// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_opengl/juce_opengl.h>
#include "SpectralVisualizer.h"
#include "KeyboardSelector.h"
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
    SettingsPanel settings;
    aura::ChoiceBox scales, tonic, factoryPresets, channelMode;
    std::array<juce::Slider, 12> sliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment, tonicAttachment, channelAttachment;
    juce::TextButton transientBypass { "BYPASS" }, delta { "LISTEN DELTA" }, bypass { "BYPASS" }, menu { "⚙" }, oversampling { "1x" }, scaleButton { "SCALE" }, customButton { "CUSTOM" };
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3> buttonAttachments;
    std::array<juce::Rectangle<int>, 5> pods;
    juce::Rectangle<int> outputPod, spatialPod, deltaPod, headerPreset;
    int lastPreset = 0, displayedProgram = -1; bool displayedModified = false;
    juce::TooltipWindow tooltips { this, 700 };
    juce::OpenGLContext openGL;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessorEditor)
};
