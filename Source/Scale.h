// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace aura
{
inline constexpr std::array<const char*, 8> scaleNames {
    "Major", "Natural Minor", "Harmonic Minor", "Dorian", "Lydian",
    "Pentatonic", "Chromatic", "Custom" };
inline constexpr std::array<const char*, 12> noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
// Stored masks are intervals from the selected tonic; C is the legacy default.
inline constexpr std::array<std::uint16_t, 7> scaleMasks {
    0xAB5, 0x5AD, 0x9AD, 0x6AD, 0xAD5, 0x295, 0xFFF };
inline std::uint16_t noteMask(int mode, int custom, int tonic = 0) noexcept
{
    const auto base = mode >= 0 && mode < 7 ? scaleMasks[static_cast<std::size_t>(mode)]
                                          : static_cast<std::uint16_t>(custom & 0xFFF);
    const auto root = (tonic % 12 + 12) % 12;
    return static_cast<std::uint16_t>(((base << root) | (base >> (12 - root))) & 0xFFF);
}
inline float nearestNoteHz(float hz, std::uint16_t mask) noexcept
{
    if (mask == 0 || hz <= 0.0f || !std::isfinite(hz)) return hz;
    const auto midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    const auto centre = static_cast<int>(std::floor(midi));
    auto best = centre;
    auto distance = 100.0f;
    for (int note = centre - 12; note <= centre + 12; ++note)
    {
        const auto pitchClass = (note % 12 + 12) % 12;
        if ((mask & (1u << pitchClass)) != 0)
        {
            const auto d = std::abs(static_cast<float>(note) - midi);
            if (d < distance) { distance = d; best = note; }
        }
    }
    return 440.0f * std::exp2((static_cast<float>(best) - 69.0f) / 12.0f);
}
}
