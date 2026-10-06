# Exact tagged CI DMG validation

2026-10-06: built by [release CI run 37506590041](https://github.com/Retroalligator/Aura/actions/runs/37506590041) from tag v1.5.0, commit `2de48c0dd79242017394f82333364e431e3bf432`.

```text
891fc046b12ed30ff483176db2290e8cfc0b864f26df33ac35aec790bfc91f91  Aura-1.5.0-Universal.dmg
```

The macOS build, processor regression suite and packaging completed successfully
on the Intel runner. Its first AU validation could not discover the newly copied
per-user component and therefore failed; no binaries were published by that run.
A follow-up workflow reuses these artifacts, verifies their original run/commit
and build results, refreshes registration using a system-folder installation on
a clean disposable runner, and gates publication on validation success.

The exact downloaded DMG was also verified locally on macOS 26.3.1: its SHA-256
matched the CI artifact's checksum; the installed AU binary matched the image
byte for byte; native Apple Silicon and Rosetta auval both reported
AU VALIDATION SUCCEEDED. Official pluginval 1.0.4, strictness 10 with GUI tests,
reported SUCCESS. Attached logs correspond to this exact image.

Previous installed AU was backed up. Installed VST3 and FL Studio were untouched.
macOS signing remains ad-hoc; notarization and direct Logic playback were not
performed. Windows timing limitations are documented separately.
