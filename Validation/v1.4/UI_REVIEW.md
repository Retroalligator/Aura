# Aura 1.4 editor review

Reviewed the native silent OpenGL preview on October 6, 2026 against the earlier
attached Aura steel-rack image. The separately named image_b90fe6.jpg and
watermarked_img_1704036837373526381.jpg were unavailable. This is a self-review,
not an independent certification or a pixel-difference benchmark.

The large logarithmic spectrum, five brushed-steel pods, larger central Amount
knob and three lower-rack panels follow the supplied arrangement. Machined rotary
faces, cyan/violet/gold indicators, dark beveled framing, rainbow particles and
an envelope overlay are present. Curves and particle density follow actual audio;
the illustrated reference waveform is not repeated as a fixed decoration.

The header exposes the factory manager, functional 1x/x4 processing toggle,
Power and Settings. Root Key and Scale Type sit side by side inside SCALES.
At 1180 × 810, titles, numeric readouts, mini-pots, piano and rack panels fit.
Secondary instrument labels are small. Minimum-size geometry remains 1000 × 740;
that size has not been directly captured for this release.

Native accessibility interaction changed Root Key from D to F and verified the
resulting F Major keyboard states and the edited-preset asterisk. Recalling Vocal
Magic restored A Natural Minor, Amount 74%, transient preservation 90%, formant
preservation 100%, 80 Hz–14.5 kHz, gain −1.5 dB and x4 on, without an asterisk.
The read/write choice interface accepts only existing named items and retains
JUCE popup/keyboard behavior. Processor tests independently verify all roots,
custom intervals, preset state recall and actual output pitch. No full screen-
reader or WCAG audit was performed.

The editor targets 60 Hz; actual display frame pacing was not profiled. Analysis
frames arrive at Fs/2048, so display targets now hold between frames and release
on actual silence or after callbacks stop for max(250 ms, three analysis hops).
Reduced motion disables particles and transient flashes while retaining curves
and meters. The Settings popup was opened and keyboard selection was attempted, but its
auxiliary menu state was not exposed reliably; the Reduced motion selection
was not independently confirmed.
The final preview screenshot shows 8192 FFT and the full 16445-sample delay
in the footer. Settings uses a vector gear rather than relying on a font glyph.

The Anti-AI Design Slop rubric is adapted to a desktop audio instrument; mobile
and feed criteria are not applicable. Self-review scores, 0–5:

| Dimension | Score | Reason |
|---|---:|---|
| Originality | 3 | Aura-specific feedback within the explicitly requested rack reference |
| Message clarity | 4 | Root, scale, Amount and preservation controls have explicit names |
| Hierarchy | 4 | Spectrum, central Amount, then processing pods and auxiliary rack |
| Brand specificity | 4 | Aura prism, steel surfaces and spectral colors recur |
| Readability | 3 | Main values are clear; dense secondary labels remain small |
| Buyer trust | 3 | Functional controls and metering; commercial qualification is limited |
| Intentionality | 4 | Particles, transient flashes and LED activity are driven by audio |
| Accessibility | 3 | Named writable choices and states, reduced-motion option; audit limited |

The rubric's public-use gate calls for originality ≥4 and every other dimension
≥3. This self-review is therefore **Revise for public marketing/commercial release**;
the requested development deliverable remains reviewable. No fictional metering,
unsupported AAX label or simulated oversampling claim is included. Listening tests,
minimum-size interaction and a functioning DAW playback review remain useful
release qualification work.
