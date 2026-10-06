# Aura changes

## 1.6.0

- Added Real-time processing with a native 4096-point FFT and 1024-sample hops,
  alongside the existing Studio 8192/2048 engine. Reported delay is reduced to
  8253 samples in Real-time versus 16445 in Studio, including FIR alignment.
- Added an accessible Real-time mode toggle in Processing Settings, requested
  resolution descriptions, active FFT/latency status, and a dynamic editor footer.
  Studio remains the default for new instances, factory programs, and older states.
- Preallocated ten paths across the two resolution banks. Each bank offers native,
  2x Standard/High, and 4x Standard/High processing with its own aligned dry delay.
  Oversampling retains the analysis duration and frequency resolution within a bank.
- Added a resolution handover that primes the incoming path, fades output out over
  25 ms, reports new host latency through a 20 ms message-thread timer, commits the
  new bank after acknowledgement, and fades back in over 25 ms. The switch includes
  a brief silent hold. Factor/quality changes within one bank retain the 50 ms crossfade.
- Appended `realTimeMode` with AU version hint 6 and a false default. Factory
  programs now recall all 21 parameters; existing IDs and legacy oversampling
  migration remain compatible. Resolution is a nonautomatable saved processing
  preference intended to be selected before playback or a bounce; live UI changes
  use the handover, while factor/quality choices remain automatable.
- Added FFT-size/bin-count metadata to visualization frames and corrected the
  main/rack spectrum mappings for the active resolution. The final-output analyzer
  retains its 8192-point analysis.
- Updated the product overview and linked the interface and processing settings
  images for the GitHub page. Release asset links use v1.6.0.
- Retained ad-hoc macOS signatures, unsigned Windows executables, and the existing
  host/listening qualification limits. Earlier validation evidence remains tied to
  its recorded version; see `VALIDATION.md` for current checks.

## 1.5.0

- Coupled the main spectrum's color saturation to SWEETENING AMOUNT, with
  smoothed transitions for its wave, particle trails, and formant curve. The lower
  POST spectrum keeps its fixed palette.
- Replaced the oversampling toggle/menu with a Processing Settings panel offering
  1x, 2x, and 4x, plus Standard or High resampling filter quality. Quality is
  disabled at native rate; Reduced motion, Reset to Default, Done, and pending
  processing status share the panel.
- Added a 2x engine with a 16384-point FFT and 4096-sample hops. Native and both
  resampled factors keep the 8192-host-sample analysis window and fixed
  16445-sample reported latency.
- Preallocated five native/resampled paths and changed scheduling to process only
  the active path plus an incoming path during transitions. Incoming paths prime
  for latency plus one analysis window before a 50 ms crossfade; inactive paths
  no longer run continuously. Separate frame-grid offsets spread transition work.
- Retained the legacy `oversampling` Boolean and its AU version hint. Appended
  `oversamplingMode` and `processingQuality` choices with version hint 5; old
  Boolean states migrate to 1x or 4x and missing quality defaults to High.
- Extended the five factory programs to recall all 20 parameters, including the
  new mode and quality choices. Processing dropdowns expose named choices to
  assistive clients.
- Retained Universal 2 AU/VST3 packaging and ad-hoc signatures. Developer ID
  signing, notarization, commercial listening quality, and universal host timing
  qualification remain outside the verified claims. See version-specific
  `VALIDATION.md` evidence for signal, timing, and AU checks.

## 1.4.0

- Increased the native STFT to 8192 points with 2048-sample hops, retaining the
  periodic Hann window and 75% overlap. Nine-frame HPSS now adds 8192 samples
  of lookahead; fixed reported host latency is 16445 samples including FIR alignment.
- Added actual x4 FIR oversampling with a 32768-point internal FFT at four times
  the host rate, preserving the native analysis window and frequency resolution.
  Both paths stay warm for a 50 ms mode crossfade, with their frame grids offset
  by half a host hop to spread processing work.
- Added fast/slow transient envelope detection to boost broad percussive bins
  alongside the median HPSS masks. Preserved percussion bypasses phase and formant
  correction, then joins the processed complex bins before a shared IFFT/OLA.
- Added five complete factory programs: Default, Vocal Magic, 808 Tuner,
  Lush Pad Sweetener, and Drum Transient Preserver. Program recall includes all
  18 parameters; the header marks edited settings and saved state retains program identity.
- Moved ROOT KEY and SCALE TYPE into side-by-side dropdowns inside SCALES.
  Added the functional 1x / x4 header control and Reset to Default in Settings.
  Root, scale, and factory dropdowns expose named choices to assistive clients.
- Retained the 60 Hz display target, logarithmic rainbow spectrum, 4096-particle
  pool, cepstral curve, transient flashes, range handles, and telemetry LED rings.
  Spectrum targets now hold between analysis frames and release on silence or
  after callbacks stop for the longer of 250 ms or three analysis hops.
- Moved large engine and FIFO storage to construction-time heap ownership with
  fixed processing capacity. Added an incremental frequency median, cached pitch
  targets, silence handling, and reduced redundant envelope/transform work.
  Even-spectrum cepstral extraction uses two real forward FFTs, with the first
  scaled as a normalized inverse transform; macOS builds require the vDSP backend.
- Appended `oversampling` with AU version hint 4 and an off default for older
  states; prior parameter IDs, order, and defaults remain compatible.
- Extended signal and processor checks for the larger window, factory recall,
  x4 alignment, mode automation, median equivalence, and callback allocations.
  Current validation results and qualification limits are recorded in `VALIDATION.md`.
- Added ad-hoc signing of the DMG as well as both Universal 2 plugin bundles.
  Developer ID signing and notarization remain unavailable in this environment.
- Continued AU validation independently of FL Studio, whose work remains stopped
  at the user's request. Both AU and VST3 formats are built and packaged.

## 1.3.0

- Added an automatable 12-note TONIC selector, transposing presets and custom intervals.
- Rebuilt the editor around the supplied steel-rack reference: rainbow particle
  spectrum, glowing range handles, five metallic control pods and a lower rack.
- Added transient sensitivity/bypass, formant shape/tension, output gain, Mute,
  processed Solo and global Bypass. Existing parameter IDs and defaults remain compatible.
- Added actual stereo post-output FFT, momentary LUFS, RMS and a 400 ms sample-peak window.
- Added keyboard accessibility states and a Reduced motion display option.
- Added tonic/audio-target, timbre, output-routing, stereo analyzer, meter calibration,
  brief-peak retention and legacy-state checks. Fixed preset restoration after Custom.
- Stopped further FL Studio testing at the user's request; v1.3 host validation uses AU.

## 1.2.0

- Added centred nine-frame / 17-bin median HPSS with complementary soft masks.
- Added a separate original-phase percussive OLA delay, controlled by PUNCH
  (`transientPreserve`, 0–1).
- Added real-cepstral spectral-envelope extraction and reapplication, controlled
  by THROAT (`formantPreserve`, 0–1).
- Added input-envelope spline, preserved-transient flash and telemetry LED rings.
- Increased fixed host/dry/bypass latency from 2048 to 4096 samples for HPSS lookahead.
- Appended the new APVTS parameters without changing prior IDs or AU version hints;
  v1.0 states supply 100% preservation defaults.
- Added reconstruction, crossfade, envelope, telemetry, state, endpoint automation,
  sample-rate/block-size alignment and silence checks.

## 1.0.0

Initial AU/VST3 spectral snapping engine, scale keyboard, range controls, spectral
visualizer, Universal 2 build and automated DMG packaging.
