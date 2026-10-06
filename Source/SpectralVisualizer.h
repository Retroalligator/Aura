// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
class SpectralVisualizer final : public juce::Component, private juce::Timer
{
public:
    explicit SpectralVisualizer(AuraAudioProcessor&);
    ~SpectralVisualizer() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void setReducedMotion(bool value) { reducedMotion = value; if (value) { particles.fill({}); flash = 0; } }
    const aura::SpectrumFrame& inputFrame() const noexcept { return frame; }
    const aura::SpectrumFrame& postFrame() const noexcept { return post; }
    std::function<void(float, float)> onPreservation;
    std::function<void()> onFrame;
private:
    void timerCallback() override;
    juce::Rectangle<float> plot() const;
    float frequencyX(float) const;
    float xFrequency(float) const;
    float magnitudeY(float) const;
    void finishGesture();
    float random() noexcept { seed = seed * 1664525u + 1013904223u; return static_cast<float>(seed >> 8) / 16777216.0f; }
    struct Particle { float x = 0, db = -60, alpha = 0, velocity = 0; };
    AuraAudioProcessor& processor;
    aura::SpectrumFrame frame, post;
    std::array<float, aura::binCount> input {}, output {}, envelope {};
    std::array<float, 192> waveInput {}, waveOutput {}, waveEnvelope {};
    std::array<Particle, 4096> particles {};
    std::uint32_t seed = 42; std::size_t nextParticle = 0;
    std::uint32_t lastInputFrame = 0, lastPostFrame = 0;
    float flash = 0, punch = 0, throat = 0, hoverX = -1, colourAmount = 0.5f;
    bool reducedMotion = false;
    juce::RangedAudioParameter* dragging = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralVisualizer)
};
