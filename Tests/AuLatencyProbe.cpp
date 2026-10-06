// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#include <AudioToolbox/AudioToolbox.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
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
            for (const auto realTime : {false, true})
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
            UInt32 parameterBytes = 0;
            require(AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &parameterBytes, nullptr), "parameter list size");
            std::vector<AudioUnitParameterID> parameters(parameterBytes / sizeof(AudioUnitParameterID));
            require(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, parameters.data(), &parameterBytes), "parameter list");
            bool foundMode = false;
            for (auto id : parameters)
            {
                AudioUnitParameterInfo info {}; UInt32 infoBytes = sizeof(info);
                require(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &info, &infoBytes), "parameter info");
                char name[256] {};
                if (info.cfNameString != nullptr) CFStringGetCString(info.cfNameString, name, sizeof(name), kCFStringEncodingUTF8);
                else std::strncpy(name, info.name, sizeof(name) - 1);
                if (std::strcmp(name, "Real-time mode") == 0)
                {
                    if ((info.flags & kAudioUnitParameterFlag_NonRealTime) == 0) throw std::runtime_error("resolution mode must be a non-automatable preference");
                    require(AudioUnitSetParameter(unit, id, kAudioUnitScope_Global, 0, realTime ? 1.0f : 0.0f, 0), "select resolution");
                    foundMode = true;
                }
                if (info.cfNameString != nullptr && (info.flags & kAudioUnitParameterFlag_CFNameRelease) != 0) CFRelease(info.cfNameString);
            }
            if (!foundMode) throw std::runtime_error("Real-time setting missing");
            require(AudioUnitInitialize(unit), "initialize AU");
            Float64 latency = 0; UInt32 size = sizeof(latency);
            require(AudioUnitGetProperty(unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &size), "get latency");
            const auto samples = latency * rate;
            std::cout << (realTime ? "Real-time " : "Studio ") << "prepared AU " << rate << " Hz: " << latency << " seconds, " << samples << " samples\n";
            require(AudioUnitUninitialize(unit), "uninitialize AU");
            require(AudioComponentInstanceDispose(unit), "dispose AU");
            if (std::abs(samples - (realTime ? 8253 : 16445)) > 0.01) throw std::runtime_error("incorrect host latency");
        }
        std::cout << "PASS prepared AU latency is 8253 / 16445 samples in both modes at all four rates\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
