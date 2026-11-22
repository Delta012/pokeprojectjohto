#!/bin/sh

# Usage: Run this script from the project root and provide the following environment variables:
#   - SKREW_SYS - controls the distribution downloaded; either `windows`, `linux`, or `wine`
#   - SKREW_VER - the release version of metroskrew archive to be downloaded
#   - SKREW_DIR - the subproject directory in which metroskrew shall be stored
#
# The makefiles run it as `sh tools/devtools/get_metroskrew.sh`, which is dash on Debian and Ubuntu:
# keep this POSIX sh — no `pipefail`, no `pushd`/`popd`.

set -eux

case "$SKREW_SYS" in
windows)
    SKREW_EXT=".7z"
    if command -v 7z >/dev/null 2>&1; then
        EXTRACTOR="7z x -y -o."
    else
        EXTRACTOR="p7zip -d"
    fi
    ;;

linux | wine)
    SKREW_EXT=".tar.xz"
    EXTRACTOR="tar xf"
    ;;

*)
    echo "Unrecognized platform $SKREW_SYS"
    exit 1
    ;;
esac

TMP="/tmp"

SKREW="metroskrew"
SKREW_GH="https://github.com/mid-kid/$SKREW"
SKREW_ARC="metroskrew-$SKREW_SYS$SKREW_EXT"
SKREW_URL="$SKREW_GH/releases/download/$SKREW_VER/$SKREW_ARC"
SKREW_TMP="$TMP/$SKREW"

rm -rf "$SKREW_DIR"

# We extract the archive to `/tmp` to ensure that Windows does not muddle with permissions.
(
    cd "$TMP"
    if command -v wget >/dev/null 2>&1; then
        wget --quiet --show-progress -O "$SKREW_ARC" "$SKREW_URL"
    else
        curl -fL --progress-bar -o "$SKREW_ARC" "$SKREW_URL"
    fi
    $EXTRACTOR "$SKREW_ARC" >/dev/null
)

mv "$SKREW_TMP" "$SKREW_DIR"
