// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "KeyboardSelector.h"
namespace { bool sharp(int note) { return note == 1 || note == 3 || note == 6 || note == 8 || note == 10; } }
void KeyboardSelector::Key::paintButton(juce::Graphics& g, bool over, bool down)
{
    const auto black = sharp(note);
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    const auto colour = active ? (tonic || black ? aura::accent : aura::gold) : black ? juce::Colour(0xff252d35) : juce::Colour(0xffabb0b4);
    juce::ColourGradient gradient(colour.brighter(over ? 0.25f : 0.12f), 0, 0, colour.darker(down ? 0.45f : 0.28f), 0, r.getBottom(), false);
    g.setGradientFill(gradient); g.fillRoundedRectangle(r, 2);
    g.setColour(juce::Colour(0xff111820)); g.drawRoundedRectangle(r, 2, 1);
    if (active) { g.setColour(colour.brighter(0.45f)); g.drawHorizontalLine(2, r.getX() + 1, r.getRight() - 1); }
    g.setColour(active || !black ? juce::Colour(0xff17212a) : aura::text);
    g.setFont(juce::Font(juce::FontOptions(9).withStyle(tonic ? "Bold" : "Regular")));
    g.drawText(getName(), r.withTrimmedTop(r.getHeight() - 13), juce::Justification::centred);
}
KeyboardSelector::KeyboardSelector(juce::AudioProcessorValueTreeState& s) : state(s)
{
    setTitle("Scale keyboard"); setDescription("Highlighted notes transpose with the tonic. Click a key to edit custom intervals.");
    for (int i = 0; i < 12; ++i)
    {
        auto& key = keys[static_cast<std::size_t>(i)]; key = std::make_unique<Key>(i);
        key->setClickingTogglesState(true);
        key->setTitle(juce::String(aura::noteNames[static_cast<std::size_t>(i)]) + " pitch class");
        key->setTooltip("Toggle " + juce::String(aura::noteNames[static_cast<std::size_t>(i)]) + " in the custom scale");
        key->onClick = [this, i] { toggle(i); }; addAndMakeVisible(*key);
    }
    timerCallback(); startTimerHz(30);
}
void KeyboardSelector::resized()
{
    const auto w = static_cast<float>(getWidth()) / 7;
    int white = 0;
    for (int i = 0; i < 12; ++i)
    {
        auto& key = *keys[static_cast<std::size_t>(i)];
        if (!sharp(i)) key.setBounds(juce::roundToInt(w * static_cast<float>(white++)), 0, juce::roundToInt(w), getHeight());
    }
    constexpr std::array<int, 5> notes { 1, 3, 6, 8, 10 }, boundaries { 1, 2, 4, 5, 6 };
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        auto& key = *keys[static_cast<std::size_t>(notes[i])];
        key.setBounds(juce::roundToInt(w * (static_cast<float>(boundaries[i]) - 0.3f)), 0, juce::roundToInt(w * 0.6f), juce::roundToInt(getHeight() * 0.62f)); key.toFront(false);
    }
}
void KeyboardSelector::timerCallback()
{
    const auto mode = static_cast<int>(state.getRawParameterValue("scaleMode")->load());
    const auto root = static_cast<int>(state.getRawParameterValue("scaleTonic")->load());
    const auto mask = aura::noteMask(mode, static_cast<int>(state.getRawParameterValue("customNoteBits")->load()), root);
    if (mask == previousMask && root == previousTonic) return;
    previousMask = mask; previousTonic = root;
    for (int i = 0; i < 12; ++i)
    {
        auto& key = *keys[static_cast<std::size_t>(i)]; key.active = (mask & (1 << i)) != 0; key.tonic = i == root;
        key.setToggleState(key.active, juce::dontSendNotification); key.repaint();
    }
}
void KeyboardSelector::toggle(int note)
{
    const auto mode = static_cast<int>(state.getRawParameterValue("scaleMode")->load());
    const auto root = static_cast<int>(state.getRawParameterValue("scaleTonic")->load());
    const auto base = mode < 7 ? aura::scaleMasks[static_cast<std::size_t>(mode)] : static_cast<std::uint16_t>(state.getRawParameterValue("customNoteBits")->load());
    const auto mask = base ^ (1 << ((note - root + 12) % 12));
    auto* bits = state.getParameter("customNoteBits");
    bits->beginChangeGesture(); bits->setValueNotifyingHost(bits->convertTo0to1(static_cast<float>(mask))); bits->endChangeGesture();
    auto* choice = state.getParameter("scaleMode");
    choice->beginChangeGesture(); choice->setValueNotifyingHost(choice->convertTo0to1(7)); choice->endChangeGesture(); timerCallback();
}
