"""Host selector for the metroskrew toolchain pins.

metroskrew installs one archive per host, the same split the Makefile uses for `SKREW_SYS`:

    windows  bin/mwccarm.exe  bin/mwasmarm.exe  bin/mwldarm.exe  bin/skrewrap.exe   (PE)
    linux    bin/mwccarm      bin/mwasmarm      bin/mwldarm      bin/skrewrap       (ELF)
    wine     Windows payload, driven through wine on macOS

The versioned payload binaries the launchers select with `-wrap:sdk <version>` keep the same
file name on every host (`bin/mwccarm-3.0-138.exe`, named by
`share/metroskrew/sdk/ds/<version>/mwccarm.exe.txt`), but their bytes are host-specific, so
neither the launcher path nor the payload digest is portable.

That is why a producer profile pins the toolchain per host: every toolchain section is a map
host -> value, and a producer reads the entry for the host it runs on. `mwccarm`/`mwasmarm`
recorded command lines do the same, because token[0] is a launcher path.

    profile = {
        "tool_sha256": {"windows": {...}, "linux": {...}},
        "C_base":      {"windows": [...], "linux": [...]},
    }

A section pinned for one host only is a hard error on the other host: silently pinning nothing
would let an unpinned compiler produce a guarded object.
"""

from __future__ import annotations

import os
import platform

SUPPORTED_HOSTS = ('windows', 'linux', 'wine')

BIN = 'tools/metroskrew/bin'


def host() -> str:
    """Host key, matching the Makefile's `SKREW_SYS`.

    MSYS2's python is a Cygwin-flavoured build: `os.name` is `posix` and `sys.platform` is
    `cygwin` even though the toolchain there is the Windows one, and its `stat` even resolves
    `bin/mwccarm` to `bin/mwccarm.exe`. `platform.system()` is the only reliable discriminator
    (`MSYS_NT-…`, `MINGW64_NT-…`, `CYGWIN_NT-…`, `Windows`, `Linux`, `Darwin`).
    """
    system = platform.system()
    if os.name == 'nt' or system == 'Windows' or system.startswith(('MSYS', 'MINGW', 'CYGWIN')):
        return 'windows'
    if system == 'Darwin':
        return 'wine'
    return 'linux'


HOST = host()


def exe_suffix() -> str:
    """.exe for the Windows payload; empty for the Linux one (mirrors common.mk's EXE)."""
    return '.exe' if HOST == 'windows' else ''


def launcher(name: str) -> str:
    """Repository-relative path of a launcher, e.g. `launcher('mwccarm')`."""
    return f'{BIN}/{name}{exe_suffix()}'


def repair(host_: str) -> str:
    return (f'this host ({host_}) has no pinned toolchain entry here; run '
            f'`python3 tools/devtools/reset_producer_pins.py --game <GAME>` on '
            f'{host_} and commit the section it adds')


def is_host_keyed(value: object) -> bool:
    """A host-keyed section maps host names to values; paths and token lists never do."""
    return isinstance(value, dict) and bool(value) and set(value) <= set(SUPPORTED_HOSTS)


def pick(value: object, where: str = ''):
    """Entry of a host-keyed section for the running host; nested sections included."""
    label = f'{where}: ' if where else ''
    if not is_host_keyed(value):
        raise SystemExit(f'{label}section must be keyed by host '
                         f'({", ".join(SUPPORTED_HOSTS)}); {repair(HOST)}')
    if HOST not in value:
        raise SystemExit(f'{label}section has no {HOST!r} entry; {repair(HOST)}')
    return value[HOST]


def section(profile: dict, key: str, where: str = ''):
    """Value of a host-keyed top-level profile section for the running host."""
    label = f'{where}:{key}' if where else key
    if key not in profile:
        raise SystemExit(f'{label}: profile section {key!r} is missing')
    return pick(profile[key], label)


def put_entry(owner: dict, key: str, value) -> None:
    """Store `value` for this host under owner[key], keeping the other hosts' entries."""
    current = owner.get(key)
    merged = dict(current) if is_host_keyed(current) else {}
    merged[HOST] = value
    owner[key] = {h: merged[h] for h in SUPPORTED_HOSTS if h in merged}


def put_section(profile: dict, key: str, value) -> None:
    """Merge one host entry into a host-keyed top-level section."""
    put_entry(profile, key, value)
