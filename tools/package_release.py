#!/usr/bin/env python3
"""Package tracked project files plus the prebuilt DLL, without local user data."""
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
dist = root / 'dist'
dll = dist / 'events-observer.dll'
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
files = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
info = {
    'artifact': dll.name, 'sha256': hashlib.sha256(dll.read_bytes()).hexdigest(),
    'source_revision': revision, 'target': 'x86_64-windows-gnu',
    'nexus_api': 6, 'compiler': 'Zig 0.17.0', 'schedule_rows': 104, 'unique_events': 88,
    'validation': 'Native C++ schedule tests; 12 Python generator/installer tests; PE verification; Wine 11 actual DLL load/render/unload with simulated Nexus API. User confirmed the addon works in GW2; automated integration uses a simulated Nexus API.',
}
(dist / 'BUILD-INFO.json').write_text(json.dumps(info, indent=2) + '\n')
archive = dist / 'events-observer-nexus-fedora-0.1.2.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as out:
    for name in files + ['dist/events-observer.dll', 'dist/BUILD-INFO.json']:
        if name:
            out.write(root / name, Path('events-observer-nexus') / name)
assets = [dll, archive, dist / 'BUILD-INFO.json']
(dist / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
print(archive)
