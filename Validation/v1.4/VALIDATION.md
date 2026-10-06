# Aura 1.4.0 validation

Tested October 6, 2026 on Apple silicon, macOS 26.3.1 (25D771280a), AppleClang 17,
macOS 26.1 SDK and CMake 4.3.4. Both plugin binaries contain arm64 and x86_64
slices and target macOS 11.0+. macOS 11 hardware was not exercised. Earlier reports
and logs remain in `Validation/v1.0/`, `v1.2/` and `v1.3/`.

| Check | Result |
|---|---|
| Release Universal 2 AU / VST3 build | Passed |
| CTest regression suite, native arm64 | Passed, zero failures |
| Signal / processor suite, arm64 and x86_64 / Rosetta | Passed, zero failures on both |
| Native `auval`, installed final-image component 1.4.0 | AU VALIDATION SUCCEEDED |
| Rosetta `auval`, installed final-image component 1.4.0 | AU VALIDATION SUCCEEDED |
| pluginval 1.0.4, AU, strictness 10, GUI tests enabled | SUCCESS |
| Direct prepared-AU latency property, both architectures / four rates | Exactly 16445 host samples |
| Silent native OpenGL preview / root and factory selection | Observed and exercised |
| Final DMG and both contained bundles: strict signatures | Passed, ad-hoc signatures |
| Final-image AU / VST3 architecture and version checks | Both slices, 1.4.0 |
| Compressed DMG checksum | VALID; adjacent SHA-256 provided |
| Direct Logic insertion / playback / project recall | Not verified for v1.4 |
| Further FL Studio / VST3 host validation | Stopped at the user's request |

The native STFT is 8192 points with Hann analysis/synthesis and 2048-sample hops.
Four centred HPSS lookahead frames add 8192 samples: spectral-engine delay is
16384 samples. FIR alignment adds 61, producing fixed host latency **16445 samples**
(342.60 ms at 48 kHz). True x4 processing uses a 32768-point FFT and 8192-sample
hop at four times the rate, preserving physical analysis duration and frequency
resolution. Native and x4 engines remain warm, with identical stereo frame grids
and a half-hop offset between processing modes to spread transform work.

The direct Core Audio probe creates the installed AU, sets matching input/output
formats, initializes it and reads `kAudioUnitProperty_Latency`. Both architectures
report 16445 samples at 44.1, 48, 96 and 192 kHz. Reproduce with the standalone
`Tests/AuLatencyProbe.cpp` (requires the installed Aura AU):

```sh
clang++ -std=c++20 -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
  Tests/AuLatencyProbe.cpp -framework AudioToolbox -o /tmp/AuraAuLatencyProbe
arch -arm64 /tmp/AuraAuLatencyProbe
arch -x86_64 /tmp/AuraAuLatencyProbe
```

Signal checks cover startup and hop-boundary impulses, amount-zero reconstruction,
HPSS crossfades, phase-locked snapping, transient sensitivity, formant shape and
tension, cepstral correction, silence/non-finite handling, FIFO overflow and event
retention. Native amount-zero reconstruction error is at most 1.49012e-7; Rosetta
is at most 1.78814e-7. Isolated preserved native impulses have at most 5.96046e-8
error. The offset-grid reconstruction/reset fixture passes at 5.96046e-8 native
and 1.19209e-7 Rosetta. These are synthetic fixtures, not proof that every attack
in a mixed recording is classified as percussion.

The optimized real-only cepstrum matches the original normalized complex-IFFT /
forward-FFT reference within 4.76837e-7 at 8192 points and 2.38419e-6 at 32768
points across three lifter tensions. The combined HPSS synthetic-vowel envelope
error improves from 0.0890994 to 0.0568922 with preservation; the unsplit fixture
improves from 0.128919 to 0.0822411. Correction replaces the broad envelope within
the active band; sparse spectra and band boundaries can prevent an exact subsequent
match. No listening-quality or commercial-transparency certification is claimed.

A 430 Hz tone snaps to 439.999 Hz when A is enabled. A 365 Hz input with Major
moves to 349.230 Hz in C and 369.994 Hz in D. All twelve roots, preset/custom
interval rotation, empty scales, Solo with Mix zero, and state recall pass.
All five factory programs apply and serialize their complete 18-parameter values
and program identity. Edited values remain edited on recall. Oversampling is
appended with AU version hint 4; older IDs/order/hints/defaults remain compatible,
and old states default oversampling off. Factory sounds are designed starting
points; real vocal, drum and pad listening was not performed.

