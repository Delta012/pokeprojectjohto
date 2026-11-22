#!/usr/bin/env python3
"""Re-derive producer pins for the guarded partition producers after a toolchain change.

The producers under tools/alarm-partition, tools/arm7-alarm-partition, tools/dwc-iterator-advance
and tools/msl-* pin the exact compiler binary and the exact command line that is allowed to
produce their objects, so the objects they emit stay reproducible. Moving the build to metroskrew
(the `-wrap:sdk <version>` CodeWarrior distribution) changes both, so the pins are re-read from
the makefiles instead of being hand-edited.

Run from the repository root, once per game version:

    python3 tools/devtools/reset_producer_pins.py --game HEARTGOLD
    python3 tools/devtools/reset_producer_pins.py --game SOULSILVER

What is rewritten: the compiler binaries (the launcher plus the payload binary that `-wrap:sdk
<version>` selects), the producer helper scripts, and the recorded command lines. Pinned sources,
headers and the rest of the closure are verified against their current digests, and drift aborts
the run, so a genuine source change still fails loudly instead of being silently re-pinned.

Toolchain pins are host-specific, so the toolchain sections they write are keyed by host:

    "tool_sha256": {"windows": {"tools/metroskrew/bin/mwccarm.exe": ...},
                    "linux":   {"tools/metroskrew/bin/mwccarm":      ...}}

metroskrew installs one host's distribution at a time, Windows ships PE launchers with a `.exe`
suffix and Linux ELF launchers without one, and the versioned payload binaries keep the same name
on both hosts while holding different bytes. A run rewrites only the running host's entry, so one
profile pins both distributions once each host has been used; tools/devtools/mw_host.py is the
selector the producers read, and a profile with no entry for the running host aborts rather than
compiling with an unpinned compiler. Run this script once per host (and after any payload update).

Three producer routes have no source in this tree, so nothing builds them and no dry run exists:
    lib/NitroSDK/asm/to90_os_alarm_handler.s      (alarm-partition handler role)
    sub/lib/asm/to90_arm7_alarm_handler.s         (arm7-alarm-partition handler role)
    lib/NitroDWC/src/to91_ov45_iterator_advance108.cpp (dwc-iterator-advance)
Their recorded command lines are carried over unchanged apart from the tool path and the
`-wrap:sdk <version>` insertion; they are reported so the gap stays visible.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path

import mw_host

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'common.mk').is_file())
BIN = 'tools/metroskrew/bin'
CPP_TAIL = ['-gccdep', '-MD', '-c', '-o']
# Producer routes that this tree carries no source for: label -> missing file
UNSOURCED = {
    'alarm-partition handler': 'lib/NitroSDK/asm/to90_os_alarm_handler.s',
    'arm7-alarm-partition handler': 'sub/lib/asm/to90_arm7_alarm_handler.s',
    'dwc-iterator-advance': 'lib/NitroDWC/src/to91_ov45_iterator_advance108.cpp',
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(pairs, what: str = 'pinned inputs') -> None:
    """Abort if a pinned non-tool input changed or disappeared underneath us."""
    for rel, pin in pairs:
        path = ROOT / rel
        if not path.is_file():
            raise SystemExit(f'{what}: pinned input missing: {rel}')
        if pin is not None and digest(path) != pin:
            raise SystemExit(f'{what}: {rel} changed; that is source drift, not a toolchain change')


def compilers(version: str, tools: tuple[str, ...]) -> dict[str, str]:
    """Digest the launcher and the payload binary `-wrap:sdk <version>` resolves for each tool.

    Both are host-specific: metroskrew installs `bin/mwccarm.exe` (PE) on Windows and
    `bin/mwccarm` (ELF) on Linux, and the versioned payload binaries keep the same name on
    every host while carrying different bytes, so the result is stored under a host key.
    """
    pins: dict[str, str] = {}
    for tool in tools:
        names = [mw_host.launcher(tool)]
        # The version-to-binary mapping file keeps the `.exe.txt` spelling on every host.
        mapping = ROOT / 'tools/metroskrew/share/metroskrew/sdk/ds' / version / f'{tool}.exe.txt'
        if mapping.is_file():
            names.append(f'{BIN}/{mapping.read_text().strip()}')
        for rel in names:
            path = ROOT / rel
            if not path.is_file():
                raise SystemExit(f'compiler pins: missing {rel} (run `make skrew`)')
            pins[rel] = digest(path)
    return pins


def inherited(profile: dict, key: str) -> list[str]:
    """Recorded command line to carry over when re-pinning for this host.

    This is the one read that may look at *another* host's entry: on the first run on a host the
    section only holds the other host's line, and `ported_tokens` swaps the launcher and the
    `-wrap:sdk <version>` pair anyway. Producers never read this way — they only accept their own
    host's entry.
    """
    value = profile[key]
    if not mw_host.is_host_keyed(value):
        return value
    for host in (mw_host.HOST, *mw_host.SUPPORTED_HOSTS):
        if host in value:
            return value[host]
    raise SystemExit(f'{key}: no recorded command line to carry over')


def make(game: str, args: list[str], in_sub: bool = False) -> str:
    return subprocess.run(['make', *args], cwd=ROOT / 'sub' if in_sub else ROOT,
                          check=True, capture_output=True, text=True).stdout


def bootstrap(game: str) -> None:
    """Bring back the generated inputs the profiles pin; a freshly cleaned tree has none.

    `fx_const.h` is generated by the build (and removed by `make clean`), yet it is pinned as a
    producer input, so `make clean; reset_producer_pins.py; make` only works if the script can
    rebuild what it has to digest. `skrew` likewise restores the toolchain after clean-skrew.
    """
    make(game, ['skrew'])
    make(game, [f'GAME_VERSION={game}', 'lib/include/nitro/fx/fx_const.h'])


def producer_tokens(game: str, target: str, option: str, in_sub: bool = False) -> list[str]:
    """The command line make would hand the producer for `target`, as tokens."""
    # `-B` keeps the recipe printed once the object has already been built; `-n` keeps it unexecuted.
    for line in make(game, ['-B', '-n', f'GAME_VERSION={game}', target], in_sub).splitlines():
        if 'build.py' in line and f'{option} "' in line:
            _, _, quoted = line.partition(f'{option} "')
            return shlex.split(quoted.rstrip()[:-1])
    raise SystemExit(f'no producer command line recorded for {target}')


def command_tokens(tokens: list[str]) -> list[str]:
    """Repository-relative form of a captured command line."""
    exe = Path(tokens[0]).resolve().relative_to(ROOT).as_posix()
    return [exe] + tokens[1:]


def ported_tokens(tokens: list[str], launcher: str, version: str) -> list[str]:
    """Carry an inherited command line over to the ported toolchain (idempotent)."""
    rest = [t for t in tokens if t != 'wine'][1:]
    if rest[:1] == ['-wrap:sdk']:
        rest = rest[2:]
    return [launcher, '-wrap:sdk', version] + rest


def strip_tail(tokens: list[str]) -> list[str]:
    """Drop the pieces the producer script appends itself before pinning the line."""
    if tokens[-len(CPP_TAIL):] != CPP_TAIL:
        raise SystemExit(f'unexpected producer command tail: {" ".join(tokens[-8:])}')
    return tokens[:-len(CPP_TAIL)]


def modules_pins(profile_path: Path, profile: dict) -> dict[str, str]:
    """Re-pin the producer's own helper scripts; they are tool code, not decompiled input."""
    return {name: digest(profile_path.parent / name) for name in profile['modules']}


