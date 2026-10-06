# SPDX-License-Identifier: AGPL-3.0-only
"""Verify the exact native release bundle before sending it to GitHub Packages."""
import argparse
import hashlib
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--tag', required=True)
parser.add_argument('--revision', required=True)
parser.add_argument('--payloads', type=Path, required=True)
args = parser.parse_args()
if not re.fullmatch(r'v\d+\.\d+\.\d+', args.tag):
    raise SystemExit('Invalid release tag')
if not re.fullmatch(r'[0-9a-f]{40}', args.revision):
    raise SystemExit('Invalid release commit')
version = args.tag[1:]
expected = {
    f'Aura-{version}-Universal.dmg',
    f'Aura-{version}-Windows-x64-Setup.exe',
    f'Aura-{version}-Windows-x64.exe',
    f'Aura-{version}-Windows-x64.zip',
    f'Aura-{version}-complete-source.tar.gz',
    f'Aura-{version}-CI-validation.tar.gz',
}
lines = (args.payloads / 'SHA256SUMS.txt').read_text().splitlines()
checksums = {}
for line in lines:
    match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
    if not match or match[2] in checksums:
        raise SystemExit('Invalid or duplicate checksum record')
    checksums[match[2]] = match[1]
if set(checksums) != expected:
    raise SystemExit('Checksum manifest differs from expected release assets')
if {p.name for p in args.payloads.iterdir()} != expected | {'SHA256SUMS.txt'}:
    raise SystemExit('Downloaded payloads differ from expected release assets')
for name, digest in checksums.items():
    with (args.payloads / name).open('rb') as stream:
        actual = hashlib.file_digest(stream, 'sha256').hexdigest()
    if actual != digest:
        raise SystemExit(f'Checksum mismatch: {name}')
manifest = {
    'repository': 'Retroalligator/Aura', 'tag': args.tag, 'commit': args.revision,
    'release': f'https://github.com/Retroalligator/Aura/releases/tag/{args.tag}',
    'assets': [{'name': name, 'sha256': checksums[name]} for name in sorted(expected)],
}
(args.payloads / 'release-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(args.payloads / 'INSTALL.txt').write_text(
    f'Aura {version} native release bundle\n\n'
    'macOS: open the Universal DMG and install the AU/VST3 bundles.\n'
    'Windows: run the Setup EXE, or extract the portable VST3 ZIP.\n'
    'The standalone EXE requires an audio input/device to process.\n'
    'The complete-source archive includes the exact pinned JUCE dependency.\n'
    'SHA256SUMS.txt and CI validation evidence accompany the installers.\n\n'
    'This registry image is a file archive; extract /aura rather than run it.\n'
    'macOS is ad-hoc signed and not notarized; Windows is unsigned.\n'
    'License: AGPL-3.0-only.\n'
    + manifest['release'] + '\n'
)
print(f'PASS {len(expected)} release assets verified for {args.tag} at {args.revision}')
