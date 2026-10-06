// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "SpectralVisualizer.h"
namespace
{
float bandValue(const std::array<float, aura::binCount>& data, float u, float rate)
{
    const auto centre = 20.0f * std::pow(1000.0f, u);
    if (centre > rate * 0.5f) return 0;
    const auto a = juce::jlimit(1, aura::binCount - 1, juce::roundToInt(centre * std::pow(1000.0f, -0.5f / 192) * aura::fftSize / rate));
    const auto b = juce::jlimit(a, aura::binCount - 1, juce::roundToInt(centre * std::pow(1000.0f, 0.5f / 192) * aura::fftSize / rate));
    float sum = 0;
    for (int i = a; i <= b; ++i) sum += data[static_cast<std::size_t>(i)] * data[static_cast<std::size_t>(i)];
    return std::sqrt(sum / static_cast<float>(b - a + 1));
}
juce::Path smoothPath(const std::array<float, 192>& values, juce::Rectangle<float> r)
{
    std::array<juce::Point<float>, 192> points;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto db = juce::Decibels::gainToDecibels(values[i], -60.0f);
        points[i] = { r.getX() + r.getWidth() * static_cast<float>(i) / 191,
            r.getBottom() - r.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 60) / 60) };
    }
    juce::Path path; path.startNewSubPath(points[0]);
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto p0 = points[i == 0 ? 0 : i - 1], p1 = points[i], p2 = points[i + 1], p3 = points[std::min(i + 2, points.size() - 1)];
        path.cubicTo(p1 + (p2 - p0) / 6, p2 - (p3 - p1) / 6, p2);
    }
    return path;
}
}
SpectralVisualizer::SpectralVisualizer(AuraAudioProcessor& p) : processor(p)
{
    setTitle("Particle spectrum and frequency range");
    setDescription("Drag the glowing low and high range handles. Hover to inspect frequency. Double click to reset the range.");
    startTimerHz(60);
}
SpectralVisualizer::~SpectralVisualizer() { stopTimer(); finishGesture(); }
juce::Rectangle<float> SpectralVisualizer::plot() const { return getLocalBounds().toFloat().reduced(15, 15).withTrimmedLeft(29).withTrimmedTop(17).withTrimmedBottom(26); }
float SpectralVisualizer::frequencyX(float hz) const
{
    const auto r = plot(); return r.getX() + r.getWidth() * juce::jlimit(0.0f, 1.0f, std::log(hz / 20.0f) / std::log(1000.0f));
}
float SpectralVisualizer::xFrequency(float x) const
{
    const auto r = plot(); return 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}
