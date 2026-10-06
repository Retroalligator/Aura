// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SpectrumFifo.h"

namespace aura
{
#if JUCE_MAC
static_assert(JUCE_USE_VDSP_FRAMEWORK, "Aura requires the allocation-free macOS vDSP FFT backend");
#endif
// A rectangular, symmetric low-quefrency lifter is a projection in the log
// spectrum. Replacing that projection preserves the higher-quefrency detail.
template <int Order> class BasicCepstralEnvelope
{
    static constexpr int fftSize = 1 << Order, binCount = fftSize / 2 + 1;
public:
    void prepare(double rate, float tension = 0.5f) noexcept
    {
        cutoff = juce::jlimit(1, fftSize / 4, juce::roundToInt(rate * 0.001 * std::exp2((tension - 0.5f) * 2.0f)));
    }
    void extract(const std::array<float, binCount>& magnitude,
                 std::array<float, binCount>& logEnvelope) noexcept
    {
        // A frame-relative floor prevents newly empty bins after shifting
        // from dominating the cepstrum and producing enormous gain ratios.
        const auto peak = *std::max_element(magnitude.begin(), magnitude.end());
        const auto floor = std::max(1.0e-9f, peak * 0.001f);
#if JUCE_MAC
        for (int k = 0; k < binCount; ++k)
            spectrum[static_cast<std::size_t>(k)] = std::log(std::max(floor, magnitude[static_cast<std::size_t>(k)]));
        for (int k = 1; k < fftSize / 2; ++k)
            spectrum[static_cast<std::size_t>(fftSize - k)] = spectrum[static_cast<std::size_t>(k)];
        // The log spectrum is real and even, so its forward DFT is real and
        // equals N times its inverse DFT. JUCE's real forward transform is
        // unnormalised and returns interleaved complex positive-frequency bins.
        fft.performRealOnlyForwardTransform(spectrum.data(), true);
        std::fill(cepstrum.begin(), cepstrum.begin() + fftSize, 0.0f);
        constexpr auto inverseSize = 1.0f / fftSize;
        for (int q = 0; q <= cutoff; ++q)
        {
            const auto coefficient = spectrum[static_cast<std::size_t>(2 * q)] * inverseSize;
            cepstrum[static_cast<std::size_t>(q)] = coefficient;
            if (q != 0) cepstrum[static_cast<std::size_t>(fftSize - q)] = coefficient;
        }
        fft.performRealOnlyForwardTransform(cepstrum.data(), true);
        for (int k = 0; k < binCount; ++k)
            logEnvelope[static_cast<std::size_t>(k)] = cepstrum[static_cast<std::size_t>(2 * k)];
#else
        // JUCE's portable real FFT allocates large scratch at 4x. Complex
        // transforms accept both preallocated arrays and avoid that allocation.
        for (int k = 0; k < binCount; ++k)
            spectrum[static_cast<std::size_t>(k)] = { std::log(std::max(floor, magnitude[static_cast<std::size_t>(k)])), 0.0f };
        for (int k = 1; k < fftSize / 2; ++k)
            spectrum[static_cast<std::size_t>(fftSize - k)] = spectrum[static_cast<std::size_t>(k)];
        fft.perform(spectrum.data(), cepstrum.data(), true);
        for (int q = 0; q < fftSize; ++q)
            cepstrum[static_cast<std::size_t>(q)] = q <= cutoff || q >= fftSize - cutoff
                ? juce::dsp::Complex<float> { cepstrum[static_cast<std::size_t>(q)].real(), 0.0f }
                : juce::dsp::Complex<float> {};
        fft.perform(cepstrum.data(), spectrum.data(), false);
        for (int k = 0; k < binCount; ++k)
            logEnvelope[static_cast<std::size_t>(k)] = spectrum[static_cast<std::size_t>(k)].real();
#endif

    }
private:
    juce::dsp::FFT fft { Order };
    int cutoff = 48;
#if JUCE_MAC
    // vDSP real transforms use caller-owned 2*N workspaces.
    std::array<float, 2 * fftSize> spectrum {}, cepstrum {};
#else
    std::array<juce::dsp::Complex<float>, fftSize> spectrum {}, cepstrum {};
#endif
};

using CepstralEnvelope = BasicCepstralEnvelope<fftOrder>;

template <std::size_t N> class SlidingMedian
{
public:
    void reset(const std::array<float, N>& initial) noexcept { sorted = initial; std::sort(sorted.begin(), sorted.end()); }
    float value() const noexcept { return sorted[N / 2]; }
    void advance(float outgoing, float incoming) noexcept
    {
        if (!(outgoing < incoming || outgoing > incoming)) return;
        auto removed = std::lower_bound(sorted.begin(), sorted.end(), outgoing);
        if (removed == sorted.end()) return;
        std::move(removed + 1, sorted.end(), removed);
        auto inserted = std::lower_bound(sorted.begin(), sorted.end() - 1, incoming);
        std::move_backward(inserted, sorted.end() - 1, sorted.end());
        *inserted = incoming;
    }
private:
    std::array<float, N> sorted {};
};

template <std::size_t N> float median(std::array<float, N> values) noexcept
{
    // Small fixed insertion sort: bounded work and no allocator.
    for (std::size_t i = 1; i < N; ++i)
    {
        const auto value = values[i];
        auto j = i;
        while (j > 0 && values[j - 1] > value) { values[j] = values[j - 1]; --j; }
        values[j] = value;
    }
    return values[N / 2];
}
inline float percussiveMask(float harmonic, float percussive) noexcept
{
    const auto scale = std::max(harmonic, percussive);
    if (scale <= 1.0e-12f) return 0.0f;
    const auto h = harmonic / scale, p = percussive / scale;
    return p * p / (h * h + p * p);
}
}
