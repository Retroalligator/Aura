// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include "Scale.h"
#include <array>

namespace aura
{
inline constexpr std::array<const char*, 20> parameterIds {
    "scaleMode", "customNoteBits", "freqLow", "freqHigh", "amount", "mix",
    "transientPreserve", "formantPreserve", "scaleTonic", "transientSensitivity",
    "transientBypass", "formantShift", "formantTension", "outputGain",
    "outputMute", "soloWet", "globalBypass", "oversampling", "oversamplingMode", "processingQuality"
};
struct FactoryPreset { const char* name; std::array<float, parameterIds.size()> values; };
constexpr FactoryPreset preset(const char* name, int scale, int root, float amount, float punch, float formants,
                               float low, float high, float shift, float tension, float sensitivity, float gain, bool x4)
{
    return { name, { static_cast<float>(scale), static_cast<float>(scaleMasks[static_cast<std::size_t>(scale)]),
        low, high, amount, 1, punch, formants, static_cast<float>(root), sensitivity,
        0, shift, tension, gain, 0, 0, 0, x4 ? 1.0f : 0.0f, x4 ? 2.0f : 0.0f, 1.0f } };
}
inline constexpr std::array<FactoryPreset, 5> factoryPresets {
    preset("Default", 0, 0, 0.5f, 1, 1, 80, 12000, 0, 0.5f, 0.5f, 0, false),
    preset("Vocal Magic", 1, 9, 0.74f, 0.9f, 1, 80, 14500, 0, 0.6f, 0.55f, -1.5f, true),
    preset("808 Tuner", 1, 0, 1, 0.75f, 0.35f, 20, 500, 0, 0.4f, 0.65f, -1.5f, false),
    preset("Lush Pad Sweetener", 0, 2, 0.45f, 0.4f, 0.85f, 60, 16000, -0.4f, 0.35f, 0.35f, -1.5f, false),
    preset("Drum Transient Preserver", 6, 0, 0.2f, 1, 0, 80, 14500, 0, 0.5f, 0.85f, -1.5f, true)
};
}
