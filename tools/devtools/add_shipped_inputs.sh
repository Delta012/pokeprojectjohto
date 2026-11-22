#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Track the build inputs that .gitignore hides (`*.narc` in the root, plus arc-local ignores
# such as files/arc/.gitignore's `headbutt.*.narc`). A fresh clone without them cannot build:
# the arcs in tools/devtools/shipped_inputs.txt have no input directory, so no make rule can
# produce them.
#
# Run from anywhere in the repository, then commit what it stages.
#
# usage: add_shipped_inputs.sh [manifest]      (default: tools/devtools/shipped_inputs.txt)

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"
manifest=${1:-tools/devtools/shipped_inputs.txt}

if [ ! -f "$manifest" ]; then
    echo "no manifest at $manifest" >&2
    exit 1
fi

paths=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$manifest" | tr -d '\r')

missing=0
for path in $paths; do
    if [ ! -f "$path" ]; then
        echo "not present in this checkout: $path" >&2
        missing=$((missing + 1))
    fi
done
if [ "$missing" -ne 0 ]; then
    echo "$missing manifest entries are missing from the working tree" >&2
    exit 1
fi

# -f is required: these paths are ignored on purpose, the generated arcs next to them are not.
git add -f -- $paths
git diff --cached --name-only -- $paths | wc -l | while read -r n; do
    echo "staged $n shipped inputs (manifest lists $(echo "$paths" | wc -l))"
done