def note(route: str) -> None:
    print(f'  note: {route} command line carried over; {UNSOURCED[route]} is absent from this tree')


def alarm(path: Path, profile: dict, game: str) -> None:
    build = f'build/{game.lower()}.us'
    c = command_tokens(producer_tokens(game, f'{build}/lib/NitroSDK/src/os/os_alarm_before_handler.o', '--gnu-profile'))
    version = c[c.index('-wrap:sdk') + 1]
    pins = compilers(version, ('mwccarm', 'mwasmarm'))
    verify((rel, None) for rel in profile['C_dependencies'])
    # closure keeps only host-independent inputs; the toolchain binaries move to the
    # host-keyed tool_sha256 section, the same shape arm7-alarm-partition already uses.
    keep = {rel: pin for rel, pin in profile['closure'].items() if not rel.startswith('tools/')}
    verify((rel, pin) for rel, pin in keep.items())
    profile['closure'] = keep
    mw_host.put_section(profile, 'tool_sha256', pins)
    profile['modules'] = modules_pins(path, profile)
    mw_host.put_section(profile, 'GNU_C_tokens', c)
    note('alarm-partition handler')
    asm = ported_tokens(inherited(profile, 'GNU_ASM_tokens'), mw_host.launcher('mwasmarm'), version)
    mw_host.put_section(profile, 'GNU_ASM_tokens', asm)


