#!/usr/bin/env python3
"""Install the built addon and enable Nexus autoload while GW2 is closed."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import tempfile

SIGNATURE = 0x45564F42
NAME = 'Events and Metas Observer (Fedora port)'
BASE = Path(__file__).resolve().parent
DEFAULT_GAME = Path.home() / '.local/share/Steam/steamapps/common/Guild Wars 2'


def game_running(proc_root=Path('/proc')):
    for proc in proc_root.iterdir():
        if not proc.name.isdigit():
            continue
        try:
            if (proc / 'comm').read_text().strip().lower() == 'gw2-64.exe':
                return True
        except OSError:
            pass
    return False


def install(game, binary, check_running=True):
    if check_running and game_running():
        raise ValueError('Close GW2 before installation so Nexus cannot overwrite its configuration.')
    game = Path(game).expanduser().resolve()
    addons = game / 'addons'
    config = addons / 'Nexus/AddonConfig.json'
    if not (game / 'Gw2-64.exe').is_file() or not (game / 'd3d11.dll').is_file() or not config.is_file():
        raise ValueError('Nexus installation was not found. Select the GW2 directory with --game and launch Nexus once first.')
    if not binary.is_file():
        raise ValueError('Build dist/events-observer.dll first, or download it from this project\'s release.')
    records = json.loads(config.read_text(encoding='utf-8-sig'))
    if not isinstance(records, list) or any(not isinstance(record, dict) for record in records):
        raise ValueError('Unexpected Nexus configuration format; nothing was changed.')
    matches = [record for record in records if record.get('Signature') == SIGNATURE]
    if len(matches) > 1 or any(record.get('Name') != NAME for record in matches):
        raise ValueError('The addon signature is already used by an unexpected record; nothing was changed.')
    record = matches[0] if matches else {
        'Signature': SIGNATURE, 'Name': NAME, 'IsFavorite': False,
        'AllowPrereleases': False, 'IsPausingUpdates': False,
    }
    record['IsLoaded'] = True
    record['IsDisabledUntilUpdate'] = False
    if not matches:
        records.append(record)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = config.with_name(config.name + '.before-events-observer-' + stamp + '.bak')
    target = addons / 'events-observer.dll'
    shutil.copy2(config, backup)
    if target.exists():
        shutil.copy2(target, target.with_name(target.name + '.before-' + stamp + '.bak'))
    staged = target.with_suffix('.installing')
    try:
        shutil.copy2(binary, staged)
        if hashlib.sha256(staged.read_bytes()).digest() != hashlib.sha256(binary.read_bytes()).digest():
            raise ValueError('Installed DLL checksum differs from the build; configuration was not changed.')
        staged.replace(target)
    finally:
        staged.unlink(missing_ok=True)
    with tempfile.NamedTemporaryFile(mode='w', dir=config.parent, prefix='events-config-', suffix='.installing', delete=False) as output:
        staged_config = Path(output.name)
        json.dump(records, output, indent=2)
        output.write('\n')
        output.flush()
        os.fsync(output.fileno())
    try:
        staged_config.chmod(stat.S_IMODE(config.stat().st_mode))
        staged_config.replace(config)
    finally:
        staged_config.unlink(missing_ok=True)
    return target, backup


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=DEFAULT_GAME, help='Directory containing Gw2-64.exe')
    args = parser.parse_args()
    try:
        target, backup = install(args.game, BASE / 'dist/events-observer.dll')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    print('Installed:', target)
    print('Nexus automatic loading: enabled (IsLoaded=true). Start GW2 normally.')
    print('Configuration backup:', backup)


if __name__ == '__main__':
    main()
