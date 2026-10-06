# Public-source macOS rebuild

2026-10-06, macOS 26.3.1. Built from published source commit
`5f3a0c0fda130927d5a24b69ea8850d59343d7ef`, Release, Universal 2,
JUCE 8.0.15 pinned to `91ad83ae34a81e0833b1a2b0866f54846370ae53`.

- Native CTest: 1/1 passed.
- Intel regression executable under Rosetta: 0 failures.
- Native and Rosetta Apple `auval`: AU VALIDATION SUCCEEDED on both.
- Official pluginval 1.0.4, AU strictness 10 with editor tests: SUCCESS.
- The tested AU was copied from the final licensed DMG; installed binary matched
  the image binary byte for byte. Previous installed AU was preserved locally.
- Packager checked arm64/x86_64 on both AU/VST3, strict ad-hoc signatures on
  the bundles and final DMG, and the disk-image checksum.

The release workflow independently rebuilds the tagged source on macOS and
Windows and publishes its test evidence after both platform jobs pass. Its
Windows check includes processor regressions and setup install/uninstall on a
clean runner. No Windows DAW, direct Logic playback, Developer ID notarization,
or Authenticode signing is claimed. The installers are development previews.
