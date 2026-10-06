# Aura 1.0.0 validation

Tested on October 6, 2026, on an Apple silicon Mac running macOS 26.3.1,
using AppleClang 17 and the macOS 26.1 SDK. Both bundled Mach-O executables
contain arm64 and x86_64 slices and declare a macOS 11.0 minimum.
Running on macOS 11 hardware was not tested.

| Check | Result |
|---|---|
| Release CMake build of AU and VST3 | Passed |
| DSP / processor CTest suite | Passed, zero failures |
| `auval -v aufx Aura AS01`, native arm64 | AU VALIDATION SUCCEEDED |
| Same AU validation via `arch -x86_64` / Rosetta | AU VALIDATION SUCCEEDED |
| pluginval 1.0.4, VST3, strictness 10, GUI tests enabled | SUCCESS |
| pluginval 1.0.4, AU, strictness 10, GUI tests enabled | SUCCESS |
| Live OpenGL editor in silent developer harness | Spectrum, custom-note toggle, Amount and unit labels verified |
| Final mounted DMG, both bundles: strict deep signature verification | Passed |
| Final mounted DMG, both executables: Universal 2 verification | Passed |
| Compressed DMG checksum (`hdiutil verify`) | VALID |
| Direct Logic Pro insertion / playback / project recall | Incomplete |
| Direct FL Studio scan / insertion / playback / project recall | Incomplete |

The automated signal checks cover Hann overlap-add reconstruction at 44.1, 48,
96, and 192 kHz; an impulse at exactly 2048 samples; stereo isolation; exact
delayed dry at Mix 0; an empty custom mask; crossed range boundaries; out-of-range
audio; A440 snapping; logarithmic half-Amount pitch; APVTS recall; invalid-state
handling; bounded FIFO overflow; and finite output during bypass switching.
Amount 0 reconstruction had a maximum absolute error of 1.78814e-7.
A 430 Hz sine snapped to 439.999 Hz with output RMS about 0.257 from a 0.4-peak input.

The regression harness counted zero C++ `new` / `new[]` calls during warmed-up
`processBlock` and bypass callbacks. Source review confirms fixed storage,
cached lock-free atomic parameter reads, no ValueTree access or mutexes in the
callback, and a pre-created macOS vDSP FFT setup. This allocation test does not
interpose every C allocator. Timing figures in the test log describe this machine
and workload, rather than a performance guarantee for all supported Macs.

pluginval exercised sample rates 44.1/48/96 kHz, block sizes 64–1024, automation,
state, editor lifecycle, bus changes, and parameter fuzzing. The AU validator
also exercised channel formats, parameter scheduling, and invalid render sizes.
These results establish API and signal behavior; direct DAW playback remains a
separate check.

Logic Pro 12.3 initially opened an empty test project. After restarting to discover
the new AU, its UI stopped at a blank `SubscriptionWorkflowWindow`. FL Studio
2026 (26.1.3.5336) did not complete startup; its UI connection timed out, and a
process sample showed the main thread blocked in a read during launch. The
alternate 2025 installation and dedicated Plugin Manager also could not be
controlled. Aura was not inserted into either DAW. No subscription or licensing
purchase was attempted. Complete the direct host checks before calling this a
fully DAW-qualified production release.

For validation, both bundles were installed in this user's
`~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3` folders.
The empty Logic test project is under the workspace's `work/` folder. Logic's
startup workflow created a missing default library and cloned existing sounds
using its built-in initialization. No existing project was used for testing.

The packaging script stages outside cloud-backed output directories, omits
source extended attributes, clears image-side bundle attributes, and verifies
signatures again from the final read-only image. This addresses Finder metadata
being restored during copying. The image contains both bundles, matching system
installation-folder symlinks, a PNG background with directions, and text/SVG guides.
The image is ad-hoc signed and has not been notarized.

Detailed evidence is in `Validation/`. The adjacent `.dmg.sha256` file records
the final compressed image hash.
