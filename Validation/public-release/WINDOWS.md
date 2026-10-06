# Windows initial build qualification

2026-10-06: [successful Windows build](https://github.com/Retroalligator/Aura/actions/runs/37505536762),
source commit `5f3a0c0fda130927d5a24b69ea8850d59343d7ef`, Windows 2022 x64
GitHub-hosted runner, Visual Studio 2022 Release, pinned JUCE 8.0.15.

The VST3 and standalone compiled; CTest reported 1/1 passed, 0 failures.
The Inno Setup executable installed both payloads and the license; installed
executables matched the build hashes. Silent uninstall removed both payloads.
The downloaded portable ZIP contains VST3, standalone, licenses and source notice.
All downloaded artifact SHA-256 files matched their payloads.

## Real-time timing remains open

During live processing-mode changes at 512 samples / 48 kHz the test measured
19.8599 ms p99 and 20.0605 ms maximum callbacks against a 10.6667 ms deadline.
This exceeds the buffer budget on that runner. Functional regressions do not
assert a timing threshold, so their pass is not a real-time qualification.
Use 1x initially and check host CPU/load; larger buffers may be needed.
Windows DAW playback and GUI validation were not performed.

The tagged release workflow repeats build, regressions and install/uninstall,
and attaches that run's evidence to the release. These initial artifacts are:

```text
e25ca74abef5a0b4a395bb9e6c6bf901a1e60b38c18cd8820e68a61d085ab524  Aura-1.5.0-Windows-x64-Setup.exe
e411d1a8f400a577e326cda57f0f50caef0cdeaae0679bfd700fa57b41911b7e  Aura-1.5.0-Windows-x64.exe
b7ab6fb2b5fe2fb0ead40f27d459809b2bddaa938ffbeb083bcd76a121227ce0  Aura-1.5.0-Windows-x64.zip
```

The release contains independently rebuilt artifacts; use its SHA256SUMS.txt
for the hashes of the files actually published there.
