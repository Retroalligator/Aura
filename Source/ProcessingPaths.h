// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "SpectralProcessor.h"

namespace aura
{
template <std::size_t Capacity> class FixedDelay
{
public:
    void prepare(int delay) noexcept { length = juce::jlimit(0, static_cast<int>(Capacity), delay); reset(); }
    void reset() noexcept { samples.fill(0); position = 0; }
    float process(float input) noexcept
    {
        if (length == 0) return input;
        const auto output = samples[static_cast<std::size_t>(position)];
        samples[static_cast<std::size_t>(position)] = input;
        if (++position == length) position = 0;
        return output;
    }
private:
    std::array<float, Capacity> samples {};
    int length = 0, position = 0;
};
inline constexpr int processingChunkSize = 1024;
class ProcessingPath
{
public:
    virtual ~ProcessingPath() = default;
    virtual int filterLatency() const noexcept = 0;
    virtual void prepare(double rate, int alignment, int frameOffset) noexcept = 0;
    virtual void restart(std::uint64_t hostPosition) noexcept = 0;
    virtual void process(const juce::dsp::AudioBlock<const float>& input, int channels,
                         const SpectralSettings&, SpectrumFifo*) noexcept = 0;
    float sample(int channel, int index) const noexcept { return output[static_cast<std::size_t>(channel)][static_cast<std::size_t>(index)]; }
protected:
    std::array<std::array<float, processingChunkSize>, 2> output {};
};
template <int FactorOrder> class ResampledPath final : public ProcessingPath
{
    static constexpr int factor = 1 << FactorOrder;
public:
    explicit ResampledPath(bool highQuality)
        : resampler(2, FactorOrder, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, highQuality, true)
    { resampler.initProcessing(processingChunkSize); }
    int filterLatency() const noexcept override { return juce::roundToInt(resampler.getLatencyInSamples()); }
    void prepare(double rate, int alignment, int frameOffset) noexcept override
    {
        sampleRate = rate; globalFrameOffset = frameOffset;
        for (auto& delay : delays) delay.prepare(alignment);
        restart(0);
    }
    void restart(std::uint64_t hostPosition) noexcept override
    {
        const auto offset = static_cast<int>((hostPosition + static_cast<std::uint64_t>(globalFrameOffset)) % hopSize);
        for (auto& channel : dsp) channel.prepare(sampleRate * factor, offset * factor);
        for (auto& delay : delays) delay.reset();
        resampler.reset();
    }
    void process(const juce::dsp::AudioBlock<const float>& input, int channels,
                 const SpectralSettings& settings, SpectrumFifo* fifo) noexcept override
    {
        std::array<float*, 2> pointers { output[0].data(), output[1].data() };
        juce::dsp::AudioBlock<float> down(pointers.data(), 2, input.getNumSamples());
        auto up = resampler.processSamplesUp(input);
        for (std::size_t i = 0; i < up.getNumSamples(); ++i)
            for (int c = 0; c < channels; ++c)
            {
                float dry = 0;
                auto* samples = up.getChannelPointer(static_cast<std::size_t>(c));
                samples[i] = dsp[static_cast<std::size_t>(c)].processSample(samples[i], dry, settings, c == 0 ? fifo : nullptr);
            }
        resampler.processSamplesDown(down);
        for (int c = 0; c < channels; ++c)
            for (std::size_t i = 0; i < input.getNumSamples(); ++i)
                output[static_cast<std::size_t>(c)][i] = delays[static_cast<std::size_t>(c)].process(output[static_cast<std::size_t>(c)][i]);
    }
private:
    std::array<BasicSpectralProcessor<fftOrder + FactorOrder>, 2> dsp;
    juce::dsp::Oversampling<float> resampler;
    std::array<FixedDelay<256>, 2> delays;
    double sampleRate = 48000;
    int globalFrameOffset = 0;
};
class NativePath final : public ProcessingPath
{
public:
    int filterLatency() const noexcept override { return 0; }
    void prepare(double rate, int alignment, int frameOffset) noexcept override
    {
        sampleRate = rate; globalFrameOffset = frameOffset;
        for (auto& delay : delays) delay.prepare(alignment);
        restart(0);
    }
    void restart(std::uint64_t hostPosition) noexcept override
    {
        const auto offset = static_cast<int>((hostPosition + static_cast<std::uint64_t>(globalFrameOffset)) % hopSize);
        for (auto& channel : dsp) channel.prepare(sampleRate, offset);
        for (auto& delay : delays) delay.reset();
    }
    void process(const juce::dsp::AudioBlock<const float>& input, int channels,
                 const SpectralSettings& settings, SpectrumFifo* fifo) noexcept override
    {
        for (std::size_t i = 0; i < input.getNumSamples(); ++i)
            for (int c = 0; c < channels; ++c)
            {
                float dry = 0; const auto channel = static_cast<std::size_t>(c);
                output[channel][i] = delays[channel].process(dsp[channel].processSample(input.getChannelPointer(channel)[i], dry, settings, c == 0 ? fifo : nullptr));
            }
    }
private:
    std::array<SpectralProcessor, 2> dsp;
    std::array<FixedDelay<256>, 2> delays;
    double sampleRate = 48000;
    int globalFrameOffset = 0;
};
}
