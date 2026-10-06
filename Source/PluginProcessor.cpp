// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ProcessingPaths.h"
#include <cstring>

static_assert(std::atomic<float>::is_always_lock_free, "Audio parameter reads must be lock-free");

struct AuraAudioProcessor::ProcessingState
{
    static constexpr int chunkSize = aura::processingChunkSize;
    std::array<std::unique_ptr<aura::ProcessingPath>, 10> paths {
        std::make_unique<aura::NativePath>(),
        std::make_unique<aura::ResampledPath<1>>(false), std::make_unique<aura::ResampledPath<1>>(true),
        std::make_unique<aura::ResampledPath<2>>(false), std::make_unique<aura::ResampledPath<2>>(true),
        std::make_unique<aura::BasicNativePath<12>>(),
        std::make_unique<aura::ResampledPath<1, 12>>(false), std::make_unique<aura::ResampledPath<1, 12>>(true),
        std::make_unique<aura::ResampledPath<2, 12>>(false), std::make_unique<aura::ResampledPath<2, 12>>(true)
    };
    // Keep both dry timelines warm; each resolution has its own actual delay.
    std::array<std::array<aura::FixedDelay<aura::latencySamples + 256>, 2>, 2> dryDelay;
    std::array<std::array<float, chunkSize>, 2> input {};
    aura::OutputAnalyser analyser;
    juce::SmoothedValue<float> fade, resolutionGain;
    enum class ResolutionStage { normal, fadeOut, awaitHost, fadeIn };
    ResolutionStage resolutionStage = ResolutionStage::normal;
    int filterLatency = 0, active = 0, pending = -1, primeRemaining = 0, resumeRemaining = 0;
    std::uint64_t hostPosition = 0, latencySequence = 0, notification = 0;
    double sampleRate = 48000;
    ProcessingState()
    {
        for (const auto& path : paths) filterLatency = std::max(filterLatency, path->filterLatency());
        jassert(filterLatency <= 256);
    }
    static int bank(int path) noexcept { return path >= 5 ? 1 : 0; }
    static int spectralLatency(int path) noexcept { return bank(path) != 0 ? aura::latencySamples / 2 : aura::latencySamples; }
    static int analysisSize(int path) noexcept { return bank(path) != 0 ? aura::fftSize / 2 : aura::fftSize; }
    static int pathIndex(int mode, int qualityMode, bool realTime = false) noexcept
    { return (realTime ? 5 : 0) + (mode == 0 ? 0 : 1 + (mode - 1) * 2 + qualityMode); }
    int latency() const noexcept { return spectralLatency(active) + filterLatency; }
    void prepare(double rate, int mode, int qualityMode, bool realTime) noexcept
    {
        sampleRate = rate;
        constexpr std::array<int, 5> offsets { 0, 256, 768, 1024, 1536 };
        for (std::size_t i = 0; i < paths.size(); ++i)
            paths[i]->prepare(rate, filterLatency - paths[i]->filterLatency(), offsets[i % 5] / (i >= 5 ? 2 : 1));
        for (int b = 0; b < 2; ++b)
            for (auto& delay : dryDelay[static_cast<std::size_t>(b)]) delay.prepare((b == 0 ? aura::latencySamples : aura::latencySamples / 2) + filterLatency);
        analyser.prepare(rate); fade.reset(rate, 0.05); resolutionGain.reset(rate, 0.025);
        active = pathIndex(mode, qualityMode, realTime); clearTransition(); hostPosition = 0;
    }
    void clearTransition() noexcept
    {
        pending = -1; primeRemaining = 0; resumeRemaining = 0; notification = 0;
        resolutionStage = ResolutionStage::normal;
        fade.setCurrentAndTargetValue(0); resolutionGain.setCurrentAndTargetValue(1);
    }
    void reset(int mode, int qualityMode, bool committedRealTime) noexcept
    {
        // reset() keeps the resolution already reported to the host, including
        // a reset triggered synchronously by its latency-change notification.
        // A new request still follows
        // the normal host-notification protocol on subsequent callbacks.
        const auto realTime = committedRealTime;
        const auto interruptedResolution = resolutionStage != ResolutionStage::normal;
        for (auto& path : paths) path->restart(0);
        for (auto& delays : dryDelay) for (auto& delay : delays) delay.reset();
        analyser.reset(); active = pathIndex(mode, qualityMode, realTime);
        clearTransition(); hostPosition = 0;
        if (interruptedResolution)
        {
            // A host reset discards the primed histories. Keep silence until
            // the committed bank has valid audio, then fade in without a step.
            resolutionGain.setCurrentAndTargetValue(0);
            resumeRemaining = spectralLatency(active) + filterLatency + analysisSize(active);
            resolutionStage = ResolutionStage::fadeIn;
        }
    }
    void request(int desired) noexcept
    {
        if (resolutionStage != ResolutionStage::normal) return;
        // A superseded request can be cancelled while its output is inaudible.
        // Finish an in-progress fade before applying a later request.
        if (pending >= 0 && primeRemaining > 0 && pending != desired) pending = -1;
        if (pending >= 0 || desired == active) return;
        pending = desired; paths[static_cast<std::size_t>(pending)]->restart(hostPosition);
        primeRemaining = spectralLatency(pending) + filterLatency + analysisSize(pending);
        fade.setCurrentAndTargetValue(0);
    }
    float nextFade() noexcept
    {
        if (pending < 0) return 0;
        if (primeRemaining > 0)
        {
            if (--primeRemaining == 0)
            {
                if (bank(pending) == bank(active)) fade.setTargetValue(1);
                else { resolutionStage = ResolutionStage::fadeOut; resolutionGain.setTargetValue(0); }
            }
            return 0;
        }
        return bank(pending) == bank(active) ? fade.getNextValue() : 0;
    }
    float nextResolutionGain() noexcept
    {
        if (resumeRemaining > 0 && --resumeRemaining == 0) resolutionGain.setTargetValue(1);
        return resolutionGain.getNextValue();
    }
    void finishChunk(std::atomic<std::uint64_t>& request, const std::atomic<std::uint64_t>& acknowledged) noexcept
    {
        if (resolutionStage == ResolutionStage::fadeOut && !resolutionGain.isSmoothing())
        {
            resolutionStage = ResolutionStage::awaitHost;
            notification = (++latencySequence * 2) + static_cast<std::uint64_t>(bank(pending));
            request.store(notification, std::memory_order_release);
        }
        if (resolutionStage == ResolutionStage::awaitHost && acknowledged.load(std::memory_order_acquire) == notification)
        {
            active = pending; pending = -1;
            resolutionStage = ResolutionStage::fadeIn; resolutionGain.setTargetValue(1);
        }
        if (resolutionStage == ResolutionStage::fadeIn && resumeRemaining == 0 && !resolutionGain.isSmoothing()) resolutionStage = ResolutionStage::normal;
        if (pending >= 0 && bank(pending) == bank(active) && primeRemaining == 0 && !fade.isSmoothing())
        { active = pending; pending = -1; }
    }
    bool transitioning() const noexcept { return pending >= 0 || resolutionStage != ResolutionStage::normal; }
};
AuraAudioProcessor::AuraAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AuraState", createParameterLayout()), processing(std::make_unique<ProcessingState>())
{
    scale = parameters.getRawParameterValue("scaleMode");
    custom = parameters.getRawParameterValue("customNoteBits");
    low = parameters.getRawParameterValue("freqLow");
    high = parameters.getRawParameterValue("freqHigh");
    amount = parameters.getRawParameterValue("amount");
    mix = parameters.getRawParameterValue("mix");
    transientPreserve = parameters.getRawParameterValue("transientPreserve");
    formantPreserve = parameters.getRawParameterValue("formantPreserve");
    tonic = parameters.getRawParameterValue("scaleTonic"); sensitivity = parameters.getRawParameterValue("transientSensitivity");
    transientBypass = parameters.getRawParameterValue("transientBypass"); formantShift = parameters.getRawParameterValue("formantShift");
    formantTension = parameters.getRawParameterValue("formantTension"); outputGain = parameters.getRawParameterValue("outputGain");
    outputMute = parameters.getRawParameterValue("outputMute"); soloWet = parameters.getRawParameterValue("soloWet");
    globalBypass = parameters.getRawParameterValue("globalBypass");
    quality = parameters.getRawParameterValue("processingQuality");
    realTimeMode = parameters.getRawParameterValue("realTimeMode");
    legacyOversamplingIndex = parameters.getParameter("oversampling")->getParameterIndex();
    parameters.getParameter("oversampling")->addListener(this);
    parameters.getParameter("oversamplingMode")->addListener(this);
    for (std::size_t i = 0; i < aura::parameterIds.size(); ++i)
    { programParameters[i] = parameters.getParameter(aura::parameterIds[i]); programValues[i] = parameters.getRawParameterValue(aura::parameterIds[i]); }
    setLatencySamples(processing->latency());
    startTimer(20);
}
AuraAudioProcessor::~AuraAudioProcessor()
{
    stopTimer();
    parameters.getParameter("oversampling")->removeListener(this);
    parameters.getParameter("oversamplingMode")->removeListener(this);
}
void AuraAudioProcessor::timerCallback()
{
    // JUCE's host notification takes listener locks. Issue it here, never on
    // the audio thread. Output is faded to silence until this acknowledgement.
    const juce::ScopedLock callbackLock(getCallbackLock());
    if (const auto request = latencyRequest.exchange(0, std::memory_order_acquire); request != 0)
    {
        setLatencySamples(((request & 1) != 0 ? aura::latencySamples / 2 : aura::latencySamples) + processing->filterLatency);
        latencyAcknowledged.store(request, std::memory_order_release);
    }
}
void AuraAudioProcessor::parameterValueChanged(int index, float value)
{
    // Direct parameter events include repeated automation values. APVTS listeners
    // omit unchanged values, which would break the retained Boolean alias after
    // the new mode selector overrides it. Never write parameters from this event.
    requestedOversampling.store(index == legacyOversamplingIndex ? (value > 0.5f ? 2 : 0)
        : juce::jlimit(0, 2, juce::roundToInt(value * 2.0f)), std::memory_order_relaxed);
}
void AuraAudioProcessor::setOversamplingMode(int mode)
{
    mode = juce::jlimit(0, 2, mode);
    auto* parameter = parameters.getParameter("oversamplingMode");
    parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(mode))); parameter->endChangeGesture();
    requestedOversampling.store(mode, std::memory_order_relaxed);
}
void AuraAudioProcessor::setCurrentProgram(int index)
{
    if (!juce::isPositiveAndBelow(index, getNumPrograms())) return;
    currentProgram.store(index, std::memory_order_relaxed);
    const auto& preset = aura::factoryPresets[static_cast<std::size_t>(index)];
    for (std::size_t i = 0; i < programParameters.size(); ++i)
        programParameters[i]->setValueNotifyingHost(programParameters[i]->convertTo0to1(preset.values[i]));
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
}
const juce::String AuraAudioProcessor::getProgramName(int index)
{
    return juce::isPositiveAndBelow(index, getNumPrograms()) ? aura::factoryPresets[static_cast<std::size_t>(index)].name : juce::String();
}
bool AuraAudioProcessor::isCurrentProgramModified() const noexcept
{
    const auto& preset = aura::factoryPresets[static_cast<std::size_t>(currentProgram.load(std::memory_order_relaxed))];
    for (std::size_t i = 0; i < programValues.size(); ++i)
    {
        if (std::strcmp(aura::parameterIds[i], "oversampling") == 0) continue;
        const auto value = std::strcmp(aura::parameterIds[i], "oversamplingMode") == 0
            ? static_cast<float>(getOversamplingMode()) : programValues[i]->load(std::memory_order_relaxed);
        if (std::abs(value - preset.values[i]) > std::max(1.0e-5f, std::abs(preset.values[i]) * 1.0e-6f)) return true;
    }
    return false;
}
juce::AudioProcessorValueTreeState::ParameterLayout AuraAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray names;
    for (auto* name : aura::scaleNames) names.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scaleMode", 1}, "Scale", names, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"customNoteBits", 1}, "Custom notes", 0, 4095, aura::scaleMasks[0]));
    auto range = juce::NormalisableRange<float>(20.0f, 20000.0f);
    range.setSkewForCentre(1000.0f);
    const auto hzAttributes = juce::AudioParameterFloatAttributes().withLabel("Hz");
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"freqLow", 1}, "Low frequency", range, 80.0f, hzAttributes));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"freqHigh", 1}, "High frequency", range, 12000.0f, hzAttributes));
    const auto percent = juce::AudioParameterFloatAttributes().withLabel("%")
        .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)); })
        .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() * 0.01f; });
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"amount", 1}, "Amount", juce::NormalisableRange<float>(0, 1), 0.5f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix", 1}, "Mix", juce::NormalisableRange<float>(0, 1), 1.0f, percent));
    // Append new parameters; retain the v1.0 IDs, order and AU version hints.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"transientPreserve", 2}, "Punch", juce::NormalisableRange<float>(0, 1), 1.0f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"formantPreserve", 2}, "Throat", juce::NormalisableRange<float>(0, 1), 1.0f, percent));
    juce::StringArray tonics; for (auto* name : aura::noteNames) tonics.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scaleTonic", 3}, "Tonic", tonics, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"transientSensitivity", 3}, "Transient sensitivity", juce::NormalisableRange<float>(0, 1), 0.5f, percent));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"transientBypass", 3}, "Bypass transient preservation", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"formantShift", 3}, "Throat shape", juce::NormalisableRange<float>(-12, 12, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("st")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"formantTension", 3}, "Formant tension", juce::NormalisableRange<float>(0, 1), 0.5f, percent));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"outputGain", 3}, "Output gain", juce::NormalisableRange<float>(-24, 12, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"outputMute", 3}, "Mute output", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"soloWet", 3}, "Solo processed signal", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"globalBypass", 3}, "Bypass Aura", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"oversampling", 4}, "x4 oversampling", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"oversamplingMode", 5}, "Oversampling", juce::StringArray { "1x", "2x", "4x" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"processingQuality", 5}, "Resampling quality", juce::StringArray { "Standard", "High" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"realTimeMode", 6}, "Real-time mode", false, juce::AudioParameterBoolAttributes().withAutomatable(false)));
    return layout;
}
void AuraAudioProcessor::prepareToPlay(double rate, int)
{
    latencyRequest.store(0, std::memory_order_relaxed);
    latencyAcknowledged.store(0, std::memory_order_relaxed);
    processing->prepare(rate, getOversamplingMode(), getProcessingQuality(), isRealTimeMode());
    activePath.store(processing->active, std::memory_order_relaxed);
    processingTransition.store(false, std::memory_order_relaxed);
    mixSmooth.reset(rate, 0.025);
    mixSmooth.setCurrentAndTargetValue(globalBypass->load() > 0.5f ? 0.0f : soloWet->load() > 0.5f ? 1.0f : mix->load());
    gainSmooth.reset(rate, 0.025);
    gainSmooth.setCurrentAndTargetValue(globalBypass->load() > 0.5f ? 1.0f : outputMute->load() > 0.5f ? 0.0f : juce::Decibels::decibelsToGain(outputGain->load()));

    metering.prepare(rate);
    prepared = true;
    setLatencySamples(processing->latency());
}
void AuraAudioProcessor::reset()
{
    latencyRequest.store(0, std::memory_order_relaxed);
    processing->reset(getOversamplingMode(), getProcessingQuality(), getLatencySamples() < aura::latencySamples);
    activePath.store(processing->active, std::memory_order_relaxed);
    processingTransition.store(processing->transitioning(), std::memory_order_relaxed);
    const auto bypass = globalBypass->load() > 0.5f;
    mixSmooth.setCurrentAndTargetValue(bypass ? 0.0f : soloWet->load() > 0.5f ? 1.0f : mix->load());
    gainSmooth.setCurrentAndTargetValue(bypass ? 1.0f : outputMute->load() > 0.5f ? 0.0f : juce::Decibels::decibelsToGain(outputGain->load()));

    metering.reset();
}
bool AuraAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && input == layout.getMainOutputChannelSet();
}
void AuraAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{ process(buffer, false); }
void AuraAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{ process(buffer, true); }
void AuraAudioProcessor::process(juce::AudioBuffer<float>& buffer, bool bypassed) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    if (!prepared) { buffer.clear(); return; }
    aura::SpectralSettings settings;
    settings.mask = aura::noteMask(static_cast<int>(scale->load()), static_cast<int>(custom->load()), static_cast<int>(tonic->load()));
    settings.low = low->load(); settings.high = high->load(); settings.amount = amount->load();
    settings.transientPreserve = transientBypass->load() > 0.5f ? 0.0f : transientPreserve->load();
    settings.formantPreserve = formantPreserve->load();
    settings.transientSensitivity = sensitivity->load(); settings.formantShift = formantShift->load(); settings.formantTension = formantTension->load();
    const auto bypass = bypassed || globalBypass->load() > 0.5f;
    mixSmooth.setTargetValue(bypass ? 0.0f : soloWet->load() > 0.5f ? 1.0f : mix->load());
    gainSmooth.setTargetValue(bypass ? 1.0f : outputMute->load() > 0.5f ? 0.0f : juce::Decibels::decibelsToGain(outputGain->load()));
    const auto count = juce::jmin(2, juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels()));
    for (int c = count; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, buffer.getNumSamples());
    auto& state = *processing;
    const auto desired = ProcessingState::pathIndex(getOversamplingMode(), getProcessingQuality(), isRealTimeMode());
    for (int offset = 0; offset < buffer.getNumSamples(); offset += ProcessingState::chunkSize)
    {
        state.request(desired);
        const auto size = juce::jmin(ProcessingState::chunkSize, buffer.getNumSamples() - offset);
        std::array<const float*, 2> pointers { state.input[0].data(), state.input[1].data() };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < size; ++i)
            {
                const auto sample = c < count ? buffer.getSample(c, offset + i) : 0.0f;
                state.input[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)] = std::isfinite(sample) ? sample : 0.0f;
            }
        const juce::dsp::AudioBlock<const float> input(pointers.data(), 2, static_cast<std::size_t>(size));
        auto& current = *state.paths[static_cast<std::size_t>(state.active)];
        current.process(input, count, settings, &spectrumFifo);
        auto* pending = state.pending >= 0 ? state.paths[static_cast<std::size_t>(state.pending)].get() : nullptr;
        if (pending != nullptr) pending->process(input, count, settings, nullptr);
        for (int i = 0; i < size; ++i)
        {
            const auto wetMix = mixSmooth.getNextValue(), gain = gainSmooth.getNextValue(), fade = state.nextFade();
            const auto resolutionGain = state.nextResolutionGain();
            for (int c = 0; c < count; ++c)
            {
                const auto channel = static_cast<std::size_t>(c);
                const auto a = current.sample(c, i), b = pending != nullptr ? pending->sample(c, i) : a;
                std::array<float, 2> dryBanks {};
                for (std::size_t bank = 0; bank < dryBanks.size(); ++bank) dryBanks[bank] = state.dryDelay[bank][channel].process(state.input[channel][static_cast<std::size_t>(i)]);
                const auto dry = dryBanks[static_cast<std::size_t>(ProcessingState::bank(state.active))];
                const auto wet = a + fade * (b - a);
                buffer.setSample(c, offset + i, (dry + wetMix * (wet - dry)) * gain * resolutionGain);
            }
            const auto l = count > 0 ? buffer.getSample(0, offset + i) : 0.0f, r = count > 1 ? buffer.getSample(1, offset + i) : 0.0f;
            metering.process(l, r, count); state.analyser.process(l, r, count, outputSpectrumFifo);
        }
        state.hostPosition += static_cast<std::uint64_t>(size); state.finishChunk(latencyRequest, latencyAcknowledged);
        activePath.store(state.active, std::memory_order_relaxed);
        processingTransition.store(state.transitioning(), std::memory_order_relaxed);
    }
}
juce::AudioProcessorEditor* AuraAudioProcessor::createEditor() { return new AuraAudioProcessorEditor(*this); }
void AuraAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    state.setProperty("factoryPreset", currentProgram.load(std::memory_order_relaxed), nullptr);
    state.getChildWithProperty("id", "oversamplingMode").setProperty("value", getOversamplingMode(), nullptr);
    state.getChildWithProperty("id", "oversampling").setProperty("value", getOversamplingMode() == 2 ? 1.0f : 0.0f, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void AuraAudioProcessor::setStateInformation(const void* data, int size)
{
    if (size <= 0 || data == nullptr) return;
    if (auto xml = getXmlFromBinary(data, size); xml != nullptr && xml->hasTagName("AuraState"))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        const auto modeState = state.getChildWithProperty("id", "oversamplingMode");
        const auto canonicalMode = modeState.isValid() ? juce::jlimit(0, 2, static_cast<int>(modeState.getProperty("value", 0)))
            : (static_cast<float>(state.getChildWithProperty("id", "oversampling").getProperty("value", 0.0f)) > 0.5f ? 2 : 0);
        for (auto* id : { "transientPreserve", "formantPreserve", "scaleTonic", "transientSensitivity", "transientBypass", "formantShift", "formantTension", "outputGain", "outputMute", "soloWet", "globalBypass", "oversampling", "oversamplingMode", "processingQuality", "realTimeMode" })
        {
            if (!state.getChildWithProperty("id", id).isValid())
            {
                juce::ValueTree value("PARAM");
                value.setProperty("id", id, nullptr);
                auto* parameter = parameters.getParameter(id);
                value.setProperty("value", juce::String(id) == "oversamplingMode" ? static_cast<float>(canonicalMode) : parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
                state.addChild(value, -1, nullptr);
            }
        }
        currentProgram.store(juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(state.getProperty("factoryPreset", 0))), std::memory_order_relaxed);
        parameters.replaceState(state);
        requestedOversampling.store(canonicalMode, std::memory_order_relaxed);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraAudioProcessor(); }
