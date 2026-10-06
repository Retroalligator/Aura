#!/usr/bin/env bash
set -euo pipefail
[[ "$AURA_TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]
version="${AURA_TAG#v}"
test "$(git show "$AURA_TAG:CMakeLists.txt" | sed -n 's/^project(Aura VERSION \([0-9.]*\).*/\1/p')" = "$version"
mkdir -p dist "complete-source/Aura-$version/vendor/JUCE" evidence
# Confirm original per-file digests before collecting release artifacts.
for platform in macOS Windows; do
    (cd "payloads/Aura-release-$platform/dist" && sha256sum -c ./*.sha256)
done
find payloads -type f \( -name '*.dmg' -o -name '*.exe' -o -name '*.zip' \) -exec cp '{}' dist/ \;
for name in "Aura-$version-Universal.dmg" "Aura-$version-Windows-x64-Setup.exe" "Aura-$version-Windows-x64.exe" "Aura-$version-Windows-x64.zip"; do test -f "dist/$name"; done
git archive "$AURA_TAG" | tar -x -C "complete-source/Aura-$version"
curl --fail --location --retry 3 https://codeload.github.com/juce-framework/JUCE/tar.gz/91ad83ae34a81e0833b1a2b0866f54846370ae53 -o "$RUNNER_TEMP/juce-source.tar.gz"
echo "04f8d5055382582c757be9da069ea98338005f98248facd9c2804435ac853e70  $RUNNER_TEMP/juce-source.tar.gz" | sha256sum -c -
tar -xzf "$RUNNER_TEMP/juce-source.tar.gz" -C "complete-source/Aura-$version/vendor/JUCE" --strip-components=1
tar -czf "dist/Aura-$version-complete-source.tar.gz" -C complete-source "Aura-$version"
# Use the refreshed AU report, not the earlier registrar discovery failure.
cp -R recovery-evidence/. evidence/
cp payloads/Aura-release-Windows/build-windows/Testing/Temporary/LastTest.log evidence/ctest-windows.txt
tar -czf "dist/Aura-$version-CI-validation.tar.gz" -C evidence .
(cd dist && sha256sum *.dmg *.exe *.zip *.tar.gz > SHA256SUMS.txt)
gh release upload "$AURA_TAG" dist/* --repo "$GITHUB_REPOSITORY"
