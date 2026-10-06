# Aura 2.0.0 validation

Tested October 6, 2026 on Apple silicon, macOS 26.3.1, AppleClang 17,
with a Release Universal 2 build targeting macOS 11.0+.

| Check | Result |
|---|---|
| Universal 2 AU/VST3 build and packaged bundles | Passed; arm64 and x86_64 slices |
| Native CTest regressions | Passed; zero failures, 134.14 s final run |
| Same regression executable under Rosetta | Passed; zero failures |
| Mid-only / Side-only isolation, both resolutions / five resampling profiles | Passed; unselected component error below 2.7e-8 |
| Independent Mid + Side reconstruction | Passed; error below 3.8e-8 |
| Delta = selected blended result minus aligned dry, before gain | Passed; error below 1.9e-8, including −6 dB gain and 60% global Mix |
| Both component blends 0% / Delta Mix 0% | Passed; null within float rounding / exact silence |
| Mono Side-only and host bypass while Delta is on | Passed; silent delta / unity aligned dry |
| Live LR/MS basis switches | Passed; bounded finite output, fade/prime/settle, unchanged host latency |
| Callback C++ heap allocations, including spatial/Delta and path transitions | Zero observed |
| v1 state migration and 25-parameter factory presets | Passed; Stereo/full component blends/Delta off |
| Existing FFT, HPSS, cepstrum, scale, tonic and output regressions | Passed |
| Packaged AU Apple validation, native and Rosetta | AU VALIDATION SUCCEEDED |
| Packaged AU pluginval 1.0.4, strictness 10 / GUI enabled | SUCCESS |
| DMG checksum and both payload signatures | Passed; ad-hoc signed |
| Native editor / accessible channel choices and component values | Observed; Mid + Side, Mid 74%, Side 56%, Delta On |
| Real-time settings / footer with Mid + Side | Observed; 4096 FFT / 8253 samples |

The native UI has no header power, lower spectrum, metering, or Mute/Solo
buttons. The main visualizer, output gain, Mix and global BYPASS remain.
Actual product and settings captures are in docs/.

M/S uses (L+R)/2 and (L−R)/2, with unity inverse reconstruction. Component
blends precede global Mix. Delta removes aligned dry before output gain and
resolution/spatial handover gain, so a fade cannot introduce negative dry
into audition. Bypass overrides Delta and gain. In mono there is no Side.

Changing between LR and MS input bases fades down over 25 ms, restarts only
the active preallocated path, primes it in silence for its latency plus one
analysis window, then fades up over 25 ms. This briefly interrupts output.
The channel preference is saved but nonautomatable; choose it before playback
or a bounce. Changes among Mid-only/Side-only/both smooth component blends.
Factor, quality and FFT transitions serialize with the basis handover.

Real-time uses 4096/1024 FFT/hops; Studio 8192/2048. Four HPSS lookahead frames
and 61 FIR alignment samples give host delays of 8253 and 16445 samples
(171.94 and 342.60 ms at 48 kHz). This is not zero-delay live monitoring.
Both FFT banks and all oversampling paths are preallocated.

The existing Studio 512-sample / 48 kHz timing exercise recorded callback
p99/max 4.72329/5.21658 ms natively and 7.21892/7.41496 ms under Rosetta,
against a 10.6667 ms budget. These figures do not certify every mode or host's
deadlines. Basis changes restart a live spectral engine; their CPU load and
brief silence should be considered before live use.

Windows release CI builds/tests and exercises setup/uninstall independently.
Release CI evidence accompanies the public installers. Direct DAW listening,
project recall and Windows driver/host performance are not certified by these
functional checks. macOS is ad-hoc signed and not notarized; Windows is unsigned.
Historical evidence is retained under Validation/. No additional FL Studio
work was performed.
