// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "PluginEditor.h"
namespace
{
void title(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, int y, int height = 17, float size = 12)
{
    g.setFont(juce::Font(juce::FontOptions(size))); g.drawText(text, r.getX() + 3, r.getY() + y, r.getWidth() - 6, height, juce::Justification::centred);
}
}
AuraAudioProcessorEditor::AuraAudioProcessorEditor(AuraAudioProcessor& p)
    : AudioProcessorEditor(p), auraProcessor(p), visualizer(p), keyboard(p.parameters), rackSpectrum(visualizer), rackMeter(p), settings(p)
{
    setLookAndFeel(&look);
    for (auto* component : std::array<juce::Component*, 7> { &visualizer, &keyboard, &rackSpectrum, &rackMeter, &scales, &tonic, &factoryPresets }) addAndMakeVisible(component);
    for (int i = 0; i < 8; ++i) scales.addItem(aura::scaleNames[static_cast<std::size_t>(i)], i + 1);
    for (int i = 0; i < 12; ++i) tonic.addItem(aura::noteNames[static_cast<std::size_t>(i)], i + 1);
    for (int i = 0; i < p.getNumPrograms(); ++i) factoryPresets.addItem(p.getProgramName(i), i + 1);
    factoryPresets.setTitle("Factory preset manager"); factoryPresets.setTooltip("Load a factory sound; an asterisk indicates edited settings. Presets recall the root, scale and all processing controls.");
    factoryPresets.onChange = [this] { if (factoryPresets.getSelectedId() > 0) auraProcessor.setCurrentProgram(factoryPresets.getSelectedId() - 1); };
    scales.setTitle("Scale Type"); tonic.setTitle("Root Key"); tonic.setTooltip("Transpose the preset or custom interval pattern to a new tonic");
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "scaleMode", scales);
    tonicAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "scaleTonic", tonic);
    lastPreset = juce::jlimit(0, 6, scales.getSelectedId() - 1);
    scales.onChange = [this] { if (scales.getSelectedId() <= 7) lastPreset = scales.getSelectedId() - 1; repaint(); };
    tonic.onChange = [this] { keyboard.repaint(); repaint(); };
    constexpr std::array<const char*, 10> ids { "transientPreserve", "formantPreserve", "amount", "freqLow", "freqHigh", "transientSensitivity", "formantShift", "formantTension", "outputGain", "mix" };
    constexpr std::array<const char*, 10> names { "Transient preserve", "Formant preserve", "Sweetening amount", "Low end", "Top end", "Transient sensitivity", "Throat shape", "Formant tension", "Output gain", "Wet/dry mix" };
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        auto& s = sliders[i]; s.getProperties().set("podColour", static_cast<int>((i == 0 || i == 3 || i == 5 || i == 8 || i == 9 ? aura::accent : i == 4 ? aura::gold : aura::violet).getARGB()));
        s.getProperties().set("heroKnob", i == 2);
        s.setSliderStyle(i == 5 ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(i == 5 ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, i == 2 ? 95 : i == 6 || i == 7 || i == 9 ? 50 : 95, i == 2 ? 25 : 18);
        s.setTitle(names[i]); s.setColour(juce::Slider::textBoxTextColourId, i < 8 ? juce::Colour(0xff111923) : aura::text);
        auto* param = p.parameters.getParameter(ids[i]); s.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue())); addAndMakeVisible(s);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, ids[i], s);
        if (i == 3 || i == 4)
        { s.textFromValueFunction = [](double hz) { return hz >= 1000 ? juce::String(hz / 1000, 1) + " kHz" : juce::String(hz, 0) + " Hz"; }; s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() * (t.containsIgnoreCase("k") ? 1000.0 : 1.0); }; }
        else if (i == 6 || i == 8)
        { s.textFromValueFunction = [i](double v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + (i == 6 ? " st" : " dB"); }; s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); }; }
        else { s.textFromValueFunction = [](double v) { return juce::String(v * 100, 0) + "%"; }; s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue() / 100; }; }
        s.updateText();
    }
    sliders[0].getProperties().set("preservationLED", 0.0f); sliders[1].getProperties().set("preservationLED", 0.0f);
    sliders[0].setTooltip("Preserve estimated percussion in the original-phase bypass"); sliders[1].setTooltip("Reapply the input branch's cepstral envelope after snapping");
    sliders[2].setTooltip("Pull spectral partials toward the nearest selected scale note"); sliders[3].setTooltip("Low boundary of the sweetening range"); sliders[4].setTooltip("High boundary of the sweetening range");
    sliders[5].setTooltip("Adjust how readily HPSS classifies broad energy as percussive"); sliders[6].setTooltip("Shift the preserved envelope shape in semitones without changing the harmonic pitch");
    sliders[7].setTooltip("Envelope detail: broad at low tension, more detailed at high tension"); sliders[8].setTooltip("Output gain after the wet/dry blend"); sliders[9].setTooltip("Blend latency-aligned dry and processed audio");
    std::array<juce::TextButton*, 5> buttons { &transientBypass, &mute, &solo, &bypass, &power };
    constexpr std::array<const char*, 5> boolIds { "transientBypass", "outputMute", "soloWet", "globalBypass", "globalBypass" };
    for (std::size_t i = 0; i < buttons.size(); ++i)
    { addAndMakeVisible(buttons[i]); buttons[i]->setClickingTogglesState(true); buttonAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, boolIds[i], *buttons[i]); }
    power.getProperties().set("powerButton", true); power.setTitle("Bypass Aura"); bypass.setTitle("Bypass Aura");
    transientBypass.setTooltip("Disable transient preservation and send percussion through sweetening"); mute.setTitle("Mute output"); solo.setTitle("Solo processed signal"); solo.setTooltip("Audition 100% wet; retain the Mix setting for returning to the blend");
    addAndMakeVisible(oversampling); oversampling.setTitle("Open processing settings");
    oversampling.setTooltip("Choose Real-time or Studio analysis, 1x, 2x or 4x oversampling, and filter quality in Processing Settings.");
    oversampling.onClick = [this] { showSettings(!settings.isVisible()); };
    addAndMakeVisible(menu); addAndMakeVisible(scaleButton); addAndMakeVisible(customButton);
    scaleButton.onClick = [this] { chooseCustom(false); }; customButton.onClick = [this] { chooseCustom(true); };
    menu.getProperties().set("settingsButton", true); menu.setTitle("Settings");
    menu.onClick = [this] { showSettings(!settings.isVisible()); };
    addChildComponent(settings);
    settings.onReducedMotion = [this](bool enabled) { visualizer.setReducedMotion(enabled); };
    settings.onClose = [this] { showSettings(false); };
    visualizer.onPreservation = [this](float punch, float throat)
    { sliders[0].getProperties().set("preservationLED", punch); sliders[1].getProperties().set("preservationLED", throat); sliders[0].repaint(); sliders[1].repaint(); };
    visualizer.onFrame = [this]
    {
        rackSpectrum.update(); rackMeter.repaint();
        const auto program = auraProcessor.getCurrentProgram(); const auto modified = auraProcessor.isCurrentProgramModified();
        if (program != displayedProgram || modified != displayedModified)
        {
            displayedProgram = program; displayedModified = modified;
            if (modified) factoryPresets.setText(auraProcessor.getProgramName(program) + " *", juce::dontSendNotification);
            else factoryPresets.setSelectedId(program + 1, juce::dontSendNotification);
        }
        const auto osText = juce::String(1 << auraProcessor.getOversamplingMode()) + "x";
        if (oversampling.getButtonText() != osText) oversampling.setButtonText(osText);
        if (settings.isVisible()) settings.refresh();
        repaint(37, getHeight() - 35, getWidth() - 212, 18);
        const auto custom = auraProcessor.parameters.getRawParameterValue("scaleMode")->load() > 6.5f;
        scaleButton.setToggleState(!custom, juce::dontSendNotification); customButton.setToggleState(custom, juce::dontSendNotification);
    };
    setResizable(true, true); setResizeLimits(1000, 740, 1700, 1200); setSize(1180, 810);
    openGL.setComponentPaintingEnabled(true); openGL.setContinuousRepainting(false); openGL.attachTo(*this);
}
AuraAudioProcessorEditor::~AuraAudioProcessorEditor() { visualizer.onFrame = {}; visualizer.onPreservation = {}; openGL.detach(); setLookAndFeel(nullptr); }
void AuraAudioProcessorEditor::showSettings(bool visible)
{
    settings.setVisible(visible); menu.setToggleState(visible, juce::dontSendNotification);
    if (visible) { settings.refresh(); settings.toFront(true); }
}
void AuraAudioProcessorEditor::chooseCustom(bool custom)
{
    auto* mode = auraProcessor.parameters.getParameter("scaleMode");
    if (custom && mode->convertFrom0to1(mode->getValue()) < 7)
    {
        const auto preset = static_cast<int>(mode->convertFrom0to1(mode->getValue())); lastPreset = preset;
        auto* bits = auraProcessor.parameters.getParameter("customNoteBits"); bits->beginChangeGesture(); bits->setValueNotifyingHost(bits->convertTo0to1(aura::scaleMasks[static_cast<std::size_t>(preset)])); bits->endChangeGesture();
    }
    mode->beginChangeGesture(); mode->setValueNotifyingHost(mode->convertTo0to1(static_cast<float>(custom ? 7 : lastPreset))); mode->endChangeGesture();
}
void AuraAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff070b10)); const auto outer = getLocalBounds().toFloat().reduced(5);
    juce::ColourGradient frame(juce::Colour(0xff293541), 0, 0, juce::Colour(0xff0a1016), static_cast<float>(getWidth()), static_cast<float>(getHeight()), false);
    frame.addColour(0.5, juce::Colour(0xff151e27)); g.setGradientFill(frame); g.fillRoundedRectangle(outer, 9);
    g.setColour(juce::Colour(0xff445360)); g.drawRoundedRectangle(outer.reduced(2), 8, 1); g.setColour(juce::Colours::black); g.drawRoundedRectangle(outer.reduced(5), 6, 2);
    for (auto x : { 19.0f, static_cast<float>(getWidth() - 19) }) for (auto y : { 19.0f, static_cast<float>(getHeight() - 19) })
    { g.setColour(juce::Colour(0xff34414d)); g.fillEllipse(x - 3, y - 3, 6, 6); g.setColour(juce::Colour(0xff080c10)); g.drawLine(x - 2, y, x + 2, y, 1); }
    juce::Path prism; prism.startNewSubPath(51, 27); prism.lineTo(32, 49); prism.lineTo(41, 65); prism.lineTo(69, 59); prism.lineTo(71, 37); prism.closeSubPath(); prism.startNewSubPath(51, 27); prism.lineTo(41, 65); prism.lineTo(71, 37); prism.lineTo(32, 49); prism.lineTo(69, 59); prism.lineTo(51, 27);
    g.setColour(aura::violet.withAlpha(0.16f)); g.strokePath(prism, juce::PathStrokeType(7)); juce::ColourGradient logo(aura::accent, 51, 27, aura::violet, 51, 65, false); g.setGradientFill(logo); g.strokePath(prism, juce::PathStrokeType(1.2f));
    g.setColour(aura::text); g.setFont(juce::Font(juce::FontOptions(29))); g.drawText("A U R A", 84, 20, 190, 34, juce::Justification::centredLeft);
    g.setFont(11); g.setColour(aura::muted); g.drawText("SPECTRAL SWEETENER", 87, 54, 160, 18, juce::Justification::centredLeft);
    g.setColour(aura::line); g.drawRoundedRectangle(juce::Rectangle<float>(249, 53, 79, 17), 2, 1); g.setFont(9); g.setColour(aura::text); g.drawText("AU / VST3", 250, 53, 77, 17, juce::Justification::centred);
    g.setColour(aura::panel); g.fillRoundedRectangle(headerPreset.toFloat(), 5); g.setColour(aura::line); g.drawRoundedRectangle(headerPreset.toFloat(), 5, 1);
    g.setColour(aura::muted); g.setFont(10); g.drawText("PRESET", headerPreset.getX() + 11, headerPreset.getY() + 7, 47, 25, juce::Justification::centredLeft);
    g.setFont(9); g.drawText("OVERSAMPLING", getWidth() - 257, 20, 124, 15, juce::Justification::centred);
    constexpr std::array<const char*, 5> podNames { "TRANSIENTS", "FORMANTS", "SWEETENING", "FREQ RANGE", "SCALES" };
    for (std::size_t i = 0; i < pods.size(); ++i)
    { aura::steel(g, pods[i].toFloat()); g.setColour(juce::Colour(0xff0c151e)); title(g, pods[i], podNames[i], 10, 20, 14); }
    g.setColour(juce::Colour(0xff17222c)); title(g, pods[0], "PRESERVE", 31, 18, 11); title(g, pods[1], "PRESERVE", 31, 18, 11); title(g, pods[2], "AMOUNT", 31, 18, 11);
    g.setFont(9); g.drawText("ROOT KEY", pods[4].getX() + 11, pods[4].getY() + 32, 68, 15, juce::Justification::centredLeft); g.drawText("SCALE TYPE", pods[4].getX() + 87, pods[4].getY() + 32, pods[4].getWidth() - 98, 15, juce::Justification::centredLeft);
    g.setFont(10); g.drawText("SENSITIVITY", pods[0].getX() + 13, pods[0].getBottom() - 35, 101, 15, juce::Justification::centredLeft);
    g.drawText("THROAT", pods[1].getX() + 10, pods[1].getBottom() - 18, 58, 14, juce::Justification::centred); g.drawText("TENSION", pods[1].getRight() - 68, pods[1].getBottom() - 18, 58, 14, juce::Justification::centred);
    title(g, pods[2], "- PULL                         DRIVE +", pods[2].getHeight() - 28, 18, 10);
    g.drawText("LOW END", pods[3].getX() + 5, pods[3].getBottom() - 27, pods[3].getWidth() / 2 - 5, 18, juce::Justification::centred); g.drawText("TOP END", pods[3].getCentreX(), pods[3].getBottom() - 27, pods[3].getWidth() / 2 - 5, 18, juce::Justification::centred);
    g.setColour(aura::panel); g.fillRoundedRectangle(outputPod.toFloat(), 6); g.setColour(aura::line); g.drawRoundedRectangle(outputPod.toFloat().reduced(0.5f), 6, 1);
    g.setColour(aura::text); g.setFont(13); g.drawText("OUTPUT", outputPod.getX() + 13, outputPod.getY() + 9, 150, 18, juce::Justification::centredLeft);
    g.setColour(aura::muted); g.setFont(10); g.drawText("GAIN", outputPod.getX() + 17, outputPod.getBottom() - 22, 116, 16, juce::Justification::centred); g.drawText("MIX", outputPod.getRight() - 68, outputPod.getBottom() - 22, 53, 16, juce::Justification::centred);
    g.setColour(aura::muted); g.setFont(11); const auto rate = auraProcessor.getSampleRate();
    const auto activeMode = auraProcessor.isActiveRealTimeMode() ? "REAL-TIME" : "STUDIO";
    const auto status = (rate > 0 ? juce::String(rate / 1000, 1) + " kHz" : "READY") + "     /     FLOAT 32     /     " + activeMode
        + "     /     " + juce::String(auraProcessor.getAnalysisFftSize()) + " FFT     /     " + juce::String(auraProcessor.getLatencySamples()) + " samples";
    g.drawText(status, 37, getHeight() - 35, getWidth() - 212, 18, juce::Justification::centredLeft); g.setColour(aura::accent); g.fillEllipse(24, static_cast<float>(getHeight() - 29), 5, 5);
}
void AuraAudioProcessorEditor::resized()
{
    const auto width = getWidth() - 56; const auto waveHeight = getHeight() - 488;
    headerPreset = { getWidth() / 2 - 154, 28, 308, 40 }; factoryPresets.setBounds(headerPreset.getX() + 62, 35, headerPreset.getWidth() - 73, 25);
    oversampling.setBounds(getWidth() - 242, 38, 98, 27);
    power.setBounds(getWidth() - 119, 30, 38, 34); menu.setBounds(getWidth() - 70, 30, 35, 34);
    visualizer.setBounds(28, 90, width, waveHeight);
    settings.setBounds(getWidth() - 410, 88, 382, 424);
    const auto podY = 100 + waveHeight, podHeight = 186;
    constexpr std::array<float, 5> weights { 1, 1, 1.18f, 1, 1.5f }; const auto unit = static_cast<float>(width - 32) / 5.68f;
    int x = 28;
    for (std::size_t i = 0; i < pods.size(); ++i) { const auto w = i == 4 ? getWidth() - 28 - x : juce::roundToInt(unit * weights[i]); pods[i] = { x, podY, w, podHeight }; x += w + 8; }
    auto centredKnob = [this](std::size_t i, juce::Rectangle<int> pod, int w, int h, int top) { sliders[i].setBounds(pod.getCentreX() - w / 2, pod.getY() + top, w, h); };
    centredKnob(0, pods[0], 100, 107, 48); centredKnob(1, pods[1], 100, 107, 48); centredKnob(2, pods[2], 123, 127, 44);
    sliders[5].setBounds(pods[0].getX() + 12, pods[0].getBottom() - 22, pods[0].getWidth() - 88, 15); transientBypass.setBounds(pods[0].getRight() - 68, pods[0].getBottom() - 31, 57, 23);
    sliders[6].setBounds(pods[1].getX() + 10, pods[1].getBottom() - 75, 57, 57); sliders[7].setBounds(pods[1].getRight() - 67, pods[1].getBottom() - 75, 57, 57);
    const auto half = pods[3].getWidth() / 2; sliders[3].setBounds(pods[3].getX() + 3, pods[3].getY() + 55, half - 4, 99); sliders[4].setBounds(pods[3].getCentreX(), pods[3].getY() + 55, half - 4, 99);
    tonic.setBounds(pods[4].getX() + 11, pods[4].getY() + 49, 68, 26);
    scales.setBounds(pods[4].getX() + 87, pods[4].getY() + 49, pods[4].getWidth() - 98, 26);
    scaleButton.setBounds(pods[4].getX() + 11, pods[4].getY() + 82, (pods[4].getWidth() - 25) / 2, 23); customButton.setBounds(scaleButton.getRight() + 3, scaleButton.getY(), scaleButton.getWidth(), 23);
    keyboard.setBounds(pods[4].getX() + 12, pods[4].getY() + 114, pods[4].getWidth() - 24, 58);
    const auto rackY = podY + podHeight + 9, rackH = getHeight() - rackY - 48; const auto spectrumW = juce::roundToInt(static_cast<float>(width) * 0.36f), outputW = juce::roundToInt(static_cast<float>(width) * 0.28f);
    rackSpectrum.setBounds(28, rackY, spectrumW, rackH); outputPod = { rackSpectrum.getRight() + 8, rackY, outputW, rackH }; rackMeter.setBounds(outputPod.getRight() + 8, rackY, getWidth() - 28 - outputPod.getRight() - 8, rackH);
    sliders[8].setBounds(outputPod.getX() + 17, outputPod.getY() + 33, 116, rackH - 53); sliders[9].setBounds(outputPod.getRight() - 71, outputPod.getY() + 63, 60, 60);
    mute.setBounds(outputPod.getRight() - 119, outputPod.getY() + 34, 47, 24); solo.setBounds(outputPod.getRight() - 65, outputPod.getY() + 34, 47, 24);
    bypass.setBounds(getWidth() - 147, getHeight() - 36, 119, 24);
}
