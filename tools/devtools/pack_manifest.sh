#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Turn <game>.us/filesystem.csv (pret's NitroFS manifest, whose first column names the archive as
# the ROM sees it) into the manifest nitrorom pack reads from the build directory:
#
#   - every source path is re-rooted for the build directory, which is where nitrorom resolves
#     both the INI paths and the manifest paths (pret gets this for free because meson mirrors the
#     source tree in the build directory);
#   - archives whose ROM name is stripped to a serial (`/a/X/Y/Z`, see arc_strip_name in
#     filesystem.mk) are looked up by that serial instead, because this tree builds or keeps them
#     under files/a/X/Y/Z rather than under the logical name pret's meson build produces.
#
# Sources are resolved against the working directory (the project root, i.e. where the make rule
# runs); only the emitted paths are re-rooted for the build directory.
#
# usage: pack_manifest.sh <filesystem.csv> <back-rel> <output.csv>

set -e

if [ $# -ne 3 ]; then
    echo "usage: $0 <filesystem.csv> <back-rel> <output.csv>" >&2
    exit 2
fi

csv=$1
back=$2
out=$3
lines=$(mktemp)
body=$(mktemp)
trap 'rm -f "$lines" "$body"' EXIT

tr -d '\r' <"$csv" >"$lines"
head -n 1 "$lines" >"$body"

tail -n +2 "$lines" | while IFS=, read -r source target; do
    [ -n "$source" ] || continue
    if [ -e "$source" ]; then
        artifact=$source
    elif [ -e "files$target" ]; then
        artifact=files$target
    else
        echo "$0: $source: no artifact in this tree (target $target)" >&2
        exit 1
    fi
    printf '%s/%s,%s\n' "$back" "$artifact" "$target" >>"$body"
done

cp "$body" "$out"
