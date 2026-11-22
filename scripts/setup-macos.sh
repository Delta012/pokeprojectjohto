#!/usr/bin/env bash
# Reproduce this fork's macOS toolchain without changing system Wine settings.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This helper is for macOS. Other platforms: see INSTALL.md.' >&2
    exit 1
fi
command -v brew >/dev/null || { echo 'Install Homebrew first: https://brew.sh' >&2; exit 1; }
if [[ "$(uname -m)" == arm64 ]] && ! arch -x86_64 /usr/bin/true; then
    echo 'Rosetta 2 is required to run the original x86 compiler through Wine.' >&2
    exit 1
fi
HOMEBREW_NO_AUTO_UPDATE=1 brew install coreutils make gnu-sed arm-none-eabi-binutils libpng pkgconf
python3 - <<'PY'
import hashlib
from pathlib import Path
import subprocess
import tempfile
import urllib.request

root = Path.cwd()
local = root / '.local-tools'
local.mkdir(exist_ok=True)

def download(url, path, expected):
    print(f'Downloading {path.name}', flush=True)
    urllib.request.urlretrieve(url, path)
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f'SHA256 mismatch for {path.name}: {actual}')

# CodeWarrior for ARM comes from the metroskrew distribution (fetched by `make skrew`), which runs
# through Wine on macOS; only that Wine prefix is installed here.
with tempfile.TemporaryDirectory(prefix='pokeheartgold-setup-') as folder:
    tmp = Path(folder)
    if not (local / 'Wine Devel.app/Contents/Resources/wine/bin/wine').exists():
        archive = tmp / 'wine-devel-11.16-osx64.tar.xz'
        download('https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.16/' + archive.name,
                 archive, '6f9af818b7af6001aeed7818cb32bf0155598c5ea4e3b33380a03cf814e033cd')
        subprocess.run(['tar', '-xf', str(archive), '-C', str(local)], check=True)
print('Dependencies ready. Run ./scripts/build.sh progress (it fetches metroskrew and builds tools/).')
PY
