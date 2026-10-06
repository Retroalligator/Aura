// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SpectrumFifo.h"
#include "Scale.h"
#include "CepstralEnvelope.h"

namespace aura
{
struct SpectralSettings
{
    float low = 80.0f, high = 12000.0f, amount = 0.5f;
    float transientPreserve = 1.0f, formantPreserve = 1.0f;
    float transientSensitivity = 0.5f, formantShift = 0.0f, formantTension = 0.5f;
    std::uint16_t mask = scaleMasks[0];
};

template <int Order> class SpectralEngine
{
    static constexpr int fftSize = 1 << Order, hopSize = fftSize / 4, binCount = fftSize / 2 + 1;
    static constexpr int latencySamples = fftSize + hpssLookaheadFrames * hopSize;
    static constexpr int analysisBins = std::min(binCount, aura::binCount);
    static constexpr float displayGain(int bin) noexcept { return (bin == 0 || bin == fftSize / 2 ? 2.0f : 4.0f) / fftSize; }
public:
    SpectralEngine();
    void prepare(double sampleRate, int frameOffset = 0) noexcept;
    void reset() noexcept;
    // Returns wet signal and latency-aligned dry signal. All storage is fixed.
    float processSample(float input, float& delayedDry, const SpectralSettings&,
                        SpectrumFifo* visualizer = nullptr) noexcept;
private:
    void processFrame(const SpectralSettings&, SpectrumFifo*) noexcept;
    void synthesise(const std::array<juce::dsp::Complex<float>, fftSize>&,
                    std::array<float, fftSize>&) noexcept;
    struct AnalysisFrame
    {
        std::array<juce::dsp::Complex<float>, analysisBins> bins {};
        std::array<float, analysisBins> magnitude {};
        float transient = 0, energy = 0;
    };
    juce::dsp::FFT fft { Order };
    BasicCepstralEnvelope<Order> cepstral;
    double sampleRate = 48000.0;
    int inputPosition = 0, outputPosition = 0, dryPosition = 0, hopCounter = 0, initialHopCounter = 0;
    int historyPosition = 0, framesSeen = 0;
    bool phaseReady = false;
    float smoothedAmount = 0.0f;
    float smoothedTransient = 1.0f, smoothedFormant = 1.0f, previousEnergy = 0.0f;
    float smoothedShift = 0.0f, smoothedTension = 0.5f, smoothedSensitivity = 0.5f;
    bool preservationReady = false;
    std::array<AnalysisFrame, hpssTimeFrames> history {};
    std::array<float, fftSize> inputRing {}, outputRing {}, window {};
    std::array<float, latencySamples> dryRing {};
    std::array<juce::dsp::Complex<float>, fftSize> time {}, spectrum {}, shifted {}, bypass {}, inverse {};
    std::array<float, binCount> phase {}, previousPhase {}, synthesisPhase {}, magnitude {}, frequency {}, tunedFrequency {}, peakPull {};
    std::array<float, binCount> routedMagnitude {}, processedMagnitude {}, originalEnvelope {}, routedEnvelope {}, processedEnvelope {};
    std::array<int, binCount> peaks {}, owner {};
    SpectrumFrame visualFrame;
    float fastEnvelope = 0, slowEnvelope = 0, hopTransient = 0;
    float attack = 0, release = 0, slowRelease = 0;
    std::array<float, 4> transientWindow {};
    int transientPosition = 0;
};

// Own large FFT/history storage on the heap, constructed before callbacks.
template <int Order> class BasicSpectralProcessor
{
public:
    void prepare(double rate, int frameOffset = 0) noexcept { engine->prepare(rate, frameOffset); }
    void reset() noexcept { engine->reset(); }
    float processSample(float input, float& dry, const SpectralSettings& settings, SpectrumFifo* fifo = nullptr) noexcept
    { return engine->processSample(input, dry, settings, fifo); }
private:
    std::unique_ptr<SpectralEngine<Order>> engine = std::make_unique<SpectralEngine<Order>>();
};
using SpectralProcessor = BasicSpectralProcessor<fftOrder>;
}
