// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <memory>

namespace aura
{
inline constexpr int fftOrder = 13;
inline constexpr int fftSize = 1 << fftOrder;
inline constexpr int hopSize = fftSize / 4;
inline constexpr int binCount = fftSize / 2 + 1;
inline constexpr int hpssTimeFrames = 9;
inline constexpr int hpssLookaheadFrames = hpssTimeFrames / 2;
inline constexpr int hpssFrequencyBins = 17;
inline constexpr int latencySamples = fftSize + hpssLookaheadFrames * hopSize;
// Single-sided amplitude with the periodic Hann's 0.5 coherent gain removed.
inline constexpr float displayGain(int bin) noexcept { return (bin == 0 || bin == fftSize / 2 ? 2.0f : 4.0f) / fftSize; }
struct SpectrumFrame
{
    std::array<float, binCount> input {};
    std::array<float, binCount> output {};
    std::array<float, binCount> targetHz {};
    std::array<float, binCount> envelope {};
    float transientHit = 0.0f, percussiveLevel = 0.0f, formantCorrection = 0.0f;
    float sampleRate = 48000.0f;
    int analysisFftSize = fftSize, validBins = binCount;
};

// One audio producer and one editor consumer. Drop new frames on overflow.
// Never reset the FIFO while either thread may access it.
class SpectrumFifo
{
public:
    void push(const SpectrumFrame& frame) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 != 0) { (*frames)[static_cast<std::size_t>(start1)] = frame; fifo.finishedWrite(1); }
    }
    bool readLatest(SpectrumFrame& frame) noexcept
    {
        bool read = false;
        float hit = 0.0f, punch = 0.0f, throat = 0.0f;
        const auto available = fifo.getNumReady();
        for (int i = 0; i < available; ++i)
        {
            int start1, size1, start2, size2;
            fifo.prepareToRead(1, start1, size1, start2, size2);
            if (size1 != 0)
            {
                frame = (*frames)[static_cast<std::size_t>(start1)];
                hit = std::max(hit, frame.transientHit);
                punch = std::max(punch, frame.percussiveLevel);
                throat = std::max(throat, frame.formantCorrection);
                fifo.finishedRead(1);
                read = true;
            }
        }
        if (read) { frame.transientHit = hit; frame.percussiveLevel = punch; frame.formantCorrection = throat; }
        return read;
    }
private:
    juce::AbstractFifo fifo { 8 };
    std::unique_ptr<std::array<SpectrumFrame, 8>> frames = std::make_unique<std::array<SpectrumFrame, 8>>();
};
}
