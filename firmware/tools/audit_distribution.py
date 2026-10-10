# SPDX-License-Identifier: MIT
"""Audit the sealed archive against current sources, then relink after extraction."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--version', required=True)
parser.add_argument('--extract-to', required=True, type=Path)
parser.add_argument('--compiler', required=True, type=Path)
options = parser.parse_args()
assert all(c.isalnum() or c in '.-_' for c in options.version)
archive_path = root / f'distribution/EasyREAD-share-{options.version}.zip'
extracted = options.extract_to.resolve()
assert not extracted.exists(), 'Extraction target must not exist'
expected_epubs = {'firmware/test-fixtures/drm.epub', 'firmware/test-fixtures/garbage.epub',
                  'firmware/test-fixtures/truncated.epub', 'sd-card/Books/Faiyuet-Morgan-test.epub'}
samples = root / 'firmware/test-fixtures/formats'
sample_names = {p.name for p in samples.iterdir() if p.is_file() and (p.name[:2].isdigit() or p.name == 'README.md')}
expected_books = {'sd-card/Books/Faiyuet-Morgan-test.epub'} | {'sd-card/Books/EasyREAD-Tests/' + n for n in sample_names}
with zipfile.ZipFile(archive_path) as archive:
    names = set(archive.namelist())
    assert {n for n in names if n.lower().endswith('.epub')} == expected_epubs
    assert {n for n in names if n.startswith('sd-card/Books/')} == expected_books
    assert 'firmware/test-fixtures/formats/manifest.json' in names
    assert {'legal/Python-GBK/LICENSE.txt', 'legal/Python-GBK/NOTICE.md',
            'verification/format-check.json', 'verification/incremental-check.txt'} <= names
    assert not any('georgia' in n.lower() and n.lower().endswith(('.ttf', '.otf')) for n in names)
    assert not any(n.lower().endswith(('.wav', '.pem', '.key')) for n in names)
    assert not any(part in {'.git', '.venv', '.pio', '__pycache__'} for n in names for part in Path(n).parts)
    manifest = json.loads(archive.read('manifest.json'))
    assert names == set(manifest['files']) | {'manifest.json'}
    compared = 0
    for name in names:
        if name == 'manifest.json': continue
        data = archive.read(name)
        record = manifest['files'][name]
        assert len(data) == record['bytes'] and hashlib.sha256(data).hexdigest() == record['sha256'], name
        current = root / name
        if name.startswith('sd-card/Books/EasyREAD-Tests/'):
            current = samples / Path(name).name
        if name.startswith(('firmware/src/', 'firmware/tools/', 'firmware/test-fixtures/', 'legal/', 'docs/', 'assets/branding/', 'sd-card/')) or name in {
            'README.md', 'README.en.md', 'STATE.md', 'LICENSE.md', 'LICENSE_SCOPE.md', 'THIRD_PARTY_NOTICES.md', 'firmware/README.md', 'firmware/platformio.ini'}:
            if not current.is_file() and name.startswith(('legal/GCC-runtime/', 'legal/newlib/', 'legal/source-archives/', 'legal/esp_diagnostics/')):
                continue
            assert current.is_file() and data == current.read_bytes(), name
            compared += 1
        assert (extracted/name).resolve().is_relative_to(extracted), name
    extracted.mkdir()
    archive.extractall(extracted)
subprocess.run([sys.executable, str(extracted/'firmware/tools/relink_distribution.py'),
                '--package-root', str(extracted), '--compiler', str(options.compiler), '--verify-original'], check=True)
result = json.loads((extracted/'relinked/verification.json').read_text(encoding='utf-8'))
result.update(package=archive_path.name, package_sha256=hashlib.sha256(archive_path.read_bytes()).hexdigest(),
              packaged_source_and_license_match_verified=True, source_and_notice_files_compared=compared,
              epub_allowlist_verified=True, sample_books_verified=8, extracted_relink_verified=True, hardware_verified=False)
(root/f'distribution/extracted-relink-check-{options.version}.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
report_path = root/f'distribution/verification-{options.version}.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
report.update(extracted_relink_verified=True, packaged_source_and_license_match_verified=True,
              epub_allowlist_verified=True, sample_books_verified=8)
report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
