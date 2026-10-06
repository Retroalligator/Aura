#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${AURA_BUILD_DIR:-$ROOT/build-release}"
DIST="${AURA_DIST_DIR:-$ROOT/dist}"
JOBS="${AURA_JOBS:-4}"
if [[ "$(uname -s)" != Darwin ]]; then echo "Packaging requires macOS." >&2; exit 1; fi
for tool in cmake codesign hdiutil lipo; do command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 1; }; done
if [[ "${AURA_PACKAGE_ONLY:-0}" != 1 ]]; then
    # --fresh clears CMake configuration without deleting arbitrary user paths.
    cmake --fresh -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
        '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DAURA_BUILD_TESTS=ON
    cmake --build "$BUILD" --config Release --clean-first --parallel "$JOBS"
    ctest --test-dir "$BUILD" -C Release --output-on-failure
fi
mkdir -p "$DIST"
# Stage outside cloud-backed source/output folders, which can restore Finder
# xattrs after signing an AU bundle.
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/aura-stage.XXXXXX")"
MOUNT=""
cleanup() {
    if [[ -n "$MOUNT" ]]; then hdiutil detach "$MOUNT" -quiet || true; fi
    rm -rf "$STAGE"
}
trap cleanup EXIT
ARTEFACTS="$BUILD/Aura_artefacts/Release"
for format in AU VST3; do
    if [[ "$format" == AU ]]; then bundle="Aura.component"; else bundle="Aura.vst3"; fi
    ditto --noextattr --norsrc "$ARTEFACTS/$format/$bundle" "$STAGE/$bundle"
    # Finder metadata/resource forks prevent strict bundle signing.
    mkdir -p "$STAGE/$bundle/Contents/Resources/Licenses"
    cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/$bundle/Contents/Resources/Licenses/"
    cp -R "$ROOT/ThirdParty" "$STAGE/$bundle/Contents/Resources/Licenses/"
    /usr/bin/xattr -cr "$STAGE/$bundle"
    binary="$STAGE/$bundle/Contents/MacOS/Aura"
    lipo "$binary" -verify_arch arm64 x86_64
    codesign --force --deep --sign - "$STAGE/$bundle"
    codesign --verify --deep --strict "$STAGE/$bundle"
done
ln -s /Library/Audio/Plug-Ins/VST3 "$STAGE/Install VST3 Here"
ln -s /Library/Audio/Plug-Ins/Components "$STAGE/Install AU Here"
cat > "$STAGE/INSTALL.txt" <<'INSTRUCTIONS'
AURA — SPECTRAL HARMONY

Free software under GNU AGPL version 3. See LICENSE and THIRD_PARTY_NOTICES.md.
Project and complete corresponding source, including the pinned JUCE dependency:
https://github.com/Retroalligator/Aura/releases/tag/v2.0.0

Drag Aura.vst3 to “Install VST3 Here” for VST3 hosts.
Drag Aura.component to “Install AU Here” for Logic Pro and AU hosts.
macOS may request an administrator password for these system folders.

For a per-user installation, copy to ~/Library/Audio/Plug-Ins/VST3 or
~/Library/Audio/Plug-Ins/Components (create the folder if needed).
Restart the host, then rescan plugins. Aura appears under AudioStudio.

This development build is ad-hoc signed, not notarized. For a downloaded
copy, use Finder’s Open/security approval for this trusted build as needed.
Do not disable system-wide security settings.

