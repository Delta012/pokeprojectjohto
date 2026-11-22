#!/usr/bin/env bash
# Run from any directory. Keep Wine's state within this checkout.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

make_bin=make
if [[ "$(uname -s)" == Darwin ]]; then
    make_bin=gmake
    local_wine="$repo_root/.local-tools/Wine Devel.app/Contents/Resources/wine/bin"
    if [[ -x "$local_wine/wine" ]]; then
        export PATH="$local_wine:$PATH"
    fi
fi
export WINEPREFIX="${WINEPREFIX:-$repo_root/.local-tools/wineprefix}"
export WINEDEBUG="${WINEDEBUG:--all}"

for program in "$make_bin" python3; do
    if ! command -v "$program" >/dev/null; then
        echo "Missing $program. See INSTALL.md for setup." >&2
        exit 1
    fi
done
if [[ "$(uname -s)" == Darwin ]]; then
    for program in wine winepath; do
        if ! command -v "$program" >/dev/null; then
            echo "Missing $program; macOS runs the CodeWarrior binaries under Wine. See INSTALL.md." >&2
            exit 1
        fi
    done
fi

# Metrowerks CodeWarrior comes from the metroskrew distribution, and the ROM/lcf tools are built
# from the sources in tools/; both steps are no-ops once present.
"$make_bin" skrew
"$make_bin" tools

exec "$make_bin" -j"${JOBS:-8}" "$@"
