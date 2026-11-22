#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Remove every NitroFS build product under files/ that the make rules can regenerate.
#
# The per-arc mks under files/ each add a clean-<arc> hook, and those hooks name the bins and the
# .narc the mk declares. They miss three classes of output, which is what this script covers:
#
#   - the graphics leaves and their .lz siblings that the generic rules of graphics_files_rules.mk
#     (%.NCGR: %.png, %.NCLR: %.png, %.NCLR: %.pal, %.lz: %) write next to their source;
#   - the .narc of an arc whose sources live in the sibling directory of the same name, which the
#     generic "%.narc: $(wildcard $*/*.$ext)" rule reaches but no mk hook names (this includes the
#     arcs of the *other* game, which the per-game NAIXS list does not cover);
#   - the .bin/.o/.c/.d intermediates the arc tools (csv2bin, o2narc, jsonproc, mwasm) leave behind.
#
# .naix files are deliberately left alone: the per-game NAIXS list in filesystem.mk removes the ones
# it knows about, and the version-specific ones (zukan_hw_data_gold.naix, zukan_enc_silver.naix, ...)
# have no rule of their own at all — filesystem.mk:557 declares "%.naix: %.narc" with no recipe, and
# the file is really a side product of csv2bin, which only writes it when it rebuilds the arc's bins
# (CSV2BINFLAGS carries --naix). Deleting one therefore leaves a prerequisite that make cannot
# satisfy: the rule that post-processes it (the "s/_gold//g" sed in zukan_hw_data.mk) then reads a
# missing input. That is what broke the first version of this script.
#
# The test for whether clean may remove something is whether a make rule can rebuild it, so every
# removal here is driven off a source file that is present in the tree, and the leaf/narc sweeps
# only name outputs whose source exists. The exceptions are the SHIPPED list below: binaries this
# tree still ships (they are tracked in git) although their source has been decompiled; removing
# them would show up as deleted tracked files.
#
# Candidates are collected in a temporary list and removed in a few xargs batches. Naming each
# candidate to its own `rm` cost about ten thousand process spawns (3403 png/pal sources times four
# outputs, 2663 source stems, 1703 arc directories), and on MSYS2 that made this script alone take
# 35 s of the 44 s `make clean`. The batched form needs GNU xargs (`-d '\n'`), which is what both
# build hosts ship.
#
# usage: clean_filesystem.sh [project-root]        (default: the working directory)

set -e

root=${1:-.}
cd "$root"

# tracked binaries this tree ships even though their source is decompiled
shipped() {
    case $1 in
    files/fielddata/graphic/preview_graphic/preview_graphic/*.NSCR) return 0 ;;
    files/graphic/camera_viewfinder/camera_viewfinder.NSCR) return 0 ;;
    *) return 1 ;;
    esac
}

cand=$(mktemp "${TMPDIR:-/tmp}/clean_filesystem.XXXXXX")
trap 'rm -f "$cand"' EXIT INT TERM

# queue one output path if it exists and is not a shipped binary
add() {
    [ -f "$1" ] || return 0
    if shipped "$1"; then return 0; fi
    printf '%s\n' "$1" >> "$cand"
}

# graphics leaves: %.NCGR / %.NCLR / %.nbfc / %.nbfp from .png, %.NCLR from .pal
find files -type f \( -name '*.png' -o -name '*.pal' \) -print | while read -r src; do
    case $src in
    *.png)
        stem=${src%.png}
        add "$stem.NCGR"
        add "$stem.NCLR"
        add "$stem.nbfc"
        add "$stem.nbfp"
        ;;
    *.pal)
        add "${src%.pal}.NCLR"
        ;;
    esac
done

# csv2bin / jsonproc / asm outputs: <source stem>.bin
find files -type f \
    \( -name '*.csv' -o -name '*.json' -o -name '*.msg' -o -name '*.s' -o -name '*.scr' \
    -o -name '*.4bpp' -o -name '*.bmd' -o -name '*.bz2' \) -print | while read -r src; do
    add "${src%.*}.bin"
done

# arcs of the generic directory rule, including the other game's
find files -type d -print | while read -r dir; do
    add "$dir.narc"
done

if [ -s "$cand" ]; then
    xargs -r -d '\n' rm -f -- < "$cand"
fi

# pure intermediates: nothing of these extensions is tracked anywhere under files/
find files -type f \( -name '*.lz' -o -name '*.o' -o -name '*.c' -o -name '*.d' \) -delete
