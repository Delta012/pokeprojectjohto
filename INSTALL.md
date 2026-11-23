# Installation

Build the US versions of Pokémon HeartGold and SoulSilver from source.

## macOS quick start

Clone this repository, enter its directory, then follow these steps.

With Homebrew and (on Apple Silicon) Rosetta 2 installed:

```sh
./scripts/setup-macos.sh
./scripts/build.sh progress
./scripts/build.sh GAME_VERSION=SOULSILVER progress
```

The setup helper installs native dependencies through Homebrew and a private Wine 11.16 inside
`.local-tools/`; its Wine prefix also stays in that ignored directory. It does not modify system
Wine settings and it installs no proprietary compiler: the CodeWarrior binaries come from the
metroskrew distribution, fetched by `make skrew` (which `scripts/build.sh` runs). Downloaded tools,
ROMs and build intermediates are ignored by Git.

The commands above perform matching builds and generate progress reports.
For Linux and Windows, follow the manual setup below.

## WSL quick start

Clone this repository into the WSL-native filesystem — not under `/mnt/<drive>`, which makes every
build syscall cross the OS boundary (see *Speed and hygiene in WSL*) — then install the same
dependency set as upstream pret/pokeheartgold and build:

```sh
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install bison flex g++ gcc-arm-none-eabi git make ninja-build pkg-config wget python3 xz-utils nasm libc6:i386 libpng-dev
make -j$(nproc)
```

`make skrew` downloads the native Linux CodeWarrior payload; no Wine, no manual compiler install.

## Manual setup

### 0. Clone the repository

Using a terminal or git client, clone this repository to your local device. All the steps that followed should be performed in the directory to which you cloned this repository.

### 1. Toolchain: metroskrew

The build needs Metrowerks CodeWarrior for ARM (`mwccarm`, `mwasmarm`, `mwldarm`) at the exact
versions pinned per source file. It is fetched automatically; no proprietary compiler archive and
no license file are involved:

```sh
make skrew
```

That runs `tools/devtools/get_metroskrew.sh`, which downloads the metroskrew 0.1.3 distribution
(the same one pret/pokeheartgold uses) into `tools/metroskrew`: the `windows` payload on MSYS2 and
native Windows, `linux` on Linux, `wine` on macOS. `common.mk` passes `-wrap:sdk <version>` to every
compiler, assembler and linker invocation, and metroskrew resolves that selector to the pinned
binary and the matching CodeWarrior runtime, so the ~140 per-object `MWCCVER :=` pins in the
makefiles keep working unchanged.

### 2. Link and pack tools

The proprietary NitroSDK host tools are not required. Each one is replaced by a tool built from
sources in `tools/`:

| Original proprietary input | Replaced by |
| :--- | :--- |
| `tools/bin/makelcf.exe` | `tools/makelcf` (open makelcf, vendored here) |
| `ARM9-TS.lcf.template`, `ARM7-TS.lcf.template` from SDK specfiles | `specfiles/ARM9-TS.lcf.template`, `specfiles/ichneumon_sub.lcf.template` |
| `mwldarm.response.template` | the CodeWarrior `FP_fullI` archive from the metroskrew payload (`MWLD_LIBS` in `common.mk`) |
| `makerom.exe` + `rom.rsf`, `makebanner.exe` + `banner.bsf` | `tools/nitrorom pack` driven by `<game>.us/rom.ini` and `<game>.us/filesystem.csv` |
| the `fixrom` post-step | header fields in `<game>.us/rom.ini` (`serial`, `secure-crc`) |

Run `make tools` to build them (it is also a prerequisite of the default target).

#### Producer profiles

Five producers (`tools/alarm-partition`, `tools/arm7-alarm-partition`, `tools/dwc-iterator-advance`,
`tools/msl-d1-partition`, `tools/msl-new-partition`) compile a small set of objects through their own
`build.py`, and each one refuses to run unless the compiler binary and the full command line match
the pins in its `profile.json`. Those pins name the metroskrew binaries (the launcher plus the
payload binary that `-wrap:sdk <version>` selects for that version) plus the command line make
exports, which carries `-wrap:sdk <version>` in place of the old per-version directory.

Toolchain pins are host-specific, so each toolchain section of a `profile.json` is keyed by host:

    "tool_sha256": {"windows": {"tools/metroskrew/bin/mwccarm.exe": …},
                    "linux":   {"tools/metroskrew/bin/mwccarm":      …}}

