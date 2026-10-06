# Aura on GitHub Packages

`ghcr.io/retroalligator/aura:2.0.0` contains the same native installers,
complete source, validation evidence and checksums published in the GitHub
release. It is an archive image with files under `/aura`; it does not run the
plugin on Linux. For normal installation, download the DMG or Windows Setup
EXE from [the release](https://github.com/Retroalligator/Aura/releases/tag/v2.0.0).

To extract the package with Docker without starting a container:

```sh
docker pull ghcr.io/retroalligator/aura:2.0.0
docker create --name aura-release ghcr.io/retroalligator/aura:2.0.0 /unused
docker cp aura-release:/aura ./Aura-2.0.0
docker rm aura-release
```

Open the DMG on macOS or the Setup EXE on Windows. The package includes an
INSTALL.txt and release-manifest.json tying the files to the release commit.
macOS is ad-hoc signed and not notarized; Windows is unsigned. The license is
AGPL-3.0-only and full corresponding source includes pinned JUCE.

The package workflow runs after successful installer release CI, downloads the
published assets, validates their complete SHA-256 manifest and tag commit,
then pushes the archive image with a temporary GITHUB_TOKEN. It uses only the
trusted default branch for workflow code and rejects a source commit mismatch.
Manual dispatch also supports an existing release tag. Newly published registry
packages must be made public in their package settings to allow anonymous pulls.