Choose Root Key in SCALES to transpose presets or custom intervals through all 12 notes.
Toggle the keyboard to define a custom scale; C is the default for older states.
The header preset menu recalls Default, Vocal Magic, 808 Tuner,
Lush Pad Sweetener, or Drum Transient Preserver. An asterisk marks edited settings.
Amount sets the pitch pull; Mix blends latency-aligned dry and wet audio.
TRANSIENTS PRESERVE protects detected percussion; FORMANTS PRESERVE restores
the spectral envelope. THROAT and TENSION adjust envelope shape and detail.
Choose Stereo, Mid only, Side only, or Mid + Side in the lower rack.
Mid/Side blends independently scale the effect on the centre and width.
LISTEN DELTA auditions selected processing minus latency-aligned dry,
before output gain; global Mix scales the difference.
Global BYPASS restores aligned dry at unity gain.
Visualizer colors grow more saturated as Amount increases.
Open Processing Settings with the gear or oversampling button. Choose 1x, 2x or
4x, and Standard or High resampling filter quality. Standard uses shorter filters;
High uses steeper filters with greater alias rejection. Filter quality applies
at 2x and 4x. Reduced motion is also available in Processing Settings.
Studio uses an 8192-sample FFT; Real-time mode uses 4096 samples. Both retain
75% overlap and a Hann window. Oversampling multiplies the internal FFT and
sample rate together: Studio uses 8192/16384/32768, and Real-time uses
4096/8192/16384 at 1x/2x/4x. Each mode keeps its own analysis window and
frequency resolution. Oversampling changes prime then crossfade over 50 ms.
Host latency is 16445 samples in Studio or 8253 samples in Real-time mode,
including HPSS lookahead and FIR alignment. Resolution changes briefly fade
out, notify the host off the audio thread, then fade back in. Oversampling
and quality changes keep the selected resolution and latency.
An empty custom scale preserves pitch; Mix at zero gives aligned original audio.
INSTRUCTIONS
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/"
cp -R "$ROOT/ThirdParty" "$STAGE/ThirdParty"
mkdir -p "$STAGE/.background"
cp "$ROOT/Packaging/background.svg" "$STAGE/.background/background.svg"
cp "$ROOT/Packaging/background.png" "$STAGE/.background/background.png"
cp "$ROOT/Packaging/background.svg" "$STAGE/Aura — Installation Guide.svg"
DMG="$DIST/Aura-2.0.0-Universal.dmg"
TEMP_DMG="$DIST/.Aura-2.0.0-Universal.writable.dmg"
rm -f "$TEMP_DMG"
hdiutil create -volname "Aura · Spectral Harmony" -srcfolder "$STAGE" -fs HFS+ -format UDRW "$TEMP_DMG"
# Verify on the image filesystem as well as the staging filesystem.
MOUNT="$(hdiutil attach "$TEMP_DMG" -nobrowse | awk '/\/Volumes\// { sub(/^.*\/Volumes\//, "/Volumes/"); print; exit }')"
[[ -n "$MOUNT" ]] || { echo "Could not mount writable image" >&2; exit 1; }
for bundle in Aura.component Aura.vst3; do
    /usr/bin/xattr -cr "$MOUNT/$bundle"
    codesign --verify --deep --strict "$MOUNT/$bundle"
done
# Write Finder metadata directly; no Finder/Automation permission is required.
if [[ "${AURA_STYLE_DMG:-1}" == 1 ]]; then
        python3 -m venv "$BUILD/dmg-tools"
        "$BUILD/dmg-tools/bin/pip" install --disable-pip-version-check --quiet ds_store==1.3.1 mac_alias==2.2.3
        "$BUILD/dmg-tools/bin/python" "$ROOT/Packaging/style_dmg.py" "$MOUNT"
fi
hdiutil detach "$MOUNT" -quiet; MOUNT=""
hdiutil convert "$TEMP_DMG" -format UDZO -o "$DMG" -ov
rm -f "$TEMP_DMG"
codesign --force --sign - "$DMG"
codesign --verify --strict "$DMG"
hdiutil verify "$DMG"
MOUNT="$(hdiutil attach "$DMG" -nobrowse -readonly | awk '/\/Volumes\// { sub(/^.*\/Volumes\//, "/Volumes/"); print; exit }')"
[[ -n "$MOUNT" ]] || { echo "Could not mount final image" >&2; exit 1; }
for bundle in Aura.component Aura.vst3; do
    codesign --verify --deep --strict "$MOUNT/$bundle"
    lipo "$MOUNT/$bundle/Contents/MacOS/Aura" -verify_arch arm64 x86_64
done
hdiutil detach "$MOUNT" -quiet; MOUNT=""
(
    cd "$DIST"
    shasum -a 256 "${DMG##*/}"
) > "$DMG.sha256"
echo "Packaged $DMG"
