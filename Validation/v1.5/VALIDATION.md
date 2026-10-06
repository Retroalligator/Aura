# Aura 1.5.0 validation

Tested October 6, 2026 on Apple silicon, macOS 26.3.1 (25D771280a), AppleClang 17,
macOS 26.1 SDK and CMake 4.3.4. AU and VST3 binaries contain arm64 and x86_64
slices and target macOS 11.0+. macOS 11 hardware was not exercised. Earlier reports
and evidence remain in `Validation/v1.0/`, `v1.2/`, `v1.3/` and `v1.4/`.

| Check | Result |
|---|---|
| Release Universal 2 AU / VST3 build | Passed |
| Native CTest / signal and processor regression suite | Passed, zero failures |
| Same regression executable, x86_64 / Rosetta | Passed, zero failures |
| All five prepared profiles and completed live transitions | Passed |
| All mode/quality state combinations and older Boolean state recall | Passed |
| Repeated canonical / legacy automation and effective factory identity | Passed |
| Native and Rosetta `auval`, installed final-image AU 1.5.0 | AU VALIDATION SUCCEEDED on both |
| pluginval 1.0.4 AU, strictness 10, GUI enabled | SUCCESS |
| Prepared AU latency property, both architectures / four rates | Exactly 16445 host samples |
| Native OpenGL settings / color response / reduced motion / factory recall | Observed and exercised |
| Final DMG and both contained bundles: strict signatures / versions / slices | Passed, ad-hoc signatures, 1.5.0, Universal 2 |
| Compressed DMG checksum | VALID; adjacent SHA-256 provided |
| Direct Logic insertion / playback / project recall | Not verified for v1.5 |
| Further FL Studio / VST3 host validation | Stopped at the user's request |

The native STFT remains 8192 points, Hann analysis/synthesis, 75% overlap and
2048-sample hops. Four HPSS lookahead frames add 8192 samples to the STFT delay.
Maximum FIR delay and path alignment add 61, producing fixed host latency
**16445 samples** (342.60 ms at 48 kHz). Real 2x and 4x processing use 16384- and
32768-point internal FFTs at their multiplied rates, retaining the physical
analysis window and frequency resolution. Standard uses shorter FIR filters;
High uses steeper transitions and stronger stopband rejection in JUCE's
oversampling stages. Quality does not change native processing.

Five complete profiles are allocated before callbacks. Only the selected path
runs in steady state. When a change is requested, the incoming path starts with
cleared histories, primes for 24637 host samples while the previous path supplies
the output, then fades in over 50 ms. At 48 kHz, the total settling time is about
0.563 seconds plus at most one scratch chunk. Superseded requests can cancel
during priming; changes during a fade finish that fade before moving again.
A fixed central dry delay keeps Mix and bypass aligned independently of mode.

All five prepared profiles reconstruct a 0.2-peak, 1 kHz opposite-polarity stereo
signal through 1537-sample blocks. The maximum native / Rosetta errors are:

| Profile | Native | Rosetta |
|---|---:|---:|
| 1x | 7.71871e-8 | 7.76145e-8 |
| 2x Standard | 0.000123721 | 0.000123751 |
| 2x High | 2.96235e-6 | 2.99215e-6 |
| 4x Standard | 0.000270200 | 0.000270200 |
| 4x High | 0.0000705946 | 0.0000705983 |

Stereo polarity error is zero in these fixtures. A full native -> 2x Standard ->
2x High -> 4x Standard -> 4x High -> native cycle holds each mode for 61480 samples,
asserts its actual active path after priming/fading, and has a maximum tone
continuity error of 0.0002702 on both architectures. This includes the first
restart callback and complete fades, rather than only rapidly cancelled requests.
These measurements are not universal waveform identity: resampled percussion
passes through anti-alias filters. Mix zero and settled bypass use aligned dry.

All six mode/quality combinations serialize and prepare correctly, including the
stored quality while at 1x. States missing the new choices migrate legacy Boolean
false/true to 1x/4x with High quality. Direct parameter listeners handle repeated
host writes of either the legacy Boolean or new mode. Saved states canonicalize
the effective mode; modified-preset status uses the effective setting rather than
the redundant legacy value. Five factory programs round-trip their complete
20-parameter settings and program identity. New choices append AU version hint 5;
older IDs/order/hints/defaults remain compatible.

