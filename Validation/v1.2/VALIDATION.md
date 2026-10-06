# Aura 1.2.0 validation

Tested on October 6, 2026, on an Apple silicon Mac running macOS 26.3.1,
using AppleClang 17 and the macOS 26.1 SDK. Both released Mach-O executables
contain arm64 and x86_64 slices and target macOS 11.0+. Execution on macOS 11
hardware was not tested. This report applies to v1.2; v1.0 evidence is preserved
in `Validation/v1.0/`.

| Check | Result |
|---|---|
| Release CMake build, AU and VST3, Universal 2 | Passed |
| CTest DSP / processor suite, native arm64 | Passed, zero failures |
| Same signal suite under x86_64 / Rosetta | Passed, zero failures |
| `auval -v aufx Aura AS01`, native arm64, component v1.2.0 | AU VALIDATION SUCCEEDED |
| Same AU validation under x86_64 / Rosetta | AU VALIDATION SUCCEEDED |
| pluginval 1.0.4, AU v1.2.0, strictness 10, GUI tests enabled | SUCCESS |
| pluginval 1.0.4, VST3 v1.2.0, strictness 10, GUI tests enabled | SUCCESS |
| Live OpenGL silent preview | New controls, percent formatting, envelope spline and activity rings verified |
| Final mounted DMG, both bundles: strict deep signature verification | Passed |
| Final mounted DMG, both executables: Universal 2 verification | Passed |
| Compressed DMG checksum (`hdiutil verify`) | VALID |
| Direct Logic Pro insertion / playback / project recall | Incomplete: blank subscription workflow |
| Direct FL Studio scan / insertion / playback / project recall | Incomplete: scanner / UI automation blocks |

The new signal checks cover temporal/frequency median classification and soft-mask
silence/ties; isolated transient reconstruction with full snapping at 44.1, 48,
96 and 192 kHz; PUNCH crossfades; cepstral full/half envelope correction;
formant-shaped harmonic input through the complete HPSS/snapping engine;
PUNCH and THROAT telemetry; input-envelope transport; preservation endpoint
automation; silence/non-finite input; new parameter recall; and v1.0 state loading.

At PUNCH 100%, isolated impulses at several sample offsets reconstructed with a
maximum error of 5.96046e-8. Amount 0 reconstruction peaked at 1.49012e-7.
The synthetic cepstral projection test had full/half log-envelope errors below
7.2e-7. On the synthetic vowel, THROAT reduced log-envelope RMS error from
0.392769 to 0.0944115 with PUNCH 0 (about 76%); the complete split/summed path
improved from 0.371236 to 0.342. These are deterministic fixtures, not listening
scores on recorded lead vocals or drum loops.

A 430 Hz sine still snapped to 439.999 Hz with both preservation controls at
100%; output RMS was 0.287683 from a 0.4-peak input. The aligned impulse delay
is exactly 4096 samples. Wet/dry alignment also passed at block sizes 3, 127,
512 and 2048 across all four sample rates. Empty custom masks, crossed bounds,
out-of-range fundamentals, logarithmic Amount interpolation, stereo isolation,
invalid state, and FIFO overflow retained their regression coverage. FIFO draining
now keeps brief activity peaks while selecting the latest spectral curve.

The allocation harness counted zero C++ `new` / `new[]` calls from the first
callback after reset through 1000 stereo process/bypass cycles, including HPSS,
cepstral and phase-synthesis paths. Source review shows fixed arrays, pre-created
FFT setups, cached lock-free APVTS atomics, and no ValueTree, mutex, logging,
message-thread notification or host latency update in the callback. This test
does not interpose every C allocator or aligned allocator. The native timed
fixture took about 0.86 seconds for 12 seconds of audio on this Mac; it is not
a performance guarantee for every signal or supported computer.

pluginval exercised multiple sample rates, block sizes, automation, state,
editor lifecycle, bus changes and parameter fuzzing. Its AU run emitted a
non-fatal current-program -1 warning for the wrapper's preset representation;
the run completed with SUCCESS. Both validator logs identify Aura v1.2.0.
The visual preview confirmed PUNCH/THROAT values at 0 and 100 percent and
activity-ring decay; transient detection and event retention are covered by the
signal/FIFO tests. The transient flash is driven by those preserved-hit events.

Both final-image bundles were installed in this user's
`~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3` folders.
The previous v1.0 test installation was retained under
`work/aura-installed-v1.0/` for recovery. Logic Pro 12.3 rescanned Audio Units but
again stopped at a blank `SubscriptionWorkflowWindow`. FL Studio 2026 reached
its empty project and mixer this time. Opening Plugin Manager produced a UI
connection timeout; targeting its mixer effect slot returned noWindowsAvailable,
and the alternate plugin picker also timed out. Aura was not inserted into
FL Studio or Logic, so direct playback and saved-project recall remain unverified.
No paid subscription or license agreement was accepted, and no existing music
project was edited. The earlier empty Logic test project remains under `work/`.

The image is ad-hoc signed and not notarized. Packaging verified signatures and
both architecture slices from the final read-only image, plus the compressed
image checksum. The adjacent `.dmg.sha256` records the released image hash.
Evidence is in `Validation/`; the source archive includes these logs.

Median HPSS estimates the percussive component; it cannot promise that every
attack in arbitrary overlapping material is wholly classified into that branch.
Cepstral preservation applies the complete input/new envelope ratio to the
sweetened branch within the active range, with a frame-relative log floor and
finite-arithmetic guards. Sparse spectra and range boundaries limit exact
subsequent envelope equality. See `README.md` for implementation details.
Commercial transparency on recorded material and direct DAW qualification have
not been established by these automated checks.
