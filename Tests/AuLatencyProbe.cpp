// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include <AudioToolbox/AudioToolbox.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
static void require(OSStatus result, const char* operation)
{
    if (result != noErr) { std::cerr << operation << ": " << result << '\n'; throw std::runtime_error(operation); }
}
int main()
{
    try
    {
        const AudioComponentDescription description { kAudioUnitType_Effect, 0x41757261, 0x41533031, 0, 0 };
        const auto component = AudioComponentFindNext(nullptr, &description);
        if (component == nullptr) throw std::runtime_error("Aura component missing");
        for (const auto rate : {44100.0, 48000.0, 96000.0, 192000.0})
        {
            AudioComponentInstance unit = nullptr;
            require(AudioComponentInstanceNew(component, &unit), "create AU");
            for (const auto scope : { kAudioUnitScope_Input, kAudioUnitScope_Output })
            {
                AudioStreamBasicDescription format {}; UInt32 size = sizeof(format);
                require(AudioUnitGetProperty(unit, kAudioUnitProperty_StreamFormat, scope, 0, &format, &size), "get format");
                format.mSampleRate = rate;
                require(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, scope, 0, &format, sizeof(format)), "set sample rate");
            }
            require(AudioUnitInitialize(unit), "initialize AU");
            Float64 latency = 0; UInt32 size = sizeof(latency);
            require(AudioUnitGetProperty(unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &size), "get latency");
            const auto samples = latency * rate;
            std::cout << "Prepared AU " << rate << " Hz: " << latency << " seconds, " << samples << " samples\n";
            require(AudioUnitUninitialize(unit), "uninitialize AU");
            require(AudioComponentInstanceDispose(unit), "dispose AU");
            if (std::abs(samples - 16445) > 0.01) throw std::runtime_error("incorrect host latency");
        }
        std::cout << "PASS prepared AU latency is exactly16445 samples at all four rates\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
