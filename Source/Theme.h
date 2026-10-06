// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
namespace aura
{
inline const juce::Colour background { 0xff10151c }, panel { 0xff090f15 }, line { 0xff34404b };
inline const juce::Colour text { 0xffd9e0e7 }, muted { 0xff94a2b0 }, accent { 0xff54d5fa }, violet { 0xffb776f6 }, gold { 0xffe9c66b };
inline juce::Colour spectralColour(float x, float sweetening = 1.0f)
{
    constexpr std::array<juce::uint32, 6> colours { 0xff6478ff, 0xff8856ee, 0xffd754ef, 0xffef66c2, 0xffedce6d, 0xff71ebae };
    const auto p = juce::jlimit(0.0f, 4.999f, x * 5);
    const auto i = static_cast<std::size_t>(p);
    return juce::Colour(colours[i]).interpolatedWith(juce::Colour(colours[i + 1]), p - static_cast<float>(i))
        .withSaturation(0.22f + 0.76f * juce::jlimit(0.0f, 1.0f, sweetening));
}
inline void steel(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(juce::Colours::black.withAlpha(0.7f)); g.fillRoundedRectangle(r.translated(0, 2), 7);
    juce::ColourGradient finish(juce::Colour(0xffb8bcc0), r.getX(), r.getY(), juce::Colour(0xff626b73), r.getRight(), r.getBottom(), false);
    finish.addColour(0.15, juce::Colour(0xff929ba3)); finish.addColour(0.55, juce::Colour(0xffadb1b5));
    g.setGradientFill(finish); g.fillRoundedRectangle(r, 6);
    g.saveState(); g.reduceClipRegion(r.reduced(1).toNearestInt());
    for (int y = static_cast<int>(r.getY()) + 2; y < r.getBottom() - 2; y += 2)
    {
        g.setColour((y % 6 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha(y % 6 == 0 ? 0.065f : 0.035f));
        g.drawHorizontalLine(y, r.getX() + 2, r.getRight() - 2);
    }
    g.restoreState(); g.setColour(juce::Colour(0xffd1d7db).withAlpha(0.85f)); g.drawRoundedRectangle(r.reduced(0.7f), 6, 1);
    g.setColour(juce::Colours::black.withAlpha(0.28f)); g.drawRoundedRectangle(r.reduced(2), 5, 1);
}
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, panel); setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, line); setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, text); setColour(juce::PopupMenu::highlightedBackgroundColourId, line);
        setColour(juce::Slider::textBoxTextColourId, text); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::trackColourId, accent); setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::TextButton::buttonColourId, panel); setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4f442d));
        setColour(juce::TextButton::textColourOffId, text); setColour(juce::TextButton::textColourOnId, gold);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13)); }
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(juce::Font(juce::FontOptions(static_cast<bool>(slider.getProperties()["heroKnob"]) ? 18.0f : 11.0f).withStyle("Bold")));
        return label;
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced(0.6f);
        g.setColour(b.getToggleState() ? juce::Colour(0xff4b402a) : over ? line : panel.brighter(down ? 0.1f : 0.03f));
        g.fillRoundedRectangle(r, 4); g.setColour(b.getToggleState() ? gold.withAlpha(0.75f) : line.brighter(0.2f)); g.drawRoundedRectangle(r, 4, 1);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        if (static_cast<bool>(b.getProperties()["settingsButton"]))
        {
            const auto c = b.getLocalBounds().toFloat().getCentre();
            juce::Path gear;
            for (int i = 0; i < 32; ++i)
            {
                const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(i) / 32;
                const auto radius = i % 4 == 1 || i % 4 == 2 ? 9.0f : 6.8f;
                const juce::Point<float> point { c.x + std::sin(angle) * radius, c.y - std::cos(angle) * radius };
                if (i == 0) gear.startNewSubPath(point); else gear.lineTo(point);
            }
            gear.closeSubPath(); g.setColour(text); g.strokePath(gear, juce::PathStrokeType(1.2f));
            g.drawEllipse(c.x - 2.6f, c.y - 2.6f, 5.2f, 5.2f, 1.2f); return;
        }
        if (static_cast<bool>(b.getProperties()["powerButton"]))
        {
            const auto c = b.getLocalBounds().toFloat().getCentre();
            juce::Path p; p.addCentredArc(c.x, c.y, 8, 8, 0, 0.65f, 5.63f, true);
            g.setColour(b.getToggleState() ? muted : accent); g.strokePath(p, juce::PathStrokeType(1.8f));
            g.drawLine(c.x, c.y - 11, c.x, c.y - 2, 1.8f); return;
        }
        g.setColour(b.getToggleState() ? gold : text); g.setFont(juce::Font(juce::FontOptions(11)));
        g.drawText(b.getButtonText(), b.getLocalBounds().reduced(3), juce::Justification::centred);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider& slider) override
    {
        const auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(9);
        const auto radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        const auto colour = slider.getProperties().contains("podColour") ? juce::Colour(static_cast<juce::uint32>(static_cast<int>(slider.getProperties()["podColour"]))) : accent;
        const auto angle = start + position * (end - start);
        juce::Path track, active;
        track.addCentredArc(c.x, c.y, radius + 3, radius + 3, 0, start, end, true);
        active.addCentredArc(c.x, c.y, radius + 3, radius + 3, 0, start, angle, true);
        g.setColour(juce::Colours::black.withAlpha(0.8f)); g.strokePath(track, juce::PathStrokeType(5));
        g.setColour(colour.withAlpha(0.12f)); g.strokePath(active, juce::PathStrokeType(12));
        g.setColour(colour.withAlpha(0.4f)); g.strokePath(active, juce::PathStrokeType(6));
        g.setColour(colour); g.strokePath(active, juce::PathStrokeType(2));
        g.setColour(juce::Colours::black.withAlpha(0.65f)); g.fillEllipse(c.x - radius + 1, c.y - radius + 3, radius * 2, radius * 2);
        juce::ColourGradient rim(juce::Colour(0xffd7dee4), c.x - radius, c.y - radius, juce::Colour(0xff131a20), c.x + radius, c.y + radius, false);
        rim.addColour(0.45, juce::Colour(0xff64717b)); rim.addColour(0.8, juce::Colour(0xffe3e7ea));
        g.setGradientFill(rim); g.fillEllipse(c.x - radius, c.y - radius, radius * 2, radius * 2);
        const auto inner = radius - 4;
        // Alternating specular sectors form a machined radial metal face.
        for (int i = 0; i < 64; ++i)
        {
            const auto a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 64;
            const auto b = a + juce::MathConstants<float>::twoPi / 64 + 0.002f;
            const auto shade = juce::jlimit(0.0f, 1.0f, 0.5f + 0.37f * std::cos(2 * a - 0.8f) + 0.08f * std::cos(6 * a));
            juce::Path wedge; wedge.addPieSegment(c.x - inner, c.y - inner, inner * 2, inner * 2, a, b, 0);
            g.setColour(juce::Colour(0xff3c4650).interpolatedWith(juce::Colour(0xffe6e9eb), shade)); g.fillPath(wedge);
        }
        for (float rr = inner - 1; rr > 2; rr -= 2)
        { g.setColour(juce::Colours::white.withAlpha(0.035f)); g.drawEllipse(c.x - rr, c.y - rr, rr * 2, rr * 2, 0.5f); }
        g.setColour(juce::Colours::black.withAlpha(0.75f)); g.drawEllipse(c.x - inner, c.y - inner, inner * 2, inner * 2, 1);
        const auto px = c.x + std::sin(angle) * inner * 0.75f, py = c.y - std::cos(angle) * inner * 0.75f;
        g.setColour(colour.withAlpha(0.2f)); g.fillEllipse(px - 4, py - 4, 8, 8);
        g.setColour(colour.brighter(0.4f)); g.drawLine(c.x + std::sin(angle) * inner * 0.48f, c.y - std::cos(angle) * inner * 0.48f, px, py, 2);
        if (slider.getProperties().contains("preservationLED"))
        {
            const auto level = juce::jlimit(0.0f, 1.0f, static_cast<float>(slider.getProperties()["preservationLED"]));
            for (int i = 0; i < 28; ++i)
            {
                const auto a = start + (end - start) * (static_cast<float>(i) + 0.5f) / 28;
                const auto xx = c.x + std::sin(a) * (radius + 8), yy = c.y - std::cos(a) * (radius + 8);
                g.setColour((static_cast<float>(i) + 0.5f) / 28 < level ? colour.brighter(0.5f) : juce::Colours::black.withAlpha(0.45f));
                g.fillEllipse(xx - 1, yy - 1, 2, 2);
            }
        }
    }
};
}