The allocation harness reports zero C++ new/new[] calls from every profile's first
prepared callback through active FFT/HPSS/cepstral work. A separate 1000 processing
plus 125 host-bypass callback fixture completes transitions among all five profiles
without C++ allocations. It does not interpose every C or aligned allocator.
JUCE's vDSP backend is required at compile time; fixed storage and lock-free
parameter atomics are used in the processing paths. No parameter/tree writes,
logging or host notifications are performed by processing or the mode listener.

For the 512-sample, 48 kHz timed fixture, callback p99 / maximum were
**4.26775 / 4.89617 ms native** and **7.11575 / 7.18979 ms Rosetta**, below the
10.6667 ms budget in these runs. The fixture took 1.15243 seconds native and
1.82790 seconds under Rosetta for 12 seconds of stereo audio and includes complete
mode/quality transitions and bypass. These are offline wall-clock observations on
this computer, not guarantees for smaller buffers, higher rates, OS preemption,
or a loaded DAW project.

The remaining suite passes tonic/custom interval rotation and actual pitch changes,
HPSS/transient controls, formant shape/tension, empty scales, signal onset/hop
reconstruction, silence/non-finite input, FIFO overflow/event retention, output
routing and calibrated final stereo analysis/meters. Real-only cepstral envelopes
match normalized complex-IFFT/FFT references across three tensions within 4.76837e-7
at 8192 points, 2.38419e-6 at 16384, and 2.38419e-6 at 32768 on these architectures.
Wet/dry impulse alignment passes blocks 3, 127, 512 and 2048 at 44.1, 48, 96 and
192 kHz. HPSS separation and cepstral correction remain approximate on mixed
recordings; synthetic fixture success does not certify commercial transparency.
The active frequency range is not a brick-wall time-domain filter: hard onsets
can leak into the processing band. Momentary LUFS and sample peak are not
integrated loudness, true peak or a full EBU R128 certification.

A direct Core Audio probe creates the installed final AU, configures matching
input/output formats, initializes it, and reads `kAudioUnitProperty_Latency`.
Both architectures report exactly 16445 samples at 44.1, 48, 96 and 192 kHz:

```sh
clang++ -std=c++20 -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
  Tests/AuLatencyProbe.cpp -framework AudioToolbox -o /tmp/AuraAuLatencyProbe
arch -arm64 /tmp/AuraAuLatencyProbe
arch -x86_64 /tmp/AuraAuLatencyProbe
```

pluginval passes sample-rate/block-size processing, editing during processing,
automation, state, buses and parameter fuzzing. Its AU wrapper emits the existing
non-fatal current-program -1 warning; the run completes with SUCCESS. Its
pre-preparation plugin-info latency reads zero; the initialized Core Audio probe
above verifies the actual prepared property.

The UI preview confirms Amount-driven saturation on the main wave, particles and
formant envelope at 0% and 100%, the Settings panel's factors/quality and native
quality-disabled state, Reduced motion, Reset to Default, Done, and Vocal Magic
recall. The display targets 60 Hz; actual frame pacing was not measured. See
`UI_REVIEW.md` and `Validation/ui-observations.txt` for review scope.

The final-image AU 1.5.0 is installed at
`~/Library/Audio/Plug-Ins/Components/Aura.component` and its executable matches the
image. The prior AU 1.4.0 is preserved intact in `work/aura-installed-v1.4/`.
The existing installed VST3 remains 1.2.0; the new 1.5.0 VST3 is packaged without
further FL Studio work. No existing music project was edited.

Both bundles and the DMG are ad-hoc signed. No Developer ID identity is available
and no Apple notarization was performed. Current evidence is in `Validation/`
and included in the source archive. This is a validated development release;
recorded-material listening, Logic session tests, minimum-size interaction and
commercial distribution signing remain release qualification work. The build
retains the existing nested Make jobserver warning; project-source compilation
has no remaining warnings.
