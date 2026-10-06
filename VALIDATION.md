# Aura 1.6.0 validation

Tested October 6, 2026 on Apple silicon, macOS 26.3.1, AppleClang 17,
with a Release Universal 2 build targeting macOS 11.0+.

| Check | Result |
|---|---|
| Universal 2 AU / VST3 build | Passed; arm64 and x86_64 slices |
| Native CTest regression suite | Passed, zero failures |
| Same regression executable under Rosetta | Passed, zero failures |
| Ten prepared profiles / four rates / non-hop blocks | Passed; wet, blend, dry and host bypass alignment |
| Live resolution round trips / five oversampling profiles | Passed for wet, dry and host bypass |
| Synchronous host reset during latency notification | Passed; committed bank re-primes before fading in |
| Callback C++ allocations, incoming-path restarts and live switches | Zero observed |
| Native and Rosetta Apple AU validation, final local DMG AU | AU VALIDATION SUCCEEDED |
| pluginval 1.0.4 AU / strictness 10 / GUI enabled | SUCCESS |
| Prepared AU latency property, both architectures / four rates | Studio 16445; Real-time 8253 samples |
| Resolution preference AU metadata | Non-automatable; readable and writable |
| 4096 cepstrum / FIFO frequency and amplitude calibration | Passed; 2049 analysis bins; independent 8192-point POST |
| State migration and all 21-parameter factory programs | Passed; older states and factories select Studio |
| Native settings and footer | Observed: active Real-time / 4096 FFT / 8253 samples |
| DMG and both payload signatures / image checksum | Passed, ad-hoc signatures |

Real-time selects a 4096-point native FFT with 1024-sample hops, 75% overlap,
and the same HPSS and cepstral algorithms. Studio retains 8192/2048.
Four HPSS lookahead frames double each native FFT delay. FIR alignment adds
61 host samples, giving 8253 or 16445 samples. At 48 kHz these are 171.94 ms
and 342.60 ms. The mode reduces spectral buffering; it does not provide
zero-delay live monitoring.

At 1x/2x/4x the internal FFT sizes are 4096/8192/16384 for Real-time or
8192/16384/32768 for Studio. All ten paths are constructed before callbacks.
Oversampling and quality changes retain bank latency and crossfade over 50 ms.
Resolution changes prime the target, fade the entire output down over 25 ms,
issue the host latency notification on the processor's 20 ms message-thread
timer, commit the new bank, and fade up over 25 ms. A host reset during the
notification clears histories, so the new bank first re-primes in silence.
The resolution preference is saved but non-automatable. Choose it before
playback or offline bounce; changing it during playback briefly interrupts output.
The callback never allocates C++ storage or calls the host latency notifier.
The message-thread notification uses JUCE's callback lock.

Impulse checks preserve exact dry and host-bypass timing and verify the wet
FIR response's centre, DC gain, stereo polarity, and measured wet/dry blend.
The spectral Nyquist crop and interpolation filter can produce extended small
ringing; oversampled early tails more than 256 samples from the centre are
bounded below 0.05% of a unit impulse (about -66 dBFS), rather than assumed
to have finite support.

The unchanged Studio timing exercise recorded 512-sample callback p99/max
4.61833/8.05637 ms natively and 9.02579/25.954 ms under Rosetta while background
build/packaging work was active, against a 10.6667 ms block budget at 48 kHz.
Functional success and the Real-time name do not certify every callback meets
its deadline. Windows release CI supplies separate build, regression and
installer smoke-test evidence. DAW playback/project recall, Windows DAW
hosting, and listening comparisons are not newly certified here.

macOS payloads use ad-hoc signatures and are not notarized; Windows installers
are unsigned. Earlier reports remain in Validation/v1.0, v1.2, v1.3, v1.4,
v1.5, published-image and public-release. No additional FL Studio validation
was performed, following the user's instruction.
