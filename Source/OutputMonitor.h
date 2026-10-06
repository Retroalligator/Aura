// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "SpectrumFifo.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <numbers>

namespace aura
{
// BS.1770 K weighting, channel-summed 400 ms momentary loudness, updated
// at 10 ms bucket boundaries. No integrated gating or true-peak claim.
class LoudnessMeter
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process(double x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
            return y;
        }
    };
public:
    void prepare(double rate) noexcept
    {
        bucketSamples = juce::jmax(1, juce::roundToInt(rate * 0.01));
        const auto k = std::tan(std::numbers::pi * 1681.974450955533 / rate);
        const auto q = 0.7071752369554196, vh = std::pow(10.0, 3.999843853973347 / 20.0), vb = std::pow(vh, 0.4996667741545416);
        const auto d = 1 + k / q + k * k;
        for (auto& filter : shelf) filter = { (vh + vb * k / q + k * k) / d, 2 * (k * k - vh) / d,
            (vh - vb * k / q + k * k) / d, 2 * (k * k - 1) / d, (1 - k / q + k * k) / d };
        const auto kh = std::tan(std::numbers::pi * 38.13547087602444 / rate), qh = 0.5003270373238773;
        const auto dh = 1 + kh / qh + kh * kh;
        for (auto& filter : highpass) filter = { 1, -2, 1, 2 * (kh * kh - 1) / dh, (1 - kh / qh + kh * kh) / dh };
        reset();
    }
    void reset() noexcept
    {
        for (auto& filter : shelf) filter.z1 = filter.z2 = 0;
        for (auto& filter : highpass) filter.z1 = filter.z2 = 0;
        energy.fill(0); rawEnergy.fill(0); peaks.fill(0); sum = rawSum = bucket = rawBucket = peak = 0;
        position = samples = 0; lufs.store(-100); rms.store(-100); peakDb.store(-100);
    }
    void process(float left, float right, int channels) noexcept
    {
        double weighted = 0, raw = 0;
        for (int c = 0; c < channels; ++c)
        {
            const auto x = static_cast<double>(c == 0 ? left : right);
            const auto y = highpass[static_cast<std::size_t>(c)].process(shelf[static_cast<std::size_t>(c)].process(x));
            weighted += y * y; raw += x * x; peak = std::max(peak, std::abs(x));
        }
        bucket += weighted; rawBucket += raw / std::max(1, channels);
        if (++samples < bucketSamples) return;
        const auto idx = static_cast<std::size_t>(position);
        sum += bucket - energy[idx]; rawSum += rawBucket - rawEnergy[idx];
        energy[idx] = bucket; rawEnergy[idx] = rawBucket;
        peaks[idx] = peak;
        const auto length = static_cast<double>(bucketSamples * 40);
        lufs.store(static_cast<float>(-0.691 + 10 * std::log10(std::max(1.0e-10, sum / length))), std::memory_order_relaxed);
        rms.store(static_cast<float>(10 * std::log10(std::max(1.0e-10, rawSum / length))), std::memory_order_relaxed);
        peakDb.store(static_cast<float>(20 * std::log10(std::max(1.0e-5, *std::max_element(peaks.begin(), peaks.end())))), std::memory_order_relaxed);
        position = (position + 1) % 40; samples = 0; bucket = rawBucket = peak = 0;
    }
    std::atomic<float> lufs { -100 }, rms { -100 }, peakDb { -100 };
private:
    std::array<Biquad, 2> shelf {}, highpass {};
    std::array<double, 40> energy {}, rawEnergy {}, peaks {};
    int bucketSamples = 480, samples = 0, position = 0;
    double sum = 0, rawSum = 0, bucket = 0, rawBucket = 0, peak = 0;
};

class OutputAnalyser
{
public:
    OutputAnalyser()
    {
        for (int i = 0; i < fftSize; ++i) window[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) / fftSize);
    }
    void prepare(double rate) noexcept { sampleRate = static_cast<float>(rate); reset(); }
    void reset() noexcept { for (auto& channel : ring) channel.fill(0); position = hop = 0; }
    void process(float left, float right, int channels, SpectrumFifo& fifo) noexcept
    {
        const auto count = juce::jlimit(1, 2, channels);
        for (int c = 0; c < count; ++c)
        {
            const auto sample = c == 0 ? left : right;
            ring[static_cast<std::size_t>(c)][static_cast<std::size_t>(position)] = std::isfinite(sample) ? sample : 0;
        }
        position = (position + 1) % fftSize;
        if (++hop != hopSize) return;
        hop = 0;
        frame.output.fill(0); frame.sampleRate = sampleRate;
        for (int c = 0; c < count; ++c)
        {
            for (int i = 0; i < fftSize; ++i)
                time[static_cast<std::size_t>(i)] = { ring[static_cast<std::size_t>(c)][static_cast<std::size_t>((position + i) % fftSize)]
                    * window[static_cast<std::size_t>(i)], 0 };
            fft.perform(time.data(), bins.data(), false);
            for (int k = 0; k < binCount; ++k) frame.output[static_cast<std::size_t>(k)] += std::norm(bins[static_cast<std::size_t>(k)]);
        }
        // Combine spectral power, avoiding cancellation of opposite-polarity stereo.
        for (int k = 0; k < binCount; ++k) frame.output[static_cast<std::size_t>(k)] = std::sqrt(frame.output[static_cast<std::size_t>(k)] / static_cast<float>(count)) * displayGain(k);
        fifo.push(frame);
    }
private:
    juce::dsp::FFT fft { fftOrder };
    std::array<std::array<float, fftSize>, 2> ring {};
    std::array<float, fftSize> window {};
    std::array<juce::dsp::Complex<float>, fftSize> time {}, bins {};
    SpectrumFrame frame;
    float sampleRate = 48000;
    int position = 0, hop = 0;
};
}
