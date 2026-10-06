# Aura 1.3.0 validation

Tested October 6, 2026 on Apple silicon, macOS 26.3.1, AppleClang 17,
macOS 26.1 SDK. AU and VST3 binaries contain arm64 and x86_64 slices and
specify macOS 11.0+. macOS 11 hardware was not exercised. Prior reports and
logs are retained in `Validation/v1.0/` and `Validation/v1.2/`.

| Check | Result |
|---|---|
| Release Universal 2 AU / VST3 build | Passed |
| CTest regression suite, native arm64 | Passed, zero failures |
| Signal / processor suite under x86_64 / Rosetta | Passed, zero failures |
| Native `auval`, installed component version 1.3.0 | AU VALIDATION SUCCEEDED |
| x86_64 / Rosetta `auval`, component version 1.3.0 | AU VALIDATION SUCCEEDED |
| pluginval 1.0.4, AU, strictness 10, GUI tests enabled | SUCCESS |
| Native silent OpenGL preview | Rendered spectrum, particles, formant curve, steel pods and rack observed |
| Mounted final DMG, both bundles: strict deep signature and architecture checks | Passed |
| Compressed DMG checksum | VALID |
| Direct DAW insertion / playback / project recall | Not verified for v1.3 |
| Further FL Studio / VST3 host validation | Stopped at the user's request |

The tonic suite checks all 12 roots, preset and custom interval transposition,
empty scales, APVTS recall, and actual processor output. With Major and a 365 Hz
input, C produces 349.230 Hz and D produces 369.994 Hz. Solo with Mix at zero
produces the D target while the ordinary dry blend remains at 365 Hz. New
parameters are appended using AU version hint 3, retaining previous parameter
IDs, order and hints. v1.0 state loading supplies C tonic, neutral gain/shape,
and v1.2 preservation defaults. Neutral float parameters are checked within
1e-6 to allow JUCE's normalized conversion rounding.

Additional tests verify Throat shape changes timbre with Amount zero, Tension
changes envelope detail, and Sensitivity changes broadband percussive routing.
The low/high sensitivity fixture reports preserved-energy measures of
0.0764914 and 0.928515. Output gain -6 dB, Mute, and global Bypass produce
expected aligned audio; post spectrum and metering follow those output controls.
The post FFT is calibrated for mono, includes right-only stereo, and retains
opposite-polarity stereo power. Brief sample peaks survive GUI frame intervals
and release after the 400 ms window.

The stereo 1 kHz reference at -23 dBFS peak measures approximately -22.9905,
-22.9933, -23.0106 and -23.0203 LUFS at 44.1, 48, 96 and 192 kHz, respectively.
RMS agrees with -26.0103 dBFS within 0.1 dB. Silence releases both loudness and
peak. This checks the implemented momentary meter; integrated loudness,
true peak and full EBU R128 certification are outside this implementation.

The retained HPSS/cepstral suite passes isolated-transient reconstruction,
PUNCH crossfades, envelope correction, telemetry, automation endpoints,
non-finite-input handling, stereo isolation, FIFO overflow/event retention,
and wet/dry alignment at blocks 3, 127, 512 and 2048 across four sample rates.
Isolated preserved-transient error is at most 5.96046e-8; Amount-zero OLA error
is at most 1.49012e-7. Latency remains exactly 4096 samples. The synthetic
vowel log-envelope error improves from 0.392769 to 0.0944115 without the
percussive split, and from 0.371236 to 0.342 through the combined HPSS path.
These fixtures do not establish listening quality on recorded vocals or drums.

The allocation harness records zero C++ new / new[] calls from the first
callback after reset through 1000 stereo processing/bypass cycles, including
new formant settings, gain, stereo FFT and metering. It does not interpose
every C or aligned allocator. The native timed fixture took about 0.90 seconds
for 12 seconds of audio on this computer; it is not a universal CPU guarantee.
Read-only source review found fixed callback arrays, cached lock-free atomics,
pre-created FFT setups, and no ValueTree, mutex, logging or host notification
inside the callback.

pluginval passed sample-rate/block-size changes, editor lifecycle and automation,
state, buses and parameter fuzzing. The AU wrapper emits a non-fatal current-
program -1 warning; the run finishes with SUCCESS. UI screenshots and the
accessibility tree show the new tonic selector and note states. Native automation
could not complete the tonic menu gestures reliably, so direct manual interaction
review is limited; processor/APVTS tonic behavior is covered by the signal suite.
The editor requests 60 Hz updates; measured display frame pacing was not profiled.
See `UI_REVIEW.md` for the visual review and its limits.

The final-image AU v1.3.0 is installed at
`~/Library/Audio/Plug-Ins/Components/Aura.component`. The previous AU v1.2.0
was copied to `work/aura-installed-v1.2/` before replacement. The installed
VST3 was left at v1.2.0 and no further FL Studio interaction was performed.
The v1.3 DMG still includes both built formats for the original project deliverable;
v1.3 VST3 host validation was not performed. Earlier direct Logic/FL limitations
are documented in the archived v1.2 report.

Distribution uses ad-hoc signing, without Apple notarization. The final read-only
image's architecture slices, signatures and compressed checksum were verified.
The adjacent `.dmg.sha256` records its released hash. Evidence is in `Validation/`
and is included in the source archive.
