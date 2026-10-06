// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "PluginProcessor.h"

// A silent developer harness: it exercises DSP and GUI without opening an
// audio device or requesting microphone permission. Not shipped in the DMG.
class Preview final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Aura Preview"; }
    const juce::String getApplicationVersion() override { return "1.5.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override
    {
        for (const auto& value : { std::pair { "amount", 0.74f }, std::pair { "freqHigh", 14500.0f }, std::pair { "outputGain", -1.5f }, std::pair { "scaleMode", 1.0f } })
        {
            auto* parameter = processor.parameters.getParameter(value.first);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value.second));
        }
        processor.setRateAndBufferSizeDetails(48000, 512);
        processor.prepareToPlay(48000, 512);
        window = std::make_unique<Window>();
        window->setContentOwned(processor.createEditor(), true);
        window->setResizable(true, false);
        window->centreWithSize(1180, 810);
        window->setVisible(true);
        startTimer(11);
    }
    void shutdown() override { stopTimer(); window.reset(); processor.releaseResources(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow("Aura Preview - silent test signal", juce::Colour(0xff101418), allButtons)
        { setUsingNativeTitleBar(true); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    void timerCallback() override
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto hit = sampleCounter++ % 24000 < 128 ? 0.8f * noise() : 0.0f;
            const auto value = hit + static_cast<float>(0.2 * (std::sin(phase) + 0.32 * std::sin(phase * 2) + 0.14 * std::sin(phase * 3)));
            buffer.setSample(0, i, value); buffer.setSample(1, i, value);
            phase = std::fmod(phase + juce::MathConstants<double>::twoPi * 430.0 / 48000.0, juce::MathConstants<double>::twoPi);
        }
        processor.processBlock(buffer, midi);
    }
    AuraAudioProcessor processor;
    juce::AudioBuffer<float> buffer { 2, 512 };
    juce::MidiBuffer midi;
    float noise() noexcept { randomState = randomState * 1664525u + 1013904223u; return static_cast<float>(randomState >> 8) / 8388608.0f - 1.0f; }
    std::uint32_t randomState = 42;
    std::uint64_t sampleCounter = 0;
    double phase = 0;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(Preview)
