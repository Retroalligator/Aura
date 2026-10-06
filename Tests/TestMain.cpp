// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "PluginProcessor.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <chrono>
#include <bit>

namespace
{
std::atomic<bool> watchAllocations { false };
std::atomic<int> allocations { 0 };
int failures = 0;
bool exact(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
void check(bool ok, const char* name)
{
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    if (!ok) ++failures;
}
void set(AuraAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
struct ProcessingProfile { int mode, quality, path; const char* name; };
constexpr std::array<ProcessingProfile, 5> processingProfiles {{
    { 0, 1, 0, "1x / High" }, { 1, 0, 1, "2x / Standard" }, { 1, 1, 2, "2x / High" },
    { 2, 0, 3, "4x / Standard" }, { 2, 1, 4, "4x / High" }
}};
constexpr int expectedHostLatency = 16445;
void selectProfile(AuraAudioProcessor& p, const ProcessingProfile& profile)
{
    p.setOversamplingMode(profile.mode); set(p, "processingQuality", static_cast<float>(profile.quality));
}
void fillOppositeTone(juce::AudioBuffer<float>& block, int start)
{
    for (int i = 0; i < block.getNumSamples(); ++i)
    {
        const auto sample = static_cast<float>(0.2 * std::sin(juce::MathConstants<double>::twoPi * 1000 * (start + i) / 48000));
        block.setSample(0, i, sample); block.setSample(1, i, -sample);
    }
}
std::vector<float> render(const std::vector<float>& in, double rate, aura::SpectralSettings settings)
{
    aura::SpectralProcessor dsp;
    dsp.prepare(rate);
    std::vector<float> out(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) { float dry; out[i] = dsp.processSample(in[i], dry, settings); }
    return out;
}
double rms(const std::vector<float>& v, std::size_t start)
{
    double sum = 0;
    for (auto i = start; i < v.size(); ++i) sum += v[i] * v[i];
    return std::sqrt(sum / static_cast<double>(v.size() - start));
}
aura::SpectrumFrame capture(const std::vector<float>& in, aura::SpectralSettings settings,
                            float& maxHit, float& maxPunch, float& maxThroat)
{
    aura::SpectralProcessor dsp; dsp.prepare(48000);
    aura::SpectrumFifo fifo; aura::SpectrumFrame frame, newest;
    for (auto sample : in)
    {
        float dry; dsp.processSample(sample, dry, settings, &fifo);
        if (fifo.readLatest(newest))
        {
            frame = newest;
            maxHit = std::max(maxHit, frame.transientHit);
            maxPunch = std::max(maxPunch, frame.percussiveLevel);
            maxThroat = std::max(maxThroat, frame.formantCorrection);
        }
    }
    return frame;
}
double envelopeDistance(const aura::SpectrumFrame& frame)
{
    aura::CepstralEnvelope cepstral; cepstral.prepare(48000);
    std::array<float, aura::binCount> input, output, a, b;
    for (std::size_t k = 0; k < input.size(); ++k)
    { input[k] = frame.input[k] / aura::displayGain(static_cast<int>(k)); output[k] = frame.output[k] / aura::displayGain(static_cast<int>(k)); }
    cepstral.extract(input, a); cepstral.extract(output, b);
    double error = 0;
    constexpr int first = static_cast<int>(120.0 * aura::fftSize / 48000), last = static_cast<int>(5000.0 * aura::fftSize / 48000);
    for (int k = first; k < last; ++k) { const auto d = a[static_cast<std::size_t>(k)] - b[static_cast<std::size_t>(k)]; error += d * d; }
    return std::sqrt(error / (last - first));
}
template <int Order> bool cepstralMatchesComplexReference()
{
    constexpr int size = 1 << Order, bins = size / 2 + 1;
    auto magnitude = std::make_unique<std::array<float, bins>>();
    auto envelope = std::make_unique<std::array<float, bins>>();
    auto spectrum = std::make_unique<std::array<juce::dsp::Complex<float>, size>>();
    auto cepstrum = std::make_unique<std::array<juce::dsp::Complex<float>, size>>();
    aura::BasicCepstralEnvelope<Order> real; juce::dsp::FFT complex(Order);
    std::mt19937 random(351); std::uniform_real_distribution<float> distribution(0.001f, 1.0f);
    for (int k = 0; k < bins; ++k) (*magnitude)[static_cast<std::size_t>(k)] = k < aura::binCount && k % 7 != 0 ? distribution(random) : 0.0f;
    float maxError = 0;
    const auto rate = 48000.0 * size / aura::fftSize;
    for (auto tension : { 0.0f, 0.5f, 1.0f })
    {
        real.prepare(rate, tension); real.extract(*magnitude, *envelope);
        const auto floor = std::max(1.0e-9f, *std::max_element(magnitude->begin(), magnitude->end()) * 0.001f);
        for (int k = 0; k < bins; ++k) (*spectrum)[static_cast<std::size_t>(k)] = { std::log(std::max(floor, (*magnitude)[static_cast<std::size_t>(k)])), 0 };
        for (int k = 1; k < size / 2; ++k) (*spectrum)[static_cast<std::size_t>(size - k)] = (*spectrum)[static_cast<std::size_t>(k)];
        complex.perform(spectrum->data(), cepstrum->data(), true);
        const auto cutoff = juce::jlimit(1, size / 4, juce::roundToInt(rate * 0.001 * std::exp2((tension - 0.5f) * 2.0f)));
        for (int q = 0; q < size; ++q) (*cepstrum)[static_cast<std::size_t>(q)] = q <= cutoff || q >= size - cutoff ? juce::dsp::Complex<float> { (*cepstrum)[static_cast<std::size_t>(q)].real(), 0 } : juce::dsp::Complex<float> {};
        complex.perform(cepstrum->data(), spectrum->data(), false);
        for (int k = 0; k < bins; ++k) maxError = std::max(maxError, std::abs((*envelope)[static_cast<std::size_t>(k)] - (*spectrum)[static_cast<std::size_t>(k)].real()));
    }
    std::cout << size << " real/complex cepstral max error " << maxError << '\n';
    return maxError < 2.0e-5f;
}
double measuredHz(const std::vector<float>& audio, double rate)
{
    int crossings = 0; std::size_t first = 0, last = 0;
    for (std::size_t i = static_cast<std::size_t>(rate * 0.5) + 1; i < audio.size(); ++i)
        if (audio[i - 1] <= 0 && audio[i] > 0) { if (crossings == 0) first = i; last = i; ++crossings; }
    return crossings > 1 ? static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first) : 0;
}
std::vector<float> renderPlugin(const std::vector<float>& audio, int root, float mixValue, bool soloValue)
{
    AuraAudioProcessor p; set(p, "scaleTonic", static_cast<float>(root)); set(p, "amount", 1); set(p, "freqLow", 20); set(p, "freqHigh", 20000);
    set(p, "transientPreserve", 0); set(p, "formantPreserve", 0); set(p, "mix", mixValue); set(p, "soloWet", soloValue ? 1.0f : 0.0f);
    p.prepareToPlay(48000, 256); juce::AudioBuffer<float> block(2, 256); juce::MidiBuffer midi; std::vector<float> out(audio.size());
    for (std::size_t start = 0; start < audio.size(); start += 256)
    {
        for (int i = 0; i < 256; ++i) { const auto index = start + static_cast<std::size_t>(i); const auto value = index < audio.size() ? audio[index] : 0; block.setSample(0, i, value); block.setSample(1, i, value); }
        p.processBlock(block, midi);
        for (int i = 0; i < 256 && start + static_cast<std::size_t>(i) < out.size(); ++i) out[start + static_cast<std::size_t>(i)] = block.getSample(0, i);
    }
    return out;
}
}
void* operator new(std::size_t bytes)
{
    if (watchAllocations.load(std::memory_order_relaxed)) ++allocations;
    if (auto* p = std::malloc(bytes == 0 ? 1 : bytes)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    check(cepstralMatchesComplexReference<aura::fftOrder>() && cepstralMatchesComplexReference<aura::fftOrder + 1>()
          && cepstralMatchesComplexReference<aura::fftOrder + 2>(), "real-only cepstrum matches complex reference at native, 2x and 4x sizes");
    check(aura::noteMask(0, 0) == 0xAB5 && aura::noteMask(7, 0x123) == 0x123, "scale masks");
    check(std::abs(aura::nearestNoteHz(430, 1 << 9) - 440) < 0.01f, "A440 target mapping");
    check(exact(aura::nearestNoteHz(430, 0), 430), "empty mask maps to identity");
    check(aura::noteMask(0, 0, 2) == 0xAD6 && aura::noteMask(7, 0x295, 9) == aura::noteMask(5, 0, 9), "tonic rotates preset and custom intervals into absolute pitch classes");
    bool roundTrip = true;
    for (int root = 0; root < 12; ++root)
    {
        const auto mask = aura::noteMask(0, 0, root);
        roundTrip = roundTrip && std::popcount(mask) == 7 && (mask & (1 << root)) != 0 && aura::noteMask(0, 0, root + 12) == mask;
    }
    check(roundTrip && aura::noteMask(7, 0, 9) == 0, "all twelve tonics preserve interval count and empty custom scales");
    {
        std::vector<float> tone(72000);
        for (std::size_t i = 0; i < tone.size(); ++i) tone[i] = static_cast<float>(0.2 * std::sin(juce::MathConstants<double>::twoPi * 365 * static_cast<double>(i) / 48000));
        const auto c = measuredHz(renderPlugin(tone, 0, 1, false), 48000), d = measuredHz(renderPlugin(tone, 2, 1, false), 48000);
        std::cout << "Tonic C / D: 365 Hz -> " << c << " / " << d << " Hz\n";
        check(std::abs(c - 349.228) < 0.25 && std::abs(d - 369.994) < 0.25, "automatable tonic changes actual processor snapping targets");
        const auto dry = measuredHz(renderPlugin(tone, 2, 0, false), 48000), wetSolo = measuredHz(renderPlugin(tone, 2, 0, true), 48000);
        check(std::abs(dry - 365) < 0.25 && std::abs(wetSolo - 369.994) < 0.25, "Solo auditions processed audio while retaining the dry Mix setting");
    }
    {
        AuraAudioProcessor p;
        check(p.getNumPrograms() == 5 && p.getProgramName(1) == "Vocal Magic", "factory presets are exposed as host programs");
        bool presetsValid = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram(i);
            presetsValid = presetsValid && p.getCurrentProgram() == i && !p.isCurrentProgramModified();
            juce::MemoryBlock state; p.getStateInformation(state); AuraAudioProcessor recall;
            recall.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            presetsValid = presetsValid && recall.getCurrentProgram() == i && !recall.isCurrentProgramModified();
        }
        check(presetsValid, "all factory sounds apply and round-trip their complete state and identity");
        set(p, "amount", 0.61f); check(p.isCurrentProgramModified(), "edited factory state is marked modified");
        const auto before = p.parameters.getRawParameterValue("amount")->load(); p.setCurrentProgram(-1); p.setCurrentProgram(5);
        check(exact(before, p.parameters.getRawParameterValue("amount")->load()), "invalid factory program selection preserves settings");
    }
    for (const auto& profile : processingProfiles)
    {
        AuraAudioProcessor p; set(p, "amount", 0); selectProfile(p, profile); p.prepareToPlay(48000, 1537);
        juce::AudioBuffer<float> block(2, 1537); juce::MidiBuffer midi;
        const auto preparedCorrectly = p.getActiveProcessingPath() == profile.path && !p.isProcessingChangePending();
        double error = 0; float isolation = 0; bool finite = true;
        for (int n = 0; n < 40; ++n)
        {
            const auto start = n * block.getNumSamples(); fillOppositeTone(block, start); p.processBlock(block, midi);
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                finite = finite && std::isfinite(block.getSample(0, i)) && std::isfinite(block.getSample(1, i));
                isolation = std::max(isolation, std::abs(block.getSample(0, i) + block.getSample(1, i)));
                if (start + i < 30000) continue;
                const auto expected = 0.2 * std::sin(juce::MathConstants<double>::twoPi * 1000 * (start + i - expectedHostLatency) / 48000);
                error = std::max(error, std::abs(block.getSample(0, i) - expected));
            }
        }
        std::cout << profile.name << " prepared 1 kHz aligned error " << error << ", stereo polarity error " << isolation
                  << ", host latency " << p.getLatencySamples() << '\n';
        check(preparedCorrectly && finite && error < 0.002 && isolation < 1.0e-5f && p.getActiveProcessingPath() == profile.path
              && p.getLatencySamples() == expectedHostLatency, "each prepared profile reconstructs aligned stereo audio across 1537-sample chunks");
    }
    {
        AuraAudioProcessor p; set(p, "amount", 0); selectProfile(p, processingProfiles[0]); p.prepareToPlay(48000, 1537);
        juce::AudioBuffer<float> block(2, 1537); juce::MidiBuffer midi; int position = 0;
        for (int n = 0; n < 40; ++n)
        { fillOppositeTone(block, position); p.processBlock(block, midi); position += block.getNumSamples(); }
        for (auto index : { 1, 2, 3, 4, 0 })
        {
            const auto& profile = processingProfiles[static_cast<std::size_t>(index)]; selectProfile(p, profile);
            const auto requested = p.getOversamplingMode() == profile.mode && p.getProcessingQuality() == profile.quality
                && p.isProcessingChangePending();
            double error = 0; float isolation = 0; bool finite = true;
            // 61480 host samples include the 24637-sample prime, 50 ms fade,
            // and a complete steady-state analysis window after promotion.
            for (int n = 0; n < 40; ++n)
            {
                fillOppositeTone(block, position); p.processBlock(block, midi);
                for (int i = 0; i < block.getNumSamples(); ++i)
                {
                    const auto expected = 0.2 * std::sin(juce::MathConstants<double>::twoPi * 1000 * (position + i - expectedHostLatency) / 48000);
                    error = std::max(error, std::abs(block.getSample(0, i) - expected));
                    isolation = std::max(isolation, std::abs(block.getSample(0, i) + block.getSample(1, i)));
                    finite = finite && std::isfinite(block.getSample(0, i)) && std::isfinite(block.getSample(1, i));
                }
                position += block.getNumSamples();
            }
            std::cout << "Live -> " << profile.name << " 1 kHz continuity error " << error << ", active path " << p.getActiveProcessingPath() << '\n';
            check(requested && finite && error < 0.002 && isolation < 1.0e-5f && p.getActiveProcessingPath() == profile.path
                  && !p.isProcessingChangePending() && p.getLatencySamples() == expectedHostLatency,
                  "live profile changes finish priming and crossfading with continuous stereo audio and fixed latency");
        }
    }
    {
        AuraAudioProcessor p; bool latestWins = true;
        set(p, "oversampling", 1); latestWins = latestWins && p.getOversamplingMode() == 2;
        set(p, "oversamplingMode", 1); latestWins = latestWins && p.getOversamplingMode() == 1;
        set(p, "oversampling", 0); latestWins = latestWins && p.getOversamplingMode() == 0;
        set(p, "oversamplingMode", 2); latestWins = latestWins && p.getOversamplingMode() == 2;
        check(latestWins, "legacy boolean and canonical mode automation use the most recent host write");
        set(p, "oversampling", 1); p.setOversamplingMode(0);
        set(p, "oversampling", 1);
        check(p.getOversamplingMode() == 2, "repeated legacy true overrides UI 1x when the stored boolean was already true");
        set(p, "oversamplingMode", 0);
        check(p.getOversamplingMode() == 0, "repeated canonical 1x overrides legacy 4x when the stored mode was already 1x");
        p.setCurrentProgram(1); set(p, "oversampling", 0);
        const auto changed = p.isCurrentProgramModified(); p.setOversamplingMode(2);
        check(changed && p.getOversamplingMode() == 2 && !p.isCurrentProgramModified(),
              "restoring Vocal Magic's effective 4x mode clears modified status despite a stale legacy boolean");
    }
    for (int mode = 0; mode < 3; ++mode)
        for (int quality = 0; quality < 2; ++quality)
        {
            AuraAudioProcessor saved; saved.setOversamplingMode(mode); set(saved, "processingQuality", static_cast<float>(quality));
            juce::MemoryBlock state; saved.getStateInformation(state);
            AuraAudioProcessor recalled; recalled.setOversamplingMode((mode + 1) % 3); set(recalled, "processingQuality", static_cast<float>(1 - quality));
            recalled.setStateInformation(state.getData(), static_cast<int>(state.getSize())); recalled.prepareToPlay(48000, 1537);
            const auto expectedPath = mode == 0 ? 0 : 1 + (mode - 1) * 2 + quality;
            check(recalled.getOversamplingMode() == mode && recalled.getProcessingQuality() == quality
                  && recalled.getActiveProcessingPath() == expectedPath && !recalled.isProcessingChangePending()
                  && recalled.getLatencySamples() == expectedHostLatency,
                  "each mode and quality combination recalls its requested settings and prepared processing path");
        }
    for (auto legacyEnabled : { false, true })
    {
        AuraAudioProcessor p; p.setOversamplingMode(1); set(p, "processingQuality", 0);
        auto state = p.parameters.copyState();
        for (auto* id : { "oversamplingMode", "processingQuality" }) state.removeChild(state.getChildWithProperty("id", id), nullptr);
        state.getChildWithProperty("id", "oversampling").setProperty("value", legacyEnabled ? 1.0f : 0.0f, nullptr);
        juce::MemoryBlock binary; auto xml = state.createXml(); juce::AudioProcessor::copyXmlToBinary(*xml, binary);
        p.setStateInformation(binary.getData(), static_cast<int>(binary.getSize())); p.prepareToPlay(48000, 1537);
        check(p.getOversamplingMode() == (legacyEnabled ? 2 : 0) && p.getProcessingQuality() == 1
              && p.getActiveProcessingPath() == (legacyEnabled ? 4 : 0),
              legacyEnabled ? "v1.4 boolean true recalls 4x High without new parameter IDs" : "v1.4 boolean false recalls 1x High without new parameter IDs");
    }
    for (int mode = 0; mode < 3; ++mode)
    {
        aura::OutputAnalyser analyser; analyser.prepare(48000); aura::SpectrumFifo fifo; aura::SpectrumFrame frame;
        for (int i = 0; i < aura::fftSize * 3; ++i)
        {
            const auto sample = 0.2f * std::sin(juce::MathConstants<float>::twoPi * 40 * static_cast<float>(i) / aura::fftSize);
            analyser.process(mode == 1 ? 0 : sample, mode == 2 ? -sample : sample, mode == 0 ? 1 : 2, fifo);
            fifo.readLatest(frame);
        }
        const auto expected = mode == 1 ? 0.2f / std::sqrt(2.0f) : 0.2f;
        check(std::abs(frame.output[40] - expected) < 1.0e-5f, mode == 0 ? "post analyzer calibrates a mono bin-centered tone" : mode == 1 ? "post analyzer includes right-only stereo audio" : "post analyzer retains opposite-polarity stereo energy");
    }
    {
        aura::LoudnessMeter meter; meter.prepare(48000);
        for (int i = 0; i < 2400; ++i) meter.process(i == 0 ? 0.5f : 0.0f, 0, 2);
        check(std::abs(meter.peakDb.load() + 6.0206f) < 0.001f, "sample peak retains brief impulses across GUI frame intervals");
        for (int i = 0; i < 24000; ++i) meter.process(0, 0, 2);
        check(meter.peakDb.load() <= -100, "sample peak window releases after silence");
    }
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        aura::LoudnessMeter meter; meter.prepare(rate);
        for (int i = 0; i < static_cast<int>(rate * 1.2); ++i)
        { const auto value = static_cast<float>(std::pow(10.0, -23.0 / 20) * std::sin(juce::MathConstants<double>::twoPi * 1000 * i / rate)); meter.process(value, value, 2); }
        std::cout << "K-weighted stereo 1 kHz " << rate << " Hz: " << meter.lufs.load() << " LUFS\n";
        check(std::abs(meter.lufs.load() + 23) < 0.1f && std::abs(meter.rms.load() + 26.0103f) < 0.1f, "momentary LUFS and RMS agree with the stereo 1 kHz reference");
        for (int i = 0; i < static_cast<int>(rate * 0.5); ++i) meter.process(0, 0, 2);
        check(meter.lufs.load() < -80 && meter.peakDb.load() <= -100, "momentary loudness releases to silence after its 400 ms window");
    }
    {
        std::array<float, 64> source; std::mt19937 random(51); std::uniform_int_distribution<int> values(-3, 3);
        for (auto& value : source) value = static_cast<float>(values(random));
        aura::SlidingMedian<aura::hpssFrequencyBins> sliding; std::array<float, aura::hpssFrequencyBins> reference;
        auto at = [&source](int i) { return source[static_cast<std::size_t>(juce::jlimit(0, 63, i))]; };
        for (int j = 0; j < aura::hpssFrequencyBins; ++j) reference[static_cast<std::size_t>(j)] = at(j - 8);
        sliding.reset(reference); bool valid = true;
        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < aura::hpssFrequencyBins; ++j) reference[static_cast<std::size_t>(j)] = at(i + j - 8);
            valid = valid && exact(sliding.value(), aura::median(reference)); sliding.advance(at(i - 8), at(i + 9));
        }
        check(valid, "sliding frequency median matches the reference across duplicates and clamped edges");
    }
    {
        std::array<float, aura::hpssTimeFrames> time {}; time[4] = 10;
        std::array<float, aura::hpssFrequencyBins> frequency; frequency.fill(10);
        check(exact(aura::percussiveMask(aura::median(time), aura::median(frequency)), 1), "HPSS classifies a broadband time-local hit");
        time.fill(10); frequency.fill(0); frequency[8] = 10;
        check(exact(aura::percussiveMask(aura::median(time), aura::median(frequency)), 0), "HPSS classifies a persistent narrow harmonic");
        check(exact(aura::percussiveMask(3, 3), 0.5f) && exact(aura::percussiveMask(0, 0), 0), "HPSS soft-mask tie and silence are finite");
    }
    {
        aura::CepstralEnvelope cepstral; cepstral.prepare(48000);
        std::array<float, aura::binCount> original, shifted, corrected, partial, originalEnvelope, newEnvelope, result, halfEnvelope;
        for (int k = 0; k < aura::binCount; ++k)
        {
            const auto x = juce::MathConstants<float>::twoPi * static_cast<float>(k) / aura::fftSize;
            original[static_cast<std::size_t>(k)] = std::exp(-3.0f + 0.8f * std::cos(7 * x) + 0.4f * std::cos(200 * x));
            shifted[static_cast<std::size_t>(k)] = std::exp(-3.0f + 0.8f * std::cos(12 * x) + 0.4f * std::cos(220 * x));
        }
        cepstral.extract(original, originalEnvelope); cepstral.extract(shifted, newEnvelope);
        for (std::size_t k = 0; k < corrected.size(); ++k)
        {
            corrected[k] = shifted[k] * std::exp(originalEnvelope[k] - newEnvelope[k]);
            partial[k] = shifted[k] * std::exp(0.5f * (originalEnvelope[k] - newEnvelope[k]));
        }
        cepstral.extract(corrected, result); cepstral.extract(partial, halfEnvelope);
        float error = 0, halfError = 0;
        for (std::size_t k = 0; k < result.size(); ++k)
        {
            error = std::max(error, std::abs(result[k] - originalEnvelope[k]));
            halfError = std::max(halfError, std::abs(halfEnvelope[k] - 0.5f * (originalEnvelope[k] + newEnvelope[k])));
        }
        std::cout << "Cepstral full/half log-envelope error " << error << " / " << halfError << '\n';
        check(error < 2.0e-5f && halfError < 2.0e-5f, "cepstral lifter restores the original envelope and scales intensity");
    }
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::vector<float> hits(80000); hits[0] = 0.8f; hits[30001] = -0.7f; hits[60011] = 0.5f;
        aura::SpectralSettings settings; settings.low = 20; settings.high = 20000; settings.amount = 1;
        settings.mask = 1 << 9; settings.transientPreserve = 1; settings.formantPreserve = 1;
        const auto protectedHits = render(hits, rate, settings);
        float error = 0;
        for (std::size_t i = 0; i < protectedHits.size(); ++i)
            error = std::max(error, std::abs(protectedHits[i] - (i >= aura::latencySamples ? hits[i - aura::latencySamples] : 0.0f)));
        std::cout << "Protected transients " << rate << " Hz max error " << error << '\n';
        check(error < 2.0e-6f, "PUNCH 100 reconstructs isolated transients without phase-vocoder smearing");
        settings.transientPreserve = settings.formantPreserve = 0;
        const auto processed = render(hits, rate, settings);
        settings.transientPreserve = 0.5f;
        const auto blended = render(hits, rate, settings);
        float blendError = 0, effect = 0;
        for (std::size_t i = 0; i < blended.size(); ++i)
        {
            blendError = std::max(blendError, std::abs(blended[i] - 0.5f * (processed[i] + protectedHits[i])));
            effect = std::max(effect, std::abs(processed[i] - protectedHits[i]));
        }
        check(blendError < 2.0e-5f && effect > 0.01f, "PUNCH crossfades the sweetened percussion and original-phase bypass");
    }
    {
        // Formant-shaped harmonic fixture; compare frames from the actual
        // snapping path, including both HPSS and formant processing.
        std::vector<float> vowel(48000);
        for (std::size_t i = 0; i < vowel.size(); ++i)
            for (int h = 1; h <= 60; ++h)
            {
                const auto hz = 143.0 * h;
                const auto amplitude = (0.02 + std::exp(-0.5 * std::pow((hz - 850.0) / 230.0, 2))
                    + 0.7 * std::exp(-0.5 * std::pow((hz - 2450.0) / 370.0, 2))) * 0.018;
                vowel[i] += static_cast<float>(amplitude * std::sin(juce::MathConstants<double>::twoPi * hz * static_cast<double>(i) / 48000.0));
            }
        aura::SpectralSettings settings; settings.amount = 1; settings.mask = 0xFFF; settings.low = 20; settings.high = 20000;
        float hit = 0, punch = 0, throat = 0;
        settings.formantPreserve = 0;
        const auto plain = capture(vowel, settings, hit, punch, throat);
        check(exact(throat, 0), "THROAT zero reports no envelope correction");
        settings.formantPreserve = 1;
        const auto preserved = capture(vowel, settings, hit, punch, throat);
        const auto before = envelopeDistance(plain), after = envelopeDistance(preserved);
        std::cout << "Synthetic vowel log-envelope RMS error " << before << " -> " << after << '\n';
        // The unchanged percussive component sets a floor on the improvement
        // measured over the combined output. Require a material reduction.
        check(after < before * 0.95 && throat > 0.01f, "THROAT reduces envelope displacement in the complete HPSS/snapping pipeline");
        settings.transientPreserve = 0; settings.formantPreserve = 0;
        const auto unsplitPlain = capture(vowel, settings, hit, punch, throat);
        settings.formantPreserve = 1;
        const auto unsplitPreserved = capture(vowel, settings, hit, punch, throat);
        const auto unsplitBefore = envelopeDistance(unsplitPlain), unsplitAfter = envelopeDistance(unsplitPreserved);
        std::cout << "Unsplit vowel log-envelope RMS error " << unsplitBefore << " -> " << unsplitAfter << '\n';
        check(unsplitAfter < unsplitBefore * 0.85, "THROAT preserves the envelope when PUNCH is zero");
        settings.amount = 0;
        const auto neutralShape = render(vowel, 48000, settings);
        settings.formantShift = 6;
        const auto shiftedShape = render(vowel, 48000, settings);
        double shapeDifference = 0; bool shapeFinite = true;
        for (std::size_t i = 24000; i < shiftedShape.size(); ++i)
        { const auto d = shiftedShape[i] - neutralShape[i]; shapeDifference += d * d; shapeFinite = shapeFinite && std::isfinite(shiftedShape[i]); }
        check(shapeFinite && std::sqrt(shapeDifference / 24000) > 0.005, "Throat shape changes timbre even when pitch Amount is zero");
        settings.formantShift = 0; settings.amount = 1; settings.formantTension = 0;
        const auto broad = render(vowel, 48000, settings);
        settings.formantTension = 1;
        const auto detailed = render(vowel, 48000, settings);
        double tensionDifference = 0;
        for (std::size_t i = 24000; i < detailed.size(); ++i) { const auto d = broad[i] - detailed[i]; tensionDifference += d * d; }
        check(std::sqrt(tensionDifference / 24000) > 0.001, "Tension changes the envelope detail applied to harmonic audio");
        settings.formantTension = 0.5f;
        settings.transientPreserve = 1;
        bool valid = true;
        for (auto value : preserved.envelope) valid = valid && std::isfinite(value) && value > 0;
        check(valid, "FIFO carries a finite positive input formant envelope");
        std::vector<float> impulses(60000); impulses[0] = 1; impulses[30001] = 0.6f;
        hit = punch = throat = 0;
        capture(impulses, settings, hit, punch, throat);
        check(hit > 0.9f && punch > 0.99f, "FIFO transient events and PUNCH energy track preserved hits");
        settings.transientPreserve = 0; hit = punch = throat = 0;
        capture(impulses, settings, hit, punch, throat);
        check(exact(hit, 0) && exact(punch, 0), "PUNCH zero disables bypass telemetry and transient flashes");
    }
    {
        std::vector<float> noise(24000); std::mt19937 random(123); std::uniform_real_distribution<float> distribution(-0.2f, 0.2f);
        for (auto& sample : noise) sample = distribution(random);
        aura::SpectralSettings settings; settings.amount = 1; settings.low = 20; settings.high = 20000;
        float hit = 0, punch = 0, throat = 0; settings.transientSensitivity = 0;
        const auto low = capture(noise, settings, hit, punch, throat);
        settings.transientSensitivity = 1;
        const auto high = capture(noise, settings, hit, punch, throat);
        std::cout << "Sensitivity low/high preserved energy " << low.percussiveLevel << " / " << high.percussiveLevel << '\n';
        check(high.percussiveLevel > low.percussiveLevel + 0.1f, "Sensitivity changes percussive routing on sustained broadband material");
    }
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::vector<float> signal(aura::latencySamples + aura::fftSize * 3);
        std::mt19937 random(42);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        for (auto& sample : signal) sample = dist(random);
        aura::SpectralSettings settings; settings.amount = 0;
        const auto out = render(signal, rate, settings);
        float maxError = 0;
        for (std::size_t i = aura::latencySamples; i < out.size(); ++i)
            maxError = std::max(maxError, std::abs(out[i] - signal[i - aura::latencySamples]));
        std::cout << "Identity " << rate << " Hz max error " << maxError << '\n';
        check(maxError < 2.0e-5f, "Hann OLA reconstructs delayed input");
    }
    {
        aura::SpectralProcessor dsp; dsp.prepare(48000, aura::hopSize / 2);
        aura::SpectralSettings settings; settings.amount = 0;
        std::vector<float> signal(aura::latencySamples + aura::fftSize * 3);
        std::mt19937 random(73); std::uniform_real_distribution<float> distribution(-0.2f, 0.2f);
        for (auto& sample : signal) sample = distribution(random);
        for (const auto index : { 0, aura::hopSize - 1, aura::hopSize, aura::hopSize + 1 }) signal[static_cast<std::size_t>(index)] = 0.8f;
        float error = 0;
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 1) dsp.reset();
            for (std::size_t i = 0; i < signal.size(); ++i)
            { float dry; const auto wet = dsp.processSample(signal[i], dry, settings); const auto expected = i < aura::latencySamples ? 0.0f : signal[i - aura::latencySamples]; error = std::max(error, std::abs(wet - expected)); }
        }
        std::cout << "Offset frame grid OLA error " << error << '\n';
        check(error < 2.0e-5f, "half-hop frame offset reconstructs startup and hop-boundary impulses after reset");
    }
    {
        std::vector<float> signal(aura::latencySamples + aura::fftSize * 2); signal[0] = 1;
        aura::SpectralSettings settings; settings.amount = 0;
        auto out = render(signal, 48000, settings);
        check(std::abs(out[aura::latencySamples] - 1) < 1.0e-5f
              && std::distance(out.begin(), std::max_element(out.begin(), out.end())) == aura::latencySamples, "impulse STFT/HPSS latency is exactly 16384 samples");
    }
    {
        constexpr double rate = 48000;
        std::vector<float> signal(static_cast<std::size_t>(rate * 2));
        for (std::size_t i = 0; i < signal.size(); ++i) signal[i] = static_cast<float>(0.4 * std::sin(juce::MathConstants<double>::twoPi * 430 * static_cast<double>(i) / rate));
        aura::SpectralSettings settings; settings.low = 20; settings.high = 20000; settings.amount = 1; settings.mask = 1 << 9; settings.transientPreserve = settings.formantPreserve = 0;
        const auto out = render(signal, rate, settings);
        int crossings = 0;
        std::size_t first = 0, last = 0;
        for (std::size_t i = 24001; i < out.size(); ++i)
            if (out[i - 1] <= 0 && out[i] > 0) { if (crossings == 0) first = i; last = i; ++crossings; }
        const auto measured = static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first);
        std::cout << "Snapped 430 Hz -> " << measured << " Hz, RMS " << rms(out, 24000) << '\n';
        check(std::abs(measured - 440) < 0.3 && rms(out, 24000) > 0.12 && rms(out, 24000) < 0.5, "sine snaps toward enabled A with usable gain");
        settings.transientPreserve = settings.formantPreserve = 1;
        const auto protectedTone = render(signal, rate, settings);
        crossings = 0; first = last = 0;
        for (std::size_t i = 24001; i < protectedTone.size(); ++i)
            if (protectedTone[i - 1] <= 0 && protectedTone[i] > 0) { if (crossings == 0) first = i; last = i; ++crossings; }
        const auto protectedHz = static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first);
        std::cout << "Default preservation tone " << protectedHz << " Hz, RMS " << rms(protectedTone, 24000) << '\n';
        check(std::abs(protectedHz - 440) < 0.3 && rms(protectedTone, 24000) > 0.1 && rms(protectedTone, 24000) < 0.6, "harmonic snapping remains active with both preservation controls at 100");
        settings.transientPreserve = settings.formantPreserve = 0;
        settings.amount = 0.5f;
        const auto half = render(signal, rate, settings);
        crossings = 0; first = last = 0;
        for (std::size_t i = 24001; i < half.size(); ++i)
            if (half[i - 1] <= 0 && half[i] > 0) { if (crossings == 0) first = i; last = i; ++crossings; }
        const auto halfHz = static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first);
        check(std::abs(halfHz - std::sqrt(430.0 * 440.0)) < 0.3, "half amount interpolates pitch logarithmically");
        settings.amount = 1; settings.low = 1000;
        const auto outside = render(signal, rate, settings);
        float rangeError = 0, startupError = 0;
        for (std::size_t i = aura::latencySamples; i < outside.size(); ++i)
        {
            const auto error = std::abs(outside[i] - signal[i - aura::latencySamples]);
            if (i < aura::latencySamples + aura::fftSize) startupError = std::max(startupError, error);
            else rangeError = std::max(rangeError, error);
        }
        std::cout << "Out-of-range steady/startup max error " << rangeError << " / " << startupError << '\n';
        // The first window contains a hard tone onset whose spectral leakage
        // reaches the active band. Bound that transient to 3% of this fixture's
        // 0.4 peak; require precise reconstruction after the full window.
        check(rangeError < 2.0e-5f && startupError < 0.03f * 0.4f, "out-of-range steady tone is preserved with startup leakage below 3% of input peak");
        std::swap(settings.low, settings.high);
        const auto reversed = render(signal, rate, settings);
        check(reversed == outside, "crossed boundaries produce the same ordered range");
        settings.mask = 0;
        const auto identity = render(signal, rate, settings);
        float error = 0;
        for (std::size_t i = aura::latencySamples; i < identity.size(); ++i) error = std::max(error, std::abs(identity[i] - signal[i - aura::latencySamples]));
        check(error < 1.0e-5f, "empty custom scale preserves audio");
    }
    AuraAudioProcessor processor;
    check(processor.getLatencySamples() >= aura::latencySamples, "processor reports STFT/HPSS plus oversampling filter latency");
    set(processor, "mix", 0); processor.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    std::vector<float> dryOutput;
    for (int block = 0; block < (processor.getLatencySamples() + aura::fftSize) / 512 + 1; ++block)
    {
        buffer.clear(); if (block == 0) { buffer.setSample(0, 0, 1); buffer.setSample(1, 0, -1); }
        processor.processBlock(buffer, midi);
        dryOutput.insert(dryOutput.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 512);
        if (processor.getLatencySamples() / 512 == block) check(exact(buffer.getSample(1, processor.getLatencySamples() % 512), -1), "stereo channel isolation and aligned dry");
    }
    check(exact(dryOutput[static_cast<std::size_t>(processor.getLatencySamples())], 1) && rms(dryOutput, static_cast<std::size_t>(processor.getLatencySamples() + 1)) < 1.0e-20, "mix zero delivers exact delayed dry");
    for (int mode = 0; mode < 3; ++mode)
    {
        AuraAudioProcessor p; set(p, "mix", 0); set(p, "outputGain", -6);
        if (mode == 1) set(p, "outputMute", 1);
        if (mode == 2) { set(p, "outputMute", 1); set(p, "globalBypass", 1); }
        p.prepareToPlay(48000, 256); juce::AudioBuffer<float> block(2, 256);
        float error = 0; aura::SpectrumFrame post;
        const auto gain = mode == 0 ? juce::Decibels::decibelsToGain(-6.0f) : mode == 1 ? 0.0f : 1.0f;
        for (int start = 0; start < 48000; start += 256)
        {
            for (int i = 0; i < 256; ++i)
            { const auto value = 0.2f * std::sin(juce::MathConstants<float>::twoPi * 1000 * static_cast<float>(start + i) / 48000); block.setSample(0, i, value); block.setSample(1, i, value); }
            p.processBlock(block, midi); p.outputSpectrumFifo.readLatest(post);
            for (int i = 0; i < 256; ++i)
            { const auto index = start + i - p.getLatencySamples(); const auto expected = index < 0 ? 0 : gain * 0.2f * std::sin(juce::MathConstants<float>::twoPi * 1000 * static_cast<float>(index) / 48000); error = std::max(error, std::abs(block.getSample(0, i) - expected)); }
        }
        check(error < 2.0e-6f, mode == 0 ? "output gain applies the dB gain to aligned audio" : mode == 1 ? "Mute produces silence" : "global Bypass restores aligned dry independently of gain and Mute");
        const auto peak = *std::max_element(post.output.begin(), post.output.end());
        check(mode == 1 ? peak < 1.0e-9f && p.metering.lufs.load() < -80 : peak > 0.08f * gain, "post spectrum and meter follow actual output controls");
    }
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (auto blockSize : { 3, 127, 512, 2048 })
        {
            AuraAudioProcessor p; set(p, "amount", 0); set(p, "mix", 0.37f);
            p.prepareToPlay(rate, blockSize);
            juce::AudioBuffer<float> block(2, blockSize);
            bool aligned = true;
            for (int start = 0; start < p.getLatencySamples() + aura::fftSize * 2; start += blockSize)
            {
                block.clear();
                if (start == 0) { block.setSample(0, 0, 1); block.setSample(1, 0, -1); }
                p.processBlock(block, midi);
                for (int i = 0; i < blockSize; ++i)
                {
                    const auto expected = start + i == p.getLatencySamples() ? 1.0f : 0.0f;
                    aligned = aligned && std::abs(block.getSample(0, i) - expected) < 2.0e-6f
                        && std::abs(block.getSample(1, i) + expected) < 2.0e-6f;
                }
            }
            check(aligned && p.getLatencySamples() == processor.getLatencySamples(), "wet/dry latency stays aligned across sample rates and non-hop block sizes");
        }
    {
        aura::SpectralProcessor dsp; dsp.prepare(48000); aura::SpectralSettings settings;
        float error = 0;
        for (int i = 0; i < 12000; ++i)
        {
            float dry;
            const auto sample = i == 7 ? std::numeric_limits<float>::quiet_NaN()
                : i == 11 ? std::numeric_limits<float>::infinity() : 0.0f;
            error = std::max(error, std::abs(dsp.processSample(sample, dry, settings)));
            error = std::max(error, std::abs(dry));
        }
        check(exact(error, 0), "silence and non-finite input do not produce audio or cepstral noise");
    }
    {
        aura::SpectralProcessor dsp; dsp.prepare(48000); aura::SpectralSettings settings;
        settings.amount = 1; settings.transientPreserve = settings.formantPreserve = 0;
        float error = 0;
        for (int i = 0; i < 18001 + aura::latencySamples + aura::fftSize; ++i)
        {
            if (i == 4000) settings.transientPreserve = settings.formantPreserve = 1;
            float dry;
            const auto wet = dsp.processSample(i == 18001 ? 0.8f : 0.0f, dry, settings);
            error = std::max(error, std::abs(wet - (i == 18001 + aura::latencySamples ? 0.8f : 0.0f)));
        }
        check(error < 2.0e-6f, "PUNCH automation settles to a fully protected endpoint");
    }
    set(processor, "scaleMode", 7); set(processor, "customNoteBits", 0x249); set(processor, "amount", 0.73f); set(processor, "transientPreserve", 0.62f); set(processor, "formantPreserve", 0.81f);
    set(processor, "scaleTonic", 9); set(processor, "outputGain", -1.5f); set(processor, "formantShift", 2.3f); set(processor, "formantTension", 0.67f); set(processor, "transientSensitivity", 0.71f);
    juce::MemoryBlock state; processor.getStateInformation(state);
    AuraAudioProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(static_cast<int>(restored.parameters.getRawParameterValue("customNoteBits")->load()) == 0x249
          && std::abs(restored.parameters.getRawParameterValue("amount")->load() - 0.73f) < 1.0e-5f, "APVTS state round trip");
    check(std::abs(restored.parameters.getRawParameterValue("transientPreserve")->load() - 0.62f) < 1.0e-5f
          && std::abs(restored.parameters.getRawParameterValue("formantPreserve")->load() - 0.81f) < 1.0e-5f, "v1.2 preservation state round trip");
    check(exact(restored.parameters.getRawParameterValue("scaleTonic")->load(), 9)
          && std::abs(restored.parameters.getRawParameterValue("outputGain")->load() + 1.5f) < 1.0e-5f
          && std::abs(restored.parameters.getRawParameterValue("formantShift")->load() - 2.3f) < 1.0e-5f
          && std::abs(restored.parameters.getRawParameterValue("formantTension")->load() - 0.67f) < 1.0e-5f, "tonic, output and formant pod controls serialize through APVTS");
    auto legacy = processor.parameters.copyState();
    for (auto* id : { "transientPreserve", "formantPreserve", "scaleTonic", "outputGain", "formantShift", "formantTension", "transientSensitivity", "transientBypass", "outputMute", "soloWet", "globalBypass", "oversampling", "oversamplingMode", "processingQuality" }) legacy.removeChild(legacy.getChildWithProperty("id", id), nullptr);
    juce::MemoryBlock oldState; auto oldXml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*oldXml, oldState);
    restored.setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
    check(exact(restored.parameters.getRawParameterValue("transientPreserve")->load(), 1) && exact(restored.parameters.getRawParameterValue("formantPreserve")->load(), 1), "v1.0 state supplies v1.2 defaults");
    std::cout << "Legacy defaults tonic/gain/shape " << restored.parameters.getRawParameterValue("scaleTonic")->load() << " / " << restored.parameters.getRawParameterValue("outputGain")->load() << " / " << restored.parameters.getRawParameterValue("formantShift")->load() << '\n';
    check(exact(restored.parameters.getRawParameterValue("scaleTonic")->load(), 0) && std::abs(restored.parameters.getRawParameterValue("outputGain")->load()) < 1.0e-6f
          && std::abs(restored.parameters.getRawParameterValue("formantShift")->load()) < 1.0e-6f, "legacy states restore C tonic and neutral new output/shape controls");
    check(restored.getOversamplingMode() == 0 && restored.getProcessingQuality() == 1,
          "v1.0 states default to native processing with High resampling quality available");
    restored.setStateInformation("invalid", 7);
    check(static_cast<int>(restored.parameters.getRawParameterValue("customNoteBits")->load()) == 0x249, "invalid state leaves parameters intact");
    {
        aura::SpectrumFifo fifo; aura::SpectrumFrame a, b;
        for (int i = 0; i < 20; ++i) { a.sampleRate = static_cast<float>(i); fifo.push(a); }
        check(fifo.readLatest(b) && exact(b.sampleRate, 6) && !fifo.readLatest(b), "FIFO overflow drops new frames and drains safely");
        a.transientHit = 0.8f; a.percussiveLevel = 0.9f; a.formantCorrection = 0.7f; fifo.push(a);
        a.transientHit = a.percussiveLevel = a.formantCorrection = 0; a.envelope[12] = 0.3f; fifo.push(a);
        check(fifo.readLatest(b) && exact(b.transientHit, 0.8f) && exact(b.percussiveLevel, 0.9f)
              && exact(b.formantCorrection, 0.7f) && exact(b.envelope[12], 0.3f), "FIFO drains latest curve while retaining brief preservation events");
    }
    for (const auto& profile : processingProfiles)
    {
        AuraAudioProcessor p; selectProfile(p, profile); set(p, "amount", 1); set(p, "freqLow", 20); set(p, "freqHigh", 20000);
        set(p, "transientPreserve", 0.62f); set(p, "formantPreserve", 0.81f); set(p, "formantShift", 1.5f);
        p.prepareToPlay(48000, 1537);
        juce::AudioBuffer<float> block(2, 1537); aura::SpectrumFrame frame;
        allocations = 0; float correction = 0; bool finite = true;
        for (int n = 0; n < 40; ++n)
        {
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                const auto index = n * block.getNumSamples() + i;
                const auto sample = static_cast<float>(0.1 * std::sin(juce::MathConstants<double>::twoPi * 430 * index / 48000)
                    + 0.05 * std::sin(juce::MathConstants<double>::twoPi * 770 * index / 48000)) + (index == 9001 ? 0.5f : 0.0f);
                block.setSample(0, i, sample); block.setSample(1, i, -sample);
            }
            // Preparation and fixture generation are outside the watch; include
            // the first callback and enough audio to execute FFT, HPSS and both envelopes.
            watchAllocations = true; p.processBlock(block, midi); watchAllocations = false;
            if (p.spectrumFifo.readLatest(frame)) correction = std::max(correction, frame.formantCorrection);
            for (int i = 0; i < block.getNumSamples(); ++i)
                finite = finite && std::isfinite(block.getSample(0, i)) && std::isfinite(block.getSample(1, i));
        }
        std::cout << profile.name << " first/prepared callbacks C++ allocations " << allocations.load() << ", formant correction " << correction << '\n';
        check(allocations == 0 && finite && correction > 1.0e-5f && p.getActiveProcessingPath() == profile.path,
              "each profile processes its first callback and active FFT/HPSS/cepstral path without C++ heap allocations");
    }
    set(processor, "mix", 1); set(processor, "amount", 1); processor.setOversamplingMode(0); set(processor, "processingQuality", 1); processor.reset();
    auto* modeParameter = processor.parameters.getParameter("oversamplingMode");
    auto* qualityParameter = processor.parameters.getParameter("processingQuality");
    // Include the first callback after reset, including FFT/median/cepstral paths.
    allocations = 0;
    std::array<double, 1125> callbackMs {}; std::size_t timingIndex = 0;
    constexpr std::array<int, 8> schedule { 0, 1, 2, 3, 4, 3, 2, 0 };
    std::array<bool, 5> visited {}; bool settled = true; int requestedPath = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i)
    {
        buffer.clear(); buffer.setSample(0, 0, 0.5f); buffer.setSample(1, 0, -0.5f);
        if (i % 125 == 0)
        {
            const auto& profile = processingProfiles[static_cast<std::size_t>(schedule[static_cast<std::size_t>(i / 125)])];
            requestedPath = profile.path;
            modeParameter->setValueNotifyingHost(modeParameter->convertTo0to1(static_cast<float>(profile.mode)));
            qualityParameter->setValueNotifyingHost(qualityParameter->convertTo0to1(static_cast<float>(profile.quality)));
        }
        watchAllocations = true;
        auto callbackStart = std::chrono::steady_clock::now();
        processor.processBlock(buffer, midi);
        callbackMs[timingIndex++] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - callbackStart).count();
        if (i % 8 == 0)
        {
            callbackStart = std::chrono::steady_clock::now(); processor.processBlockBypassed(buffer, midi);
            callbackMs[timingIndex++] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - callbackStart).count();
        }
        watchAllocations = false;
        const auto active = processor.getActiveProcessingPath();
        if (juce::isPositiveAndBelow(active, static_cast<int>(visited.size()))) visited[static_cast<std::size_t>(active)] = true;
        if (i % 125 == 124) settled = settled && active == requestedPath && !processor.isProcessingChangePending();
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    check(allocations == 0, "no C++ heap allocations through all live mode/quality transitions and host bypass callbacks");
    check(settled && std::all_of(visited.begin(), visited.end(), [](bool value) { return value; }) && timingIndex == callbackMs.size()
          && processor.getLatencySamples() == expectedHostLatency, "all five profiles settle during the 1125 timed callbacks with fixed host latency");
    std::cout << "Stereo processing: " << elapsed << " seconds for 12 seconds of audio\n";
    std::sort(callbackMs.begin(), callbackMs.end());
    std::cout << "512-sample callback p99/max: " << callbackMs[1113] << " / " << callbackMs.back() << " ms (48 kHz budget 10.6667 ms)\n";
    check(std::isfinite(buffer.getSample(0, 0)), "automation/bypass output remains finite");
    std::cout << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