The x4 1 kHz reconstruction error is 0.000188288 native / 0.000188303 Rosetta;
opposite-polarity stereo remains aligned. Native/x4 live crossfade error is
0.000326244 / 0.000326230, without a latency change. The 1537-sample fixture
exercises processing beyond the fixed 1024-sample scratch chunk. Wet/dry alignment
passes blocks 3, 127, 512 and 2048 at four sample rates. Original-phase percussion
in x4 mode passes through the resampler filters, so it is not bit-identical to
unfiltered input. Mix zero and settled host/global bypass deliver aligned dry.

The processing boundary is not a brick-wall time-domain filter. With preservation
disabled, a hard 430 Hz onset leaks spectral energy into the active band above
1 kHz. Native / Rosetta startup maximum errors are 0.00795011 / 0.0100268 on a
0.4-peak input, followed by steady-state errors below 6.4e-7. The onset test explicitly
bounds this to 3% of input peak; it does not assert sample identity for attacks
outside the range. A first 0.01-full-scale bound failed narrowly under Rosetta,
so the criterion now expresses the intended relative onset bound. This limitation
remains visible rather than being counted as exact out-of-range preservation.

Output gain, Mute, processed Solo and global Bypass checks pass, with POST FFT and
meters following the final PCM. The calibrated stereo analyzer includes right-only
and opposite-polarity content. The -23 dBFS-peak stereo 1 kHz reference measures
-22.9905, -22.9933, -23.0106 and -23.0203 momentary LUFS at 44.1, 48, 96 and
192 kHz. RMS agrees with -26.0103 dBFS within 0.1 dB; sample peak holds brief
impulses over 400 ms and releases. This is momentary loudness and sample peak,
not integrated loudness, true peak or full EBU R128 certification.

The allocation harness reports zero C++ new/new[] calls from the first callback
after reset through 1000 stereo processing callbacks plus 125 bypass callbacks,
including live x4 switches, preservation controls, output FFT and metering. It
does not interpose every C or aligned allocator. The macOS build explicitly enables
JUCE's vDSP backend; disabling it fails a compile-time check because the fallback
real FFT can allocate scratch at the x4 size. Storage is allocated at construction;
callback code reads cached lock-free atomics and uses fixed arrays without ValueTree,
mutex, logging or host notification calls.

The final 12-second stereo timed fixture took 1.84163 seconds native and 2.89560
seconds under Rosetta. For 512 samples at 48 kHz, callback p99 / maximum were
4.28117 / 5.14171 ms native and 7.12379 / 7.72037 ms Rosetta, below the 10.6667 ms
budget in these runs. This fixture includes automation and bypass while both modes
run continuously. It is an offline wall-clock sample on this computer, not proof
of deadline safety at smaller buffers, higher rates, under OS preemption or in a
loaded DAW project. Further optimization or larger host buffers may be necessary.

pluginval passes rates/block sizes, processing while editing, automation, state,
buses and parameter fuzzing. The AU host wrapper emits the non-fatal current-program
-1 warning, as in earlier releases. Its pre-preparation plugin-info latency reads
zero; the direct initialized-AU property probe above confirms the actual prepared
latency. The run completes with SUCCESS. The build also emits the existing nested
Make jobserver warning; no project-source compiler warnings remain.

Native UI interaction verified Root Key transposition and full Vocal Magic recall.
The final screenshot shows 8192 FFT and 16445 samples, the header controls, steel
pods, rainbow spectrum, cepstral curve, post analyzer and rack. The 60 Hz target
is not a measured frame-pacing result. The Reduced motion option is implemented,
but its auxiliary popup selection was not reliably confirmed through automation.
See `UI_REVIEW.md` and `Validation/ui-observations.txt` for review limits.

The final-image AU 1.4.0 is installed at
`~/Library/Audio/Plug-Ins/Components/Aura.component`. The prior AU 1.3.0 was moved
intact to `work/aura-installed-v1.3/` before replacement. The installed VST3 remains
1.2.0; the new 1.4.0 VST3 is included in the DMG without further FL Studio work.
No existing music project was edited. Direct host playback remains unverified.

Both plugin bundles and the DMG are ad-hoc signed. No Developer ID identity is
available and no Apple notarization was performed. Universal slices, final-image
bundle versions/signatures, DMG checksum and the adjacent SHA-256 were verified.
Current evidence is in `Validation/` and included in the source archive. This is
a validated development release; recorded-material listening, DAW session testing,
minimum-size interaction and normal commercial distribution signing remain release
qualification work.
