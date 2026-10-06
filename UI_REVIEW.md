# Aura 2.0 editor review

Reviewed the native silent OpenGL preview on October 6, 2026. The steel frame,
main spectral display, five control pods, scale keyboard and preset manager
remain. The header power button, lower spectrum, metering and output Mute/Solo
buttons are removed. Three lower panels now contain Mid/Side, Delta audition,
and output gain/Mix.

At 1180 × 810 the labels, dropdowns, component knobs and Delta button fit.
Mid and Side knobs disable in Stereo and for an unselected component. Mono
also disables Side. The rack reports a pending basis change; the footer and
settings show active FFT resolution and latency.

Native accessibility selected Mid + Side, set Mid 74% and Side 56%, and toggled
Delta On. The active button and DELTA ACTIVE status were visible. Settings
selected Real-time and settled at 4096 FFT / 8253 samples, with the corresponding
footer. Screenshots in docs/ show these actual states using the silent synthetic
preview, not a fixed illustrated waveform.

The editor retains its 1000 × 740 minimum. This release's native captures are
at its default size; no full screen-reader or frame-pacing certification is
claimed. The UI targets 60 Hz and supports Reduced motion. All visual data
comes from audio processing and the cepstral envelope; actual frame rate depends
on the host and graphics environment.
