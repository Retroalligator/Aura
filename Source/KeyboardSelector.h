// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
class KeyboardSelector final : public juce::Component, private juce::Timer
{
    class Key final : public juce::Button
    {
    public:
        explicit Key(int pc) : juce::Button(aura::noteNames[static_cast<std::size_t>(pc)]), note(pc) {}
        void paintButton(juce::Graphics&, bool, bool) override;
        int note; bool active = false, tonic = false;
    };
public:
    explicit KeyboardSelector(juce::AudioProcessorValueTreeState&);
    ~KeyboardSelector() override { stopTimer(); }
    void resized() override;
private:
    void timerCallback() override;
    void toggle(int note);
    juce::AudioProcessorValueTreeState& state;
    std::array<std::unique_ptr<Key>, 12> keys;
    int previousMask = -1, previousTonic = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardSelector)
};