float SpectralVisualizer::magnitudeY(float value) const
{
    const auto r = plot(); const auto db = juce::Decibels::gainToDecibels(value, -60.0f);
    return r.getBottom() - r.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 60) / 60);
}
void SpectralVisualizer::timerCallback()
{
    colourAmount += (processor.parameters.getRawParameterValue("amount")->load() - colourAmount) * 0.14f;
    const auto fresh = processor.spectrumFifo.readLatest(frame);
    const auto freshPost = processor.outputSpectrumFifo.readLatest(post);
    const auto now = juce::Time::getMillisecondCounter();
    if (fresh) lastInputFrame = now;
    if (freshPost) lastPostFrame = now;
    // The 8192-point analyser emits fewer frames than the 60 Hz painter.
    // Hold its latest target between frames; only release on real silence
    // or when callbacks have stopped for at least three analysis hops.
    const auto staleAfter = [](float rate) { return static_cast<std::uint32_t>(std::max(250.0f, 3000.0f * aura::hopSize / std::max(1.0f, rate))); };
    const auto stale = now - lastInputFrame > staleAfter(frame.sampleRate);
    if (now - lastPostFrame > staleAfter(post.sampleRate)) post.output.fill(0);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] += ((stale ? 0 : frame.input[i]) - input[i]) * (!stale && frame.input[i] > input[i] ? 0.48f : 0.08f);
        output[i] += ((stale ? 0 : frame.output[i]) - output[i]) * (!stale && frame.output[i] > output[i] ? 0.48f : 0.08f);
        envelope[i] += ((stale ? 0 : frame.envelope[i]) - envelope[i]) * (stale ? 0.08f : 0.22f);
    }
    for (std::size_t i = 0; i < waveInput.size(); ++i)
    {
        const auto u = static_cast<float>(i) / 191;
        waveInput[i] = bandValue(input, u, frame.sampleRate);
        waveOutput[i] = bandValue(output, u, frame.sampleRate);
        waveEnvelope[i] = bandValue(envelope, u, frame.sampleRate);
    }
    flash = reducedMotion ? 0 : std::max(flash * 0.78f, fresh ? juce::jmin(1.0f, frame.transientHit * 4) : 0.0f);
    punch = std::max(punch * 0.88f, fresh ? frame.percussiveLevel : 0.0f);
    throat = std::max(throat * 0.88f, fresh ? frame.formantCorrection : 0.0f);
    if (!reducedMotion)
    {
        for (auto& particle : particles) { particle.alpha *= 0.945f; particle.db += particle.velocity; }
        if (fresh)
            for (int n = 0; n < 256; ++n)
            {
                const auto idx = static_cast<std::size_t>(random() * 191);
                const auto db = juce::Decibels::gainToDecibels(waveOutput[idx], -60.0f);
                if (db < -58) continue;
                const auto strength = (db + 60) / 60;
                particles[nextParticle] = { static_cast<float>(idx) / 191 + (random() - 0.5f) * 0.006f,
                    db + (random() - 0.5f) * (4 + 10 * strength), 0.35f + 0.6f * strength, (random() - 0.5f) * 0.06f };
                nextParticle = (nextParticle + 1) % particles.size();
            }
    }
    if (onPreservation) onPreservation(punch, throat);
    if (onFrame) onFrame(); repaint();
}
void SpectralVisualizer::paint(juce::Graphics& g)
{
    const auto r = plot();
    g.setColour(aura::panel); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 7);
    g.setColour(aura::line.brighter(0.2f)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 7, 1);
    juce::ColourGradient atmosphere(juce::Colour(0xff14243e), r.getX(), r.getY(), juce::Colour(0xff26271b), r.getRight(), r.getY(), false);
    atmosphere.addColour(0.45, juce::Colour(0xff241635)); g.setGradientFill(atmosphere); g.fillRect(r);
    g.setFont(juce::Font(juce::FontOptions(11)));
    for (auto hz : { 20.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const auto x = frequencyX(hz); g.setColour(aura::line.withAlpha(0.8f)); g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom());
        g.setColour(aura::text.withAlpha(0.85f)); g.drawText(hz >= 1000 ? juce::String(hz / 1000, 0) + "k" : juce::String(hz, 0), juce::Rectangle<float>(x - 18, r.getY() - 20, 36, 17), juce::Justification::centred);
    }
    for (int db = 0; db >= -60; db -= 12)
    {
        const auto y = r.getBottom() - r.getHeight() * static_cast<float>(db + 60) / 60;
        g.setColour(aura::line.withAlpha(0.65f)); g.drawHorizontalLine(juce::roundToInt(y), r.getX(), r.getRight());
        g.setColour(aura::muted); g.drawText(juce::String(db), juce::Rectangle<float>(3, y - 8, 34, 16), juce::Justification::centredRight);
    }
    g.saveState(); g.reduceClipRegion(r.toNearestInt());
    if (flash > 0.01f)
    {
        juce::ColourGradient flashGradient(juce::Colours::transparentWhite, r.getX(), r.getY(), juce::Colours::transparentWhite, r.getRight(), r.getY(), false);
        flashGradient.addColour(0.43, juce::Colours::transparentWhite); flashGradient.addColour(0.5, juce::Colours::white.withAlpha(flash * 0.65f)); flashGradient.addColour(0.57, juce::Colours::transparentWhite);
        g.setGradientFill(flashGradient); g.fillRect(r);
    }
    auto inPath = smoothPath(waveInput, r), outPath = smoothPath(waveOutput, r), envPath = smoothPath(waveEnvelope, r);
    auto fill = outPath; fill.lineTo(r.getRight(), r.getBottom()); fill.lineTo(r.getX(), r.getBottom()); fill.closeSubPath();
    juce::ColourGradient colours(aura::spectralColour(0, colourAmount), r.getX(), r.getY(), aura::spectralColour(1, colourAmount), r.getRight(), r.getY(), false);
    for (double t = 0.2; t < 1; t += 0.2) colours.addColour(t, aura::spectralColour(static_cast<float>(t), colourAmount));
    auto haze = colours; haze.multiplyOpacity(0.11f); g.setGradientFill(haze); g.fillPath(fill);
    if (!reducedMotion)
        for (const auto& particle : particles)
        {
            if (particle.alpha < 0.015f || particle.db < -60 || particle.db > 0) continue;
            const auto x = r.getX() + particle.x * r.getWidth(), y = r.getBottom() - (particle.db + 60) / 60 * r.getHeight();
            const auto colour = aura::spectralColour(particle.x, colourAmount); g.setColour(colour.withAlpha(particle.alpha * 0.12f)); g.fillEllipse(x - 2, y - 2, 4, 4);
            g.setColour(colour.brighter(0.15f).withAlpha(particle.alpha)); g.fillEllipse(x - 0.65f, y - 0.65f, 1.3f, 1.3f);
        }
    auto rawColour = colours; rawColour.multiplyOpacity(0.45f); g.setGradientFill(rawColour); g.strokePath(inPath, juce::PathStrokeType(1));
    for (const auto& style : { std::pair { 10.0f, 0.09f }, std::pair { 5.0f, 0.22f }, std::pair { 1.8f, 1.0f } })
    {
        auto glow = colours; glow.multiplyOpacity(style.second);
        g.setGradientFill(glow); g.strokePath(outPath, juce::PathStrokeType(style.first));
    }
    g.setColour(aura::violet.withSaturation(0.22f + 0.76f * colourAmount).withAlpha(0.14f)); g.strokePath(envPath, juce::PathStrokeType(6));
    g.setColour(aura::violet.withSaturation(0.22f + 0.76f * colourAmount).withAlpha(0.85f)); g.strokePath(envPath, juce::PathStrokeType(1.3f));
    g.restoreState();
    const auto low = processor.parameters.getRawParameterValue("freqLow")->load(), high = processor.parameters.getRawParameterValue("freqHigh")->load();
    for (const auto& boundary : { std::pair { low, aura::accent }, std::pair { high, aura::gold } })
    {
        const auto x = frequencyX(boundary.first); g.setColour(boundary.second.withAlpha(0.09f)); g.fillRect(x - 12.0f, r.getY(), 24.0f, r.getHeight());
        g.setColour(boundary.second.withAlpha(0.2f)); g.drawLine(x, r.getY(), x, r.getBottom(), 5);
        g.setColour(boundary.second); g.drawLine(x, r.getY(), x, r.getBottom(), 1.2f);
        for (auto yy : { r.getY() - 4, r.getBottom() - 4 })
        { g.setColour(boundary.second.darker(0.4f)); g.fillRoundedRectangle(x - 5, yy, 10, 11, 2); g.setColour(boundary.second.brighter(0.3f)); g.drawRoundedRectangle(x - 5, yy, 10, 11, 2, 1); }
    }
    auto hzText = [](float hz) { return hz >= 1000 ? juce::String(hz / 1000, 1) + " kHz" : juce::String(hz, 0) + " Hz"; };
    g.setFont(juce::Font(juce::FontOptions(11))); g.setColour(aura::accent); g.drawText("LO-CUT: " + hzText(low), r.withY(r.getBottom() + 8).withHeight(17), juce::Justification::centredLeft);
    g.setColour(aura::gold); g.drawText("HI-CUT: " + hzText(high), r.withY(r.getBottom() + 8).withHeight(17), juce::Justification::centredRight);
    g.setColour(aura::violet); g.drawText("FORMANT ENVELOPE", r.withY(r.getBottom() + 8).withHeight(17), juce::Justification::centred);
    if (hoverX >= r.getX() && hoverX <= r.getRight())
    {
        g.setColour(aura::text.withAlpha(0.3f)); g.drawVerticalLine(juce::roundToInt(hoverX), r.getY(), r.getBottom());
        const auto hz = xFrequency(hoverX); const auto idx = static_cast<std::size_t>(juce::jlimit(0, aura::binCount - 1, juce::roundToInt(hz * aura::fftSize / frame.sampleRate)));
        const auto label = hzText(hz) + "  /  " + juce::String(juce::Decibels::gainToDecibels(output[idx], -60.0f), 1) + " dB";
        const auto box = juce::Rectangle<float>(juce::jlimit(r.getX(), r.getRight() - 160, hoverX - 80), r.getY() + 8, 160, 21);
        g.setColour(aura::panel.withAlpha(0.9f)); g.fillRoundedRectangle(box, 3); g.setColour(aura::text); g.drawText(label, box, juce::Justification::centred);
    }
}
void SpectralVisualizer::mouseMove(const juce::MouseEvent& e) { hoverX = e.position.x; repaint(); }
void SpectralVisualizer::mouseExit(const juce::MouseEvent&) { hoverX = -1; repaint(); }
void SpectralVisualizer::mouseDown(const juce::MouseEvent& e)
{
    finishGesture();
    const auto low = frequencyX(processor.parameters.getRawParameterValue("freqLow")->load());
    const auto high = frequencyX(processor.parameters.getRawParameterValue("freqHigh")->load());
    dragging = processor.parameters.getParameter(std::abs(e.position.x - low) < std::abs(e.position.x - high) ? "freqLow" : "freqHigh");
    dragging->beginChangeGesture(); mouseDrag(e);
}
void SpectralVisualizer::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging != nullptr) dragging->setValueNotifyingHost(dragging->convertTo0to1(xFrequency(e.position.x)));
}
void SpectralVisualizer::finishGesture() { if (dragging != nullptr) { dragging->endChangeGesture(); dragging = nullptr; } }
void SpectralVisualizer::mouseUp(const juce::MouseEvent&) { finishGesture(); }
void SpectralVisualizer::mouseDoubleClick(const juce::MouseEvent&)
{
    finishGesture();
    for (auto* id : { "freqLow", "freqHigh" })
    {
        auto* p = processor.parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture();
    }
}
