// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "SpectralProcessor.h"
#include <algorithm>
#include <numbers>

namespace aura
{
namespace
{
constexpr auto twoPi = 2.0f * std::numbers::pi_v<float>;
float wrap(float x) noexcept { return std::remainder(x, twoPi); }
}
template <int Order, int BaseOrder>
SpectralEngine<Order, BaseOrder>::SpectralEngine()
{
    for (int i = 0; i < fftSize; ++i)
        window[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(twoPi * static_cast<float>(i) / fftSize);
}
template <int Order, int BaseOrder>
void SpectralEngine<Order, BaseOrder>::prepare(double rate, int frameOffset) noexcept
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    initialHopCounter = juce::jlimit(0, hopSize - 1, frameOffset);
    cepstral.prepare(sampleRate);
    attack = static_cast<float>(1 - std::exp(-1 / (sampleRate * 0.0005)));
    release = static_cast<float>(1 - std::exp(-1 / (sampleRate * 0.005)));
    slowRelease = static_cast<float>(1 - std::exp(-1 / (sampleRate * 0.03)));
    reset();
}
template <int Order, int BaseOrder>
void SpectralEngine<Order, BaseOrder>::reset() noexcept
{
    inputRing.fill(0); outputRing.fill(0); dryRing.fill(0);
    for (auto& frame : history) { frame.bins.fill({}); frame.magnitude.fill(0); frame.transient = frame.energy = 0; }
    previousPhase.fill(0); synthesisPhase.fill(0);
    inputPosition = outputPosition = dryPosition = historyPosition = framesSeen = 0;
    hopCounter = initialHopCounter;
    phaseReady = preservationReady = false; smoothedAmount = previousEnergy = 0;
    fastEnvelope = slowEnvelope = hopTransient = 0; transientWindow.fill(0); transientPosition = 0;
}
template <int Order, int BaseOrder>
float SpectralEngine<Order, BaseOrder>::processSample(float input, float& delayedDry,
                                      const SpectralSettings& settings, SpectrumFifo* fifo) noexcept
{
    const auto in = static_cast<std::size_t>(inputPosition);
    const auto out = static_cast<std::size_t>(outputPosition);
    const auto wet = outputRing[out];
    outputRing[out] = 0;
    const auto dry = static_cast<std::size_t>(dryPosition);
    delayedDry = dryRing[dry];
    const auto clean = std::isfinite(input) ? input : 0.0f;
    const auto amplitude = std::abs(clean);
    fastEnvelope += (amplitude - fastEnvelope) * (amplitude > fastEnvelope ? attack : release);
    slowEnvelope += (amplitude - slowEnvelope) * slowRelease;
    const auto onset = fastEnvelope > 1.0e-6f ? juce::jlimit(0.0f, 1.0f, (fastEnvelope - slowEnvelope) / fastEnvelope) : 0.0f;
    hopTransient = std::max(hopTransient, onset);
    inputRing[in] = dryRing[dry] = clean;
    dryPosition = (dryPosition + 1) % latencySamples;
    inputPosition = (inputPosition + 1) % fftSize;
    outputPosition = (outputPosition + 1) % fftSize;
    if (++hopCounter == hopSize)
    {
        hopCounter = 0;
        processFrame(settings, fifo);
    }
    return std::isfinite(wet) ? wet : 0.0f;
}
template <int Order, int BaseOrder>
void SpectralEngine<Order, BaseOrder>::processFrame(const SpectralSettings& settings, SpectrumFifo* fifo) noexcept
{
    for (int i = 0; i < fftSize; ++i)
        time[static_cast<std::size_t>(i)] = { inputRing[static_cast<std::size_t>((inputPosition + i) % fftSize)]
                                             * window[static_cast<std::size_t>(i)], 0.0f };
    fft.perform(time.data(), spectrum.data(), false);
    auto& newest = history[static_cast<std::size_t>(historyPosition)]; newest.energy = 0;
    for (int k = 0; k < analysisBins; ++k)
    {
        const auto idx = static_cast<std::size_t>(k);
        newest.bins[idx] = spectrum[idx];
        newest.magnitude[idx] = std::sqrt(std::norm(spectrum[idx]));
        newest.energy += newest.magnitude[idx] * newest.magnitude[idx];
    }
    transientWindow[static_cast<std::size_t>(transientPosition)] = hopTransient;
    transientPosition = (transientPosition + 1) % 4; hopTransient = 0;
    newest.transient = *std::max_element(transientWindow.begin(), transientWindow.end());
    const auto current = historyPosition;
    historyPosition = (historyPosition + 1) % hpssTimeFrames;
    // Wait for the centred median's future frames. Zero-initialised past
    // frames reconstruct startup samples with the same four Hann overlaps.
    if (framesSeen++ < hpssLookaheadFrames) return;
    framesSeen = hpssLookaheadFrames + 1;
    const auto& centre = history[static_cast<std::size_t>((current + hpssTimeFrames - hpssLookaheadFrames) % hpssTimeFrames)];
    // Upsampling images above the host Nyquist carry no original source
    // content. Exclude them from HPSS/pitch analysis; x4 synthesis still
    // accepts generated bins above it before the downsampling low-pass.
    spectrum.fill({});
    for (int k = 0; k < analysisBins; ++k) spectrum[static_cast<std::size_t>(k)] = centre.bins[static_cast<std::size_t>(k)];
    const auto hzPerBin = static_cast<float>(sampleRate / fftSize);
    const auto hopSeconds = static_cast<float>(hopSize / sampleRate);
    const auto amount = juce::jlimit(0.0f, 1.0f, settings.amount);
    smoothedAmount += (amount - smoothedAmount) * (1.0f - std::exp(-hopSeconds / 0.025f));
    const auto transient = juce::jlimit(0.0f, 1.0f, settings.transientPreserve);
    const auto formant = juce::jlimit(0.0f, 1.0f, settings.formantPreserve);
    if (!preservationReady)
    {
        smoothedTransient = transient; smoothedFormant = formant;
        smoothedShift = settings.formantShift; smoothedTension = settings.formantTension;
        smoothedSensitivity = settings.transientSensitivity; preservationReady = true;
    }
    const auto smoothing = 1.0f - std::exp(-hopSeconds / 0.025f);
    smoothedTransient += (transient - smoothedTransient) * smoothing;
    smoothedFormant += (formant - smoothedFormant) * smoothing;
    if (std::abs(smoothedTransient - transient) < 1.0e-5f) smoothedTransient = transient;
    if (std::abs(smoothedFormant - formant) < 1.0e-5f) smoothedFormant = formant;
    smoothedShift += (settings.formantShift - smoothedShift) * smoothing;
    smoothedTension += (settings.formantTension - smoothedTension) * smoothing;
    smoothedSensitivity += (settings.transientSensitivity - smoothedSensitivity) * smoothing;
    cepstral.prepare(sampleRate, smoothedTension);
    if (amount == 0.0f && smoothedAmount < 1.0e-5f) smoothedAmount = 0;
    const auto low = std::min(settings.low, settings.high);
    const auto high = std::min(std::max(settings.low, settings.high), static_cast<float>(sampleRate * 0.5));
    if (!(centre.energy > 1.0e-30f) || !std::isfinite(centre.energy))
    {
        phaseReady = false; previousEnergy = 0;
        if (fifo != nullptr)
        {
            visualFrame.input.fill(0); visualFrame.output.fill(0); visualFrame.envelope.fill(0); visualFrame.targetHz.fill(0);
            visualFrame.transientHit = visualFrame.percussiveLevel = visualFrame.formantCorrection = 0;
            visualFrame.analysisFftSize = 1 << BaseOrder; visualFrame.validBins = analysisBins;
            visualFrame.sampleRate = static_cast<float>(sampleRate / (1 << (Order - BaseOrder))); fifo->push(visualFrame);
        }
        return;
    }
    float totalEnergy = 0.0f, bypassEnergy = 0.0f;
    SlidingMedian<hpssFrequencyBins> frequencyWindow;
    std::array<float, hpssFrequencyBins> initial {};
    for (int f = 0; f < hpssFrequencyBins; ++f)
        initial[static_cast<std::size_t>(f)] = centre.magnitude[static_cast<std::size_t>(juce::jlimit(0, analysisBins - 1, f - hpssFrequencyBins / 2))];
    frequencyWindow.reset(initial);
    const auto sensitivityWeight = std::exp2((smoothedSensitivity - 0.5f) * 4.0f);
    const auto envelopeOnset = juce::jlimit(0.0f, 1.0f, (centre.transient - 0.6f) / 0.3f);
    for (int k = 0; k < analysisBins; ++k)
    {
        const auto idx = static_cast<std::size_t>(k);
        magnitude[idx] = centre.magnitude[idx];
        std::array<float, hpssTimeFrames> temporal {};
        for (int f = 0; f < hpssTimeFrames; ++f) temporal[static_cast<std::size_t>(f)] = history[static_cast<std::size_t>(f)].magnitude[idx];
        const auto frequencyMedian = frequencyWindow.value();
        frequencyWindow.advance(centre.magnitude[static_cast<std::size_t>(juce::jlimit(0, analysisBins - 1, k - hpssFrequencyBins / 2))],
                                centre.magnitude[static_cast<std::size_t>(juce::jlimit(0, analysisBins - 1, k + hpssFrequencyBins / 2 + 1))]);
        const auto medianMask = percussiveMask(median(temporal), frequencyMedian * sensitivityWeight);
        // A fast/slow envelope onset boosts only broad bins. Keep narrow,
        // sustained harmonics under median HPSS rather than bypassing all bins.
        const auto broadness = juce::jlimit(0.0f, 1.0f, frequencyMedian / std::max(1.0e-9f, magnitude[idx]));
        const auto pMask = medianMask + (1 - medianMask) * envelopeOnset * broadness;
        const auto dryWeight = smoothedTransient * pMask;
        routedMagnitude[idx] = magnitude[idx] * (1.0f - dryWeight);
        bypass[idx] = spectrum[idx] * dryWeight;
        totalEnergy += magnitude[idx] * magnitude[idx];
        bypassEnergy += std::norm(bypass[idx]);
        phase[idx] = std::arg(spectrum[idx]);
        const auto expected = twoPi * static_cast<float>((k * hopSize) % fftSize) / fftSize;
        const auto residual = phaseReady ? wrap(phase[idx] - previousPhase[idx] - expected) : 0.0f;
        frequency[idx] = std::max(0.0f, static_cast<float>(k) * hzPerBin + residual / (twoPi * hopSeconds));
        previousPhase[idx] = phase[idx];
        if (!phaseReady) synthesisPhase[idx] = phase[idx];
    }
    if (fifo != nullptr) cepstral.extract(magnitude, originalEnvelope);
    // Identity phase locking: move each local peak and its surrounding lobe
    // together. It preserves the Hann lobe phase relationships after shifting.
    int peakCount = 0;
    for (int k = 1; k < analysisBins - 1; ++k)
        if (routedMagnitude[static_cast<std::size_t>(k)] > routedMagnitude[static_cast<std::size_t>(k - 1)]
            && routedMagnitude[static_cast<std::size_t>(k)] >= routedMagnitude[static_cast<std::size_t>(k + 1)])
            peaks[static_cast<std::size_t>(peakCount++)] = k;
    int p = 0;
    for (int k = 0; k < analysisBins; ++k)
    {
        while (p + 1 < peakCount && k > (peaks[static_cast<std::size_t>(p)] + peaks[static_cast<std::size_t>(p + 1)]) / 2) ++p;
        owner[static_cast<std::size_t>(k)] = peakCount == 0 ? k : peaks[static_cast<std::size_t>(p)];
    }
    shifted.fill({ 0, 0 });
    float movedEnergy = 0.0f;
    for (int k = 0; k < analysisBins; ++k)
    {
        const auto idx = static_cast<std::size_t>(k), end = static_cast<std::size_t>(analysisBins - 1);
        const auto hz = frequency[idx];
        const auto eligible = hz >= low && hz <= high && k > 0 && idx < end
            && settings.mask != 0 && smoothedAmount > 0 && routedMagnitude[idx] > 1.0e-9f;
        const auto edge = eligible && hz > 0 ? juce::jlimit(0.0f, 1.0f,
            std::min(12.0f * std::log2(hz / std::max(1.0f, low)),
                     12.0f * std::log2(std::max(1.0f, high) / hz))) : 0.0f;
        peakPull[idx] = eligible ? smoothedAmount * edge : 0.0f;
        const auto target = eligible ? nearestNoteHz(hz, settings.mask) : hz;
        tunedFrequency[idx] = eligible && hz > 0 ? hz * std::exp2(peakPull[idx] * std::log2(target / hz)) : hz;
        synthesisPhase[idx] = peakPull[idx] > 1.0e-5f && phaseReady
            ? wrap(synthesisPhase[idx] + twoPi * tunedFrequency[idx] * hopSeconds) : phase[idx];
    }
    for (int k = 0; k < analysisBins; ++k)
    {
        const auto idx = static_cast<std::size_t>(k);
        const auto peak = static_cast<std::size_t>(owner[idx]);
        const auto hz = frequency[peak];
        const auto pull = k > 0 && k < analysisBins - 1 ? peakPull[peak] : 0.0f;
        const auto tuned = tunedFrequency[peak];
        if (fifo != nullptr && idx < visualFrame.input.size())
        {
            visualFrame.input[idx] = magnitude[idx] * displayGain(k);
            visualFrame.envelope[idx] = std::exp(originalEnvelope[idx]) * displayGain(k);
            visualFrame.targetHz[idx] = tuned;
        }
        if (pull < 1.0e-5f || std::abs(tuned - hz) < 0.001f)
        {
            shifted[idx] += magnitude[idx] > 0.0f ? spectrum[idx] * (routedMagnitude[idx] / magnitude[idx]) : juce::dsp::Complex<float> {};
            synthesisPhase[idx] = phase[idx];
            continue;
        }
        const auto destination = static_cast<float>(k) + (tuned - hz) / hzPerBin;
        const auto left = static_cast<int>(std::floor(destination));
        const auto fraction = destination - static_cast<float>(left);
        const auto value = std::polar(routedMagnitude[idx], synthesisPhase[peak] + wrap(phase[idx] - phase[peak]));
        movedEnergy += routedMagnitude[idx] * routedMagnitude[idx];
        // A centred Hann lobe advances by pi radians between FFT bins.
        // Rotate fractional deposits to the destination grid before summing;
        // plain complex interpolation would cancel neighboring lobe bins.
        const auto rotation = std::polar(1.0f, std::numbers::pi_v<float> * fraction);
        if (left > 0 && left < binCount - 1) shifted[static_cast<std::size_t>(left)] += value * rotation * (1.0f - fraction);
        if (left + 1 > 0 && left + 1 < binCount - 1) shifted[static_cast<std::size_t>(left + 1)] -= value * rotation * fraction;
    }
    float correctionEnergy = 0.0f, correctedEnergy = 0.0f;
    if (smoothedFormant > 1.0e-5f && (movedEnergy > totalEnergy * 1.0e-8f || std::abs(smoothedShift) > 0.001f) && totalEnergy > 1.0e-12f)
    {
        // Preserve the input envelope of the branch being sweetened. Using
        // the full input here would count the dry percussive energy twice.
        // The full input envelope is still supplied to the visualizer.
        cepstral.extract(routedMagnitude, routedEnvelope);
        for (int k = 0; k < binCount; ++k) processedMagnitude[static_cast<std::size_t>(k)] = std::sqrt(std::norm(shifted[static_cast<std::size_t>(k)]));
        cepstral.extract(processedMagnitude, processedEnvelope);
        const auto shapeRatio = std::exp2(-smoothedShift / 12.0f);
        for (int k = 1; k < binCount - 1; ++k)
        {
            const auto idx = static_cast<std::size_t>(k);
            const auto hz = static_cast<float>(k) * hzPerBin;
            if (hz < low || hz > high || processedMagnitude[idx] <= 1.0e-9f) continue;
            const auto sourceBin = juce::jlimit(0.0f, static_cast<float>(binCount - 1), static_cast<float>(k) * shapeRatio);
            const auto firstBin = static_cast<std::size_t>(sourceBin);
            const auto nextBin = std::min(firstBin + 1, routedEnvelope.size() - 1);
            const auto targetEnvelope = routedEnvelope[firstBin] + (sourceBin - static_cast<float>(firstBin)) * (routedEnvelope[nextBin] - routedEnvelope[firstBin]);
            const auto correction = smoothedFormant * (targetEnvelope - processedEnvelope[idx]);
            // This clamp only protects floating point exponentiation. The
            // normal correction is the complete log-envelope difference.
            shifted[idx] *= std::exp(juce::jlimit(-30.0f, 30.0f, correction));
            correctionEnergy += processedMagnitude[idx] * processedMagnitude[idx] * correction * correction;
            correctedEnergy += processedMagnitude[idx] * processedMagnitude[idx];
        }
    }
    // The original-phase bypass never enters phase or envelope processing.
    // Both branches use the same window and delay: summing after correction
    // and sharing the linear IFFT/OLA equals two separate synthesis paths.
    for (int k = 0; k < binCount; ++k) shifted[static_cast<std::size_t>(k)] += bypass[static_cast<std::size_t>(k)];
    synthesise(shifted, outputRing);
    phaseReady = true;
    if (fifo != nullptr)
    {
        // Retain the host-rate frequency grid in the fixed display payload.
        // x4 uses four times the FFT/window/hop sizes for equal resolution.
        visualFrame.analysisFftSize = 1 << BaseOrder; visualFrame.validBins = analysisBins;
        visualFrame.sampleRate = static_cast<float>(sampleRate / (1 << (Order - BaseOrder)));
        visualFrame.percussiveLevel = totalEnergy > 1.0e-12f ? std::sqrt(bypassEnergy / totalEnergy) : 0.0f;
        const auto onset = totalEnergy > 1.0e-12f ? juce::jlimit(0.0f, 1.0f, (totalEnergy - previousEnergy) / totalEnergy) : 0.0f;
        visualFrame.transientHit = std::max(onset, centre.transient) * visualFrame.percussiveLevel;
        visualFrame.formantCorrection = correctedEnergy > 1.0e-12f
            ? juce::jlimit(0.0f, 1.0f, std::sqrt(correctionEnergy / correctedEnergy) / std::log(4.0f)) : 0.0f;
        for (int k = 0; k < analysisBins; ++k)
            visualFrame.output[static_cast<std::size_t>(k)] = std::sqrt(std::norm(shifted[static_cast<std::size_t>(k)])) * displayGain(k);
        fifo->push(visualFrame);
    }
    previousEnergy = totalEnergy;
}
template <int Order, int BaseOrder>
void SpectralEngine<Order, BaseOrder>::synthesise(const std::array<juce::dsp::Complex<float>, fftSize>& bins,
                                 std::array<float, fftSize>& ring) noexcept
{
    std::copy_n(bins.begin(), binCount, time.begin());
    time[0] = { time[0].real(), 0 };
    time[fftSize / 2] = { time[fftSize / 2].real(), 0 };
    for (int k = 1; k < fftSize / 2; ++k) time[static_cast<std::size_t>(fftSize - k)] = std::conj(time[static_cast<std::size_t>(k)]);
    fft.perform(time.data(), inverse.data(), true);
    for (int i = 0; i < fftSize; ++i)
        ring[static_cast<std::size_t>((outputPosition + i) % fftSize)] += inverse[static_cast<std::size_t>(i)].real()
            * window[static_cast<std::size_t>(i)] * (2.0f / 3.0f);
}
template class SpectralEngine<fftOrder - 1, fftOrder - 1>;
template class SpectralEngine<fftOrder, fftOrder - 1>;
template class SpectralEngine<fftOrder + 1, fftOrder - 1>;
template class SpectralEngine<fftOrder>;
template class SpectralEngine<fftOrder + 1>;
template class SpectralEngine<fftOrder + 2>;
}
