// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SpectralProcessor.h"
#include "OutputMonitor.h"
#include "FactoryPresets.h"

class AuraAudioProcessor final : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener
{
public:
    AuraAudioProcessor();
    ~AuraAudioProcessor() override;
    const juce::String getName() const override { return "Aura"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return static_cast<double>(getLatencySamples() + aura::fftSize) / (getSampleRate() > 0 ? getSampleRate() : 48000.0); }
    int getNumPrograms() override { return static_cast<int>(aura::factoryPresets.size()); }
    int getCurrentProgram() override { return currentProgram.load(std::memory_order_relaxed); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    bool isCurrentProgramModified() const noexcept;
    int getOversamplingMode() const noexcept { return requestedOversampling.load(std::memory_order_relaxed); }
    int getProcessingQuality() const noexcept { return juce::jlimit(0, 1, juce::roundToInt(quality->load(std::memory_order_relaxed))); }
    int getActiveProcessingPath() const noexcept { return activePath.load(std::memory_order_relaxed); }
    bool isProcessingChangePending() const noexcept { const auto mode = getOversamplingMode(); return processingTransition.load(std::memory_order_relaxed) || getActiveProcessingPath() != (mode == 0 ? 0 : 1 + (mode - 1) * 2 + getProcessingQuality()); }
    void setOversamplingMode(int);
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    aura::SpectrumFifo spectrumFifo;
    aura::SpectrumFifo outputSpectrumFifo;
    aura::LoudnessMeter metering;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
private:
    void process(juce::AudioBuffer<float>&, bool bypassed) noexcept;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}
    struct ProcessingState;
    std::unique_ptr<ProcessingState> processing;
    juce::SmoothedValue<float> mixSmooth, gainSmooth;
    std::array<juce::RangedAudioParameter*, aura::parameterIds.size()> programParameters {};
    std::array<std::atomic<float>*, aura::parameterIds.size()> programValues {};
    std::atomic<int> currentProgram { 0 };
    std::atomic<float>* scale = nullptr;
    std::atomic<float>* custom = nullptr;
    std::atomic<float>* low = nullptr;
    std::atomic<float>* high = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* transientPreserve = nullptr;
    std::atomic<float>* formantPreserve = nullptr;
    std::atomic<float>* tonic = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* transientBypass = nullptr;
    std::atomic<float>* formantShift = nullptr;
    std::atomic<float>* formantTension = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* outputMute = nullptr;
    std::atomic<float>* soloWet = nullptr;
    std::atomic<float>* globalBypass = nullptr;
    std::atomic<float>* quality = nullptr;
    std::atomic<int> requestedOversampling { 0 }, activePath { 0 };
    std::atomic<bool> processingTransition { false };
    int legacyOversamplingIndex = -1;
    bool prepared = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessor)
};