def arm7(path: Path, profile: dict, game: str) -> None:
    c = command_tokens(producer_tokens(game, 'build/lib/src/to90_arm7_alarm_prefix.o', '--gnu-profile', in_sub=True))
    version = c[c.index('-wrap:sdk') + 1]
    for role, row in profile['roles'].items():
        if role == 'handler':  # no source in this tree; nothing compiles it
            note('arm7-alarm-partition handler')
            continue
        verify([(row['source'], row['sha256'])], f'arm7-alarm role {role}')
    verify((rel, pin) for rel, pin in profile['C_header_pins'].items())
    mw_host.put_section(profile, 'tool_sha256', compilers(version, ('mwccarm', 'mwasmarm')))
    profile['modules'] = modules_pins(path, profile)
    mw_host.put_section(profile, 'C_base', c)
    asm = ported_tokens(inherited(profile, 'ASM_base'), mw_host.launcher('mwasmarm'), version)
    mw_host.put_section(profile, 'ASM_base', asm)


def cpp_producer(path: Path, profile: dict, game: str) -> None:
    build = f'build/{game.lower()}.us'
    targets = {
        'tools/msl-d1-partition/profile.json': [f'{build}/src/to91_msl_shared_ptr_char_d1.o'],
        'tools/msl-new-partition/profile.json': [f'{build}/src/to91_msl_scalar_new.o',
                                                 f'{build}/src/to91_msl_array_new.o'],
    }[path.relative_to(ROOT).as_posix()]
    tokens = [command_tokens(producer_tokens(game, target, '--command')) for target in targets]
    if 'source' in profile:
        verify([(profile['source'], profile.get('source_sha256'))], path.parent.name)
    verify((rel, pin) for rel, pin in profile['headers'].items())
    pins = compilers(tokens[0][tokens[0].index('-wrap:sdk') + 1], ('mwccarm',))
    launcher = next(iter(pins))
    mw_host.put_section(profile, 'compiler', launcher)
    mw_host.put_section(profile, 'compiler_sha256', pins[launcher])
    profile['modules'] = modules_pins(path, profile)
    if 'producers' in profile:
        for key, line in zip(('scalar', 'array'), tokens):
            entry = profile['producers'][key]
            verify([(entry['source'], entry['source_sha256'])], f'{path.parent.name} {key}')
            mw_host.put_entry(entry['GNU_CPP_tokens'], game.lower(), strip_tail(line))
    else:
        mw_host.put_entry(profile['GNU_CPP_tokens'], game.lower(), strip_tail(tokens[0]))


HANDLERS = {
    'tools/alarm-partition/profile.json': alarm,
    'tools/arm7-alarm-partition/profile.json': arm7,
    # tools/dwc-iterator-advance is left exactly as inherited: its source is absent, the object is
    # in no link response, and its header pins no longer match this tree, so nothing here can
    # legitimately re-derive them.
    'tools/msl-d1-partition/profile.json': cpp_producer,
    'tools/msl-new-partition/profile.json': cpp_producer,
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', default='HEARTGOLD', choices=['HEARTGOLD', 'SOULSILVER'])
    game = parser.parse_args().game
    bootstrap(game)
    for rel, handler in HANDLERS.items():
        path = ROOT / rel
        profile = json.loads(path.read_text())
        handler(path, profile, game)
        path.write_text(json.dumps(profile, indent=2, ensure_ascii=False) + '\n')
        print(f're-pinned {rel} for {game}')


main()