Windows installs PE launchers with a `.exe` suffix, Linux installs ELF launchers without one, and
the versioned payload binaries (`bin/mwccarm-3.0-138.exe`) keep the same file name on every host
while holding different bytes — so one flat pin set can never fit both hosts.
`tools/devtools/mw_host.py` resolves the section for the host running the producer and keys it on
`platform.system()`, because MSYS2's python reports `os.name == "posix"`, `sys.platform == "cygwin"`
and even resolves `bin/mwccarm` to `bin/mwccarm.exe`. A profile with no entry for the running host
aborts instead of compiling through an unpinned compiler.

Host keying is not only about file names. The alarm partitions let the compiler keep its
compile-time source directory in the object's debug info, so `os_alarm_before_handler.o` differs
between hosts by exactly that path (728 bytes of a 15284-byte object, `\home\…\lib\NitroSDK\src\os\`
against `E:\…\lib\NitroSDK\src\os\`); the msl partitions rewrite the recorded directory out of the
source text they emit, so `to91_msl_*.o` hash identically on both hosts. Neither difference reaches
the ROM — `main.sha1` passes either way — and the pins are what keep the code itself honest.

After a toolchain upgrade or a flag change in `common.mk` — and once per host, to add that host's
section — re-derive them from the makefiles:

    python3 tools/devtools/reset_producer_pins.py --game HEARTGOLD
    python3 tools/devtools/reset_producer_pins.py --game SOULSILVER

The helper rebuilds the generated inputs it has to digest (`make skrew`,
`lib/include/nitro/fx/fx_const.h`), so it also works straight after `make clean`. It only rewrites
the tool-side pins of the host it runs on, and it verifies the digests of every pinned source,
header and closure entry first, so a real source change still fails instead of being re-pinned away.

Three producer routes have no source in this tree and are in no link response, so nothing builds
them: `lib/NitroSDK/asm/to90_os_alarm_handler.s` (alarm handler role),
`sub/lib/asm/to90_arm7_alarm_handler.s` (arm7 alarm handler role) and
`lib/NitroDWC/src/to91_ov45_iterator_advance108.cpp`. The first two keep their inherited command
lines with only the tool path and `-wrap:sdk` swapped in; `tools/dwc-iterator-advance` is left
exactly as inherited, since its source is absent and its header pins no longer match this checkout.

### 3. Dependencies

#### Linux

The dependency set matches upstream pret/pokeheartgold. On Debian and derivatives (Ubuntu, Mint,
and WSL2's default Ubuntu), enable the 32-bit runtime first and install:

```bash
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install bison flex g++ gcc-arm-none-eabi git make ninja-build pkg-config wget python3 xz-utils nasm libc6:i386 libpng-dev
```

What each is for in this build stack: `bison`/`flex` build `tools/makelcf`; `gcc`/`g++` (pulled in
by `g++`) build the host tools under `tools/`; `gcc-arm-none-eabi` provides `arm-none-eabi-objcopy`
for the asset rules; `libpng-dev` (with zlib) links `tools/nitrogfx` and `tools/nitrorom`;
`python3` runs the producers and generators; `wget` and `xz-utils` fetch and unpack the Linux
metroskrew archive in `make skrew`; `libc6:i386` is required because the Linux CodeWarrior payload
is 32-bit x86 ELF. `ninja-build` and `nasm` carry over from upstream's tooling and are not used by
the makefile flow; keep them for upstream parity.

No Wine is needed: `make skrew` fetches the Linux metroskrew payload. `tools/msgenc` vendors its own
pugixml sources, so no system pugixml is required. On other distributions the package names vary;
the i386 runtime is the one that is easy to miss there (e.g. `glibc-i686`/`lib32-glibc` on Arch) —
without it every `mwccarm` invocation dies with a loader error like
`sh: .../bin/mwccarm: cannot execute: required file not found`.

#### Windows

The supported Windows environment is MSYS2 (`https://www.msys2.org`). Use the **MSYS** shell
(`C:\msys64\usr\bin\bash.exe -l`, or the "MSYS2 MSYS" shortcut) rather than the UCRT64/MINGW64
shells: the POSIX host tools (`tools/nitroarc` and friends) build against the MSYS runtime, while
the two tools that need libpng are built against a MinGW runtime automatically (see
`tools/libpng.mk`).

Install the packages:

```console
$ pacman -S make git gcc python pkgconf flex bison p7zip curl
$ pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-arm-none-eabi-binutils
```

`gcc`/`make`/`flex`/`bison` from the MSYS runtime build the POSIX host tools; the three UCRT64
packages provide libpng (MSYS2 has no MSYS-runtime libpng package), the compiler `tools/nitrorom`
and `tools/nitrogfx` link with, and `arm-none-eabi-objcopy` for the asset rules. `binutils.mk`
finds the cross binutils in `/ucrt64/bin`, `/mingw64/bin` or `/clang64/bin` when they are not on
`PATH`.

Cygwin and devkitARM are not part of the tested setup for this build stack.

#### macOS

macOS 10.15 Catalina and later is supported on Intel and ARM64 hardware configurations. On ARM64, Rosetta 2 must be installed, as well as the following dependencies:

* GNU coreutils
* GNU make
* GNU sed
* arm-none-eabi binutils (`arm-none-eabi-objcopy`)
* git
* libpng
* pkg-config
* Wine (installed privately by `./scripts/setup-macos.sh`; needed to run the x86 CodeWarrior binaries)

They can be installed with the following commands:

```console
$ brew install coreutils make gnu-sed arm-none-eabi-binutils libpng git pkgconf
$ ./scripts/setup-macos.sh
```

### 4. Build ROM

Run `make` to build the ROM. The ROM will be output as `build/heartgold.us/pokeheartgold.us.nds`

To build Pokemon SoulSilver, run `make soulsilver`. You do not need to clean your working tree in between compiling. Pokemon SoulSilver will be built as `build/soulsilver.us/pokesoulsilver.us.nds`.

There are targets for building and testing changes to individual components without repackaging the ROM. For the ARM9 modules, run `make main`. For the ARM7 module, run `make sub`. For the filesystem, run `make filesystem`. To build these for SoulSilver, append `GAME_VERSION=SOULSILVER` to the appropriate command.

At the end of building each of these, there is a checksum verification step. This makes sure that the final product is byte-for-byte equivalent to the retail ROM. To disable this, append `COMPARE=0` to your command.

#### After updating this repository

This repository is still in a volatile state, and several files may be moved around or renamed. If you update this repository and experience errors rebuilding, try the following troubleshooting steps, **one line at a time** until you get the non_npc_msg `build/heartgold.us/pokeheartgold.us.nds: OK`:

```shell
make tidy && make compare
make clean && make compare
```

If errors persist, report them in this repository.

## Ported build stack: what is verified

Verified on Windows 11 + MSYS2 (`/usr/bin` first in `PATH`), metroskrew 0.1.3 payload, no Wine. Every
gate in the repository passes, for both games — the ported stack reproduces both retail ROMs
byte-for-byte:

| Gate | Result |
| :--- | :--- |
| `sub/ichneumon_sub.sha1` (ARM7 autoload, both games) | **passes**; `sub/build/ichneumon_sub.sbin` is 161496 bytes and byte-identical to the reference `1d0b3418b85fa8b5e1a9e345d3a182073cb968ac` |
| `heartgold.us/filesystem.sha1` / `soulsilver.us/filesystem.sha1` (all 384 NitroFS entries) | **pass**; every narc/naix/message/script/arc strips with the open host tools |
| `scr_seq.sha1` | **pass**; 965/965 script sequences |
| `heartgold.us/main.sha1` / `soulsilver.us/main.sha1` | **pass**; 130/130 lines each game — the ARM9 root (1122052 bytes) and all 129 overlays are byte-identical to the reference |
| `heartgold.us/rom.sha1` | **pass**; `build/heartgold.us/pokeheartgold.us.nds` is 134217728 bytes, sha1 `4fcded0e2713dc03929845de631d0932ea2b5a37` |
| `soulsilver.us/rom.sha1` | **pass**; `build/soulsilver.us/pokesoulsilver.us.nds` is 134217728 bytes, sha1 `f8dc38ea20c17541a43b58c5e6d18c1732c7e582` |

`make` runs the checksum steps by default (`COMPARE ?= 1` in `common.mk`); `make COMPARE=0` skips
them, which is what made it possible to iterate on the link while `main.sha1` still failed.

`heartgold.us/rom.ini` and `soulsilver.us/rom.ini` are written the way `nitrorom pack` consumes
them: `nitrorom` resolves every path in the INI and in the manifest against its working directory,
so `make` runs it from `build/<game>.us/`, re-roots the manifest (`$(BUILD_DIR)/filesystem.pack.csv`,
written by `tools/devtools/pack_manifest.sh`) and passes the header template, banner icon and ARM7
image as `-D` substitutions. The manifest script also maps the archive names the pret manifest uses
for arcs whose ROM name is stripped to a serial (`/a/X/Y/Z`) onto `files/a/X/Y/Z`, which is where
`filesystem.mk:arc_strip_name` puts them; five HeartGold and six SoulSilver rows need that, and a
row that resolves to neither is a hard error naming the row. `nitrorom` also insists the ARM7 image
path end in `.nef`, so the ARM7 link result is published as `sub/build/ichneumon_sub.nef` for
packing (same ELF bytes).

Two link placements matter for the ARM9 root binary and are pinned in-tree rather than left to
command-line defaults: `Library FP_fastI_v5t_LE.a` must be declared *inside* `Static main` in
`main.lsf` (pret does the same) so the soft-float members land in `main` instead of in whichever
overlay first references them, and `specfiles/ARM9-TS.lcf.template` needs a
`<FOREACH.STATIC.OBJECTS=.version>` slot because this tree supplies the CodeWarrior `id_string`
block from the object `asm/middleware.o` rather than from vendor archives.

`specfiles/ARM9-TS.lcf.template` also carries three `KEEP_SECTION` blocks. Retail keeps
`SNDi_DecibelSquareTable` and `PitchTable` (0x100 + 0x600, carried as address-labelled data in
`lib/asm/nitro_sdk_data.s`) and `SNDi_DecibelTable` (0x100, the split section
`SND_util.o (.rodata.snd_decibel)`) only because one merged `.rodata` was pulled by the reference
to `sVolumeTable`; this tree splits those symbols into their own sections, so nothing references
the three and `mwldarm` would drop 0x800 bytes from the root image. `tools/nitro-section-renames/nitro_link_order.json`
then interleaves all five sections into retail's `0x0210D650..0x0210E124` block. `KEEP_SECTION`
takes one section name per block, hence the three blocks.

`makelcf` emits every `Object` selector of a section group before every `Library` selector, so the
position of `main.lsf`'s `Library` line cannot interleave archive members with objects. Retail puts
the PSL members (`FP_fastI_v5t_LE.a (.text)`, `0x020F068C`) between two objects, which is why
`tools/nitro-section-renames/patch_nitro_lcf.py` supports a `line_moves` list in
`nitro_link_order.json`: it moves one generated selector after another and fails if either appears
zero or several times. That single move puts the soft-float block at the reference address and drops
the differing-byte count by 13193.

Two more `KEEP_SECTION` blocks are load-bearing: `lib/asm/nitro_sdk_data.s` also carries retail `.bss`
as address-labelled gaps (`.bss.unified_00`, 4 bytes between `SND_main.o` and `SND_command.o`;
`.bss.unified_04`, 0x18 bytes at `0x021E4D48` inside retail's `libctrdg.a src_ctrdg.c.o`). Both appear
in the generated lcf, but nothing references their labels, so `mwldarm` discards them anyway — and
because overlays are placed with `ORIGIN = AFTER(main)` rather than at fixed addresses, losing `0x1C`
of `.bss` moves the whole overlay arena early, which mislinks every overlay image and every
`sOverlayTemplate_*` pointer in `src/launch_application.c`. With both kept, `heartgold.us/main.sha1`
and `soulsilver.us/main.sha1` pass: root and all 129 overlays byte-exact, both games.

`main.lsf` no longer lists `lib/MSL_C/src/msl_fp_init.o`. That file is an empty
`void _fp_init(void) {}` with `#pragma force_active on`, but `FP_fastI_v5t_LE.a ieee_status.o` already
defines `_fp_init` as a bare `bx lr` and is pulled for that reference — the retail map places it at
`0x020F32E4`, and so does ours. Linking both emits two `bx lr` words where retail has one, and those 4
bytes pushed *every* later section group 4 bytes late: removing the stub took the differing-byte count
from 71218 to 1310 and the misaligned-block count from 5437 to zero.

### Windows (MSYS2) and Linux (WSL) builds

One checkout builds on either host; what differs is the CodeWarrior distribution, so each host pins
its own payload (see *Producer profiles*):

    make skrew        # MSYS2: windows archive, bin/mwccarm.exe (PE)
    make skrew        # WSL:   linux archive,   bin/mwccarm (ELF)

`platform.mk` picks the archive through `SKREW_SYS` (`windows`/`linux`/`wine`), the makefiles name the
launcher as `$(SKREW_BIN)/mwccarm$(EXE)`, and the producers read their host's pin section through
`tools/devtools/mw_host.py`. `tools/metroskrew/` never carries over between hosts: `make clean` drops
it and `make` re-installs it for the host running the build, after which
`reset_producer_pins.py --game <GAME>` has to run once there to add that host's pin section.

Verified on Linux (WSL2 `Ubuntu-24.04`, GNU Make 4.3, gcc 13.3, Python 3.12, 32 jobs) from a tree
that had been through a full `make clean` and a fresh `make skrew` download: SoulSilver —
`sub/build/ichneumon_sub.sbin`, `soulsilver.us/filesystem.sha1` 384/384,
`soulsilver.us/main.sha1` 130/130, `soulsilver.us/rom.sha1` and `scr_seq.sha1` 965/965 all pass, and
`build/soulsilver.us/pokesoulsilver.us.nds` gets the same sha1 as the Windows build. The producer
objects (the C++ partition inputs) also hash identically on the two hosts, so the Windows and Linux
CodeWarrior builds emit the same bytes. Four things had
to become host-aware for that path: the launcher name (`bin/mwccarm`, no `.exe`, and different payload
bytes); the source-directory dialect the Linux compiler binaries record in DWARF (`\home\…\src\`,
backslashes with no drive), which the C++ producers pin; `WINPATH`, the depfile path converter
(needed only under Wine — and its selection had a shell precedence bug); and one wrong-case object
path in `main.lsf` (`lib/MSL_c/…`), which a case-sensitive filesystem rejects at link time.

#### Speed and hygiene in WSL

WSL2 reaches `/mnt/<drive>` through a 9P bridge: every `open`/`stat` is a cross-OS round trip, so a
tree that lives on NTFS makes this build I/O-bound no matter how big `-j` gets — the compile of a
single 400-line file costs milliseconds of CPU and milliseconds-per-syscall of everything else.
Measured on one 32-core WSL2 host with the same checkout in both places, `make -j32` throughout,
both runs ending in `pokeheartgold.us.nds: OK`:

    git status (16.5k files)                          /mnt/e: > 300 s    ext4: ~2 s
    touch src/battle/*.c (31 files), recompile,
      link, repack, sha1 check                        /mnt/e: 20 min     ext4: 23.5 s
    full clean build (build/ and sub/build/ removed)   ext4: ~31 min

Build from the WSL-native filesystem (`cp -a /mnt/e/<repo> ~/<repo>`, or clone there): commits,
shipped inputs, and downloaded toolchain payloads carry over unchanged; `build/` and `sub/build/`
are scratch and need not follow. Use `-j$(nproc)`, too: WSL2 exposes every host core (32 on the
measured box); `-j8` leaves 75 % idle.

There is deliberately no compile-cache story: ccache cannot wrap the CodeWarrior tools. Its
preprocessor-based hashing re-feeds `mwccarm`'s own `-E` output (the `#LinEdit` dialect, source
paths printed backslash-separated) into the real compile, which reproducibly corrupts the
translation unit — `function has no prototype` and `undefined identifier` on files that compile
clean unwrapped. Incremental `make` on ext4 (seconds for a handful of touched sources) is the
supported rebuild path.

Line endings: Git for Windows sets `core.autocrlf=true` in its *system* config, so text types not
listed with `eol=lf` in `.gitattributes` check out CRLF on the Windows side — and the shared tree
then fails in WSL at the first script it execs (`env: python3\r: No such file or directory`, exit
127) or at parsers reading `filesystem.csv`/`rom.ini`. The attribute list covers the build-input
types as of this writing; if CRLF still creeps into the worktree, the offenders are exactly:

    git ls-files --eol | awk '$1=="i/lf" && $2=="w/crlf"' | sed 's/.*\t//'

Rewrite them to LF (`perl -pi -e 's/\r\n/\n/g'`), *except* types pinned `eol=crlf` on purpose
(`charmap.txt`): convert those back with `rm` + `git checkout --`.

### Shipped arc payloads

Most `.narc` archives are packed from a directory of the same name by the generic rule in
`filesystem.mk` (`nitroarc -cf $@ --index-namespace $*`), so `make clean` may delete them and
`make` rebuilds them. 59 arcs are different: they have no input directory anywhere in the tree, so
no rule can produce them — `files/pbr/personal.narc`, `files/data/ug_*.narc`,
`files/poketool/pokegra/height.narc` and friends are shipped as
bytes, exactly like `heartgold.us/rom_header_template.sbin`. A fresh checkout that lacks them dies
with `nitroarc: could not stat input directory` and the filesystem gate stops at 265/384. The one
apparent exception is `files/arc/headbutt.<game>.narc`: it sits in `files/arc/` next to the others and
survives a Linux clean, but `files/arc/headbutt.mk` builds it (`jsonproc` → `mwasmarm` → `o2narc` from
`headbutt.json` + `headbutt.json.txt`), so it is a product, not an input.

They are also invisible to git by default: the root `.gitignore` has `*.narc`, and `files/.gitignore`
has `*.NCLR`. A negation in the root file cannot undo a pattern in a deeper one, so the exact-path
negations (`!/files/pbr/personal.narc`, `!/itemtool/itemdata/item_icon/item_icon_003.NCLR`, …) live in
the file that matches — 60 in `.gitignore`, 513 in `files/.gitignore`. With them in place a plain
`git add .` stages a complete, buildable tree. On a case-insensitive filesystem `*.NCLR` also swallows
the lowercase inputs `files/data/wm.nclr` and `files/data/pl_wm.nclr`, which `filesystem.mk:42,83` packs
and both `filesystem.sha1` lists digest, so they carry their own negations too. The invariant worth
re-checking after any `.gitignore` edit: every path named by a `*.sha1` list that *exists* in a cleaned
tree must stay visible to git —

    for L in heartgold.us/filesystem.sha1 scr_seq.sha1 heartgold.us/main.sha1 sub/ichneumon_sub.sha1; do
      awk '{print $2}' "$L" | sed 's/^[*]//'
    done | sort -u | while read -r p; do [ -f "$p" ] && printf '%s\n' "$p"; done \
      | git check-ignore --no-index --stdin        # must print nothing

The list they were generated from is kept in
`tools/devtools/shipped_inputs.txt` (571 entries: the 59 rule-less arcs, the arc leaves with no
source, the 7 `files/data/*.NCLR` covered by `filesystem.mk:561`'s empty rule, and the 2 lowercase
`wm.nclr` inputs);
`sh tools/devtools/add_shipped_inputs.sh` re-checks that every entry exists and stages it with
`git add -f` regardless of the ignore rules.

`files/msgdata/touch_once.sh`, which `files/msgdata/msg.mk` runs to stamp the first generated header,
is tracked with mode `100644`; `msg.mk:39` invokes it as `sh $(TOUCH_ONCE)` so no build depends on an
exec bit that no host reliably carries across a checkout.

The same file list is what distinguishes a *cleaned* tree from a *cloned* tree: after `make clean`
these 59 arcs are the only `.narc` files left, because the sweep only removes an arc whose input
directory is present.

### Clean rebuild

`make -j32 clean` costs the same as `make clean`: the three recursive phases (`tidy`,
`clean-filesystem`, `clean-tools`) are invoked with `MAKEFLAGS=` cleared, which withholds both the
parent's jobserver and the inherited `-j`, because the per-arc hooks overlap on scratch directories
and only ever work when serialised. A bare `-j1` on those sub-makes serialises them as well, but each
one then reports `warning: -j1 forced in submake: resetting jobserver mode`.

`make clean` (or `make clean_heartgold` / `make clean_soulsilver`) removes everything this tree can
regenerate: `build/` for both games, the host tools, the ARM7 tree, `lib/dsprot`, `lib/syscall`, the
generated `fx_const.h`, the NitroFS products — per-arc bins, objects, dep files, `.naix`,
`.diffarc`, and the 68 stripped serial copies under `files/a/`. It deliberately keeps the metroskrew
payload under `tools/metroskrew/`: a downloaded, host-specific toolchain archive that no rule in this
tree can produce, whose re-fetch costs a download, and which `tools/metroskrew/**` keeps out of git
anyway. It is dropped only on request, `make clean-skrew`.

The payload is host-specific, and building the same checkout from the other host swaps it for a
Linux one without complaint. The symptom is a failed compile with
`tools/metroskrew/bin/mwasmarm.exe: No such file or directory` while `tools/metroskrew/bin/` is full:
`file tools/metroskrew/bin/mwasmarm` says `ELF 32-bit LSB executable` instead of `PE32+`. Fix it with
`make clean-skrew && make skrew`, then `reset_producer_pins.py --game <GAME>` on the host you are
building on, because the producer profiles pin those binaries by digest.

It keeps only what no rule can rebuild: the 197 arc blobs under `files/a/` that this tree ships
without a rule for them, the two `rom_header_template.sbin` header blobs — `$(HEADER_TEMPLATE)`
has an empty rule, so a checkout without them cannot pack a ROM, which is why they are un-ignored in
`.gitignore` even though pret keeps them as tracked binaries — and the files listed in
`tools/devtools/shipped_inputs.txt` (see *Shipped arc payloads*). After a host switch, drop the
payload that belongs to the other host (`make clean-skrew`) and run `reset_producer_pins.py` once on
the new host to add its toolchain pin section (see *Producer
profiles*); `make clean` then `make` alone is otherwise the full cycle.

The per-arc `clean-<arc>` hooks under `files/` name only the bins and the `.narc` their mk declares,
so `clean-filesystem` now also runs `tools/devtools/clean_filesystem.sh`, which removes the rest:
graphics leaves and their `.lz` siblings that the generic `%.NCGR: %.png` / `%.NCLR: %.pal` /
`%.lz: %` rules write next to their source, the `.narc` of any arc that has a source directory
(including the other game's, which a per-game `NAIXS` list cannot cover), and the `.bin`/`.o`/`.c`/`.d`
intermediates the arc tools leave behind. The test for removability is that a rule can rebuild the
file, so each removal is driven off a source that exists in the tree; the script names the few tracked
binaries this tree ships even though their source is decompiled (`preview_graphic/*.NSCR`,
`camera_viewfinder.NSCR`) as exceptions. After `make clean`, `git status` lists no deleted tracked
file. Removals are batched — candidates go into one temporary list that a few `xargs` calls clear —
because naming each candidate to its own `rm` (about ten thousand spawns: 3403 `.png`/`.pal` sources
times four possible outputs, 2663 source stems, 1703 arc directories) spent 35 s of the 44 s that
`make clean` took on MSYS2, against 2.6 s on Linux. One class survives by construction: side products
whose names are not the stem of any source, such as
`files/application/zukanlist/zkn_data/zukan_hw_data/zukan_hw_data_1.bin`, which `csv2bin` writes next
to `zukan_hw_data.bin` and no rule names; a fresh clone lacks them and still reaches 384/384, so they
are inert.

Two upstream clean hooks assumed an author's working directory rather than a checkout.
`clean-tools` invoked `tools/asmdiff/asmdiff.sh -c` directly, but that helper is gitignored upstream
(`.gitignore` names `asmdiff.sh`), so on a fresh clone `make clean` died with "No such file or
directory" and left the host-tool objects behind; it now runs through `sh` and is skipped when the
helper is absent. And `tools/nitroarc/Makefile` compiles with `-MMD` but never listed the depfiles it
writes, so nine `.d` files survived every clean; its `clean` now names them. A third was
host-specific: `files/arc/headbutt.mk` removed its products as `$(HEADBUTT_DIR).$(bn).na{rc,ix}`, and
brace expansion is a bash feature — under `/bin/sh = dash` the pattern reaches `rm` literally, so a
Linux `make clean` never deleted the built `headbutt.<game>.narc` and `.naix` while a Windows one did.
It now names both extensions. A fourth was environmental: `tools/nitroarc/Makefile` removed only
`nitroarc$(EXE)`, and `EXE` is decided per make invocation by `platform.mk`'s
`ifeq ($(OS),Windows_NT)` — a sub-make does not inherit it — so a binary linked as `nitroarc` by a
shell that does not export `OS` survived a `make clean` from a shell that does. Its clean rule, and
`tools/mwasmarm_patcher`'s, now name both spellings, since `$(EXE)` is a per-invocation fact rather a
property of the tree.

`.naix` is deliberately *not* swept. The version-specific ones (`zukan_hw_data_gold.naix`,
`zukan_enc_silver.naix`, …) have no rule of their own — `filesystem.mk` declares `%.naix: %.narc`
with no recipe, and the file is a side product of `csv2bin`, which only writes it while rebuilding the
arc's bins (`CSV2BINFLAGS` carries `--naix`). Deleting one leaves a prerequisite make cannot make, and
the `sed 's/_gold//g'` rule in `files/application/zukanlist/zkn_data/zukan_hw_data.mk` then reads a
missing input; that is how the first version of the script broke an otherwise clean build.

`clean` runs its phases with `-j1` regardless of the `-j` on the command line: the per-arc hooks
overlap on scratch directories (`clean-pokegra` removes `files/poketool/pokegra/build-pokegra` with
`rm -r` while other hooks and the sweep script delete files inside it), so a parallel clean fails with
`rm: cannot remove '…/build-pokegra': Directory not empty`.

Build with `make -j$(nproc)`; `nproc` is coreutils, so it resolves in MSYS2 and Linux (the
compilers are small processes, 32 threads need no more memory than 8). Work one game at a time —
`GAME_VERSION=HEARTGOLD` while developing, and the `SOULSILVER` pass at the end: `build/` and the
generated products under `files/` are one shared space, so a second concurrent build in the same
checkout deletes and overwrites the first one's artifacts (and `make clean` wipes both games at
once, `tidy` removes the whole `build/` directory). Never run two builds in the same tree at the
same time. `sub filesystem` is the fast pair to re-run while iterating (10 s), `main` relinks the
ARM9 and the 129 overlays in about 30 s, and the ROM pack step takes about another 35 s.

A clean-tree rebuild reproduces both ROMs. `make -j$(nproc) GAME_VERSION=HEARTGOLD clean` then
`make -j$(nproc) GAME_VERSION=HEARTGOLD COMPARE=1 all`: 18 min 45 s to
`build/heartgold.us/pokeheartgold.us.nds` with `main.sha1` 130/130, `filesystem.sha1` 384/384,
`rom.sha1` OK, `sub` OK and `git status` clean (the same build at `-j8` takes 21 min 21 s; system
time exceeds user time here, so extra jobs buy about 12 %, not 4×). The `SOULSILVER` pass takes
about 23 min and reproduces `build/soulsilver.us/pokesoulsilver.us.nds` the same way.

## Progress reports

On Linux with the toolchain installed, run `make progress`. On macOS, use
`./scripts/build.sh progress`.

`progress` refreshes the selected ROM with `COMPARE=1`, checks the original
SHA1, and generates `build/progress/treemap.svg` and `progress.json`. The
standalone SVG opens in a browser and shows file details on hover. The JSON
contains every object, module, CPU, byte count, source path, and category.
The HeartGold and SoulSilver reports use the same output path by default;
choose separate output directories when retaining both:

```sh
python3 tools/progress/progress.py --game heartgold.us --output build/progress/heartgold
python3 tools/progress/progress.py --game soulsilver.us --output build/progress/soulsilver
```

To refresh the checked-in README snapshot after a matching HeartGold build:

```sh
python3 tools/progress/progress.py --output docs/progress
```

The GitHub build workflow is currently disabled on this fork to avoid automatic
CI runs. If re-enabled, it builds both versions and uploads a
`decompilation-progress` artifact with both reports. It does not upload ROMs
or proprietary toolchain files in that artifact. The README snapshot is a deliberate checked-in update, not an
automatically committed change on every CI run.

## What the chart measures

This is **source-language coverage by mapped code-section bytes**, not an
official upstream progress metric or a per-function matching score.

- Green: an object attributed to a C file without detected assembly syntax.
- Amber: a C object whose source contains assembly, including conditional
  nonmatching fallbacks. The entire object is excluded from the green total.
- Slate: an object attributed to assembly source.
- Purple: the source mapping is unresolved or ambiguous.

Rectangles are grouped by CPU and linker module, then sized by object bytes.
ARM7, ARM9, libraries, and all mapped overlays are included. Modules that load
at the same address remain separate. Aliases and enclosing section symbols
are merged by interval so their bytes are not counted twice. Sections `.text`,
`.init`, `.itcm`, `.sinit`, and `.wram` count as code; literal pools inside
these sections count too. Assets, other data sections, BSS, and padding with
no mapped symbol coverage are excluded.

The source mapping uses the linker specification to disambiguate C/assembly
files with the same basename. Generated protection objects and archive
members can remain unresolved. Source classification is conservative and
lexical; it is not a C preprocessor or proof of portable behavior. Assembly
needed for hardware or runtime operations is not necessarily work that
should be converted to C.

The generator verifies the ROM hash but cannot by itself establish that an
old build corresponds to newly edited source. Use `build.sh progress` to
refresh the build first. The report records the Git commit, tracked working
tree state, and map hashes for provenance. A passing ROM checksum establishes
retail binary equivalence, not complete decompilation or portability.
