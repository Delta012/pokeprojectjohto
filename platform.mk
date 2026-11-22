
WSLENV ?= no
ifeq ($(WSLENV),no)
  NOWINE = 0
else
  # As of build 17063, WSLENV is defined in both WSL1 and WSL2
  # so we need to use the kernel release to detect between
  # the two.
  UNAME_R := $(shell uname -r)
  ifeq ($(findstring WSL2,$(UNAME_R)),)
    NOWINE = 1
  else
    NOWINE = 0
  endif
endif

ifeq ($(OS),Windows_NT)
  EXE := .exe
  WINE :=
  GREP := grep -P
  SED := sed -r
  SHA1SUM := sha1sum
  MKTEMP := mktemp
else
  EXE :=
  WINE := wine
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    GREP := grep -E
    SED := gsed -r
    SHA1SUM := shasum
    MKTEMP := gmktemp
  else
    GREP := grep -P
    SED := sed -r
    SHA1SUM := sha1sum
    MKTEMP := mktemp
  endif
endif

ifeq ($(NOWINE),1)
  WINE :=
endif
# Depfile path converter. Only needed when the toolchain runs under Wine (macOS, or a WSL session
# using the Windows payload): the depfiles then hold Windows paths that have to be rewritten to the
# checkout's own paths. A native build needs none — MSYS2 feeds MSYS-style paths straight to the
# Windows tools, and a Linux build (WSL included) runs the Linux payload, whose depfiles are already
# POSIX. The old one-liner also had a shell precedence bug (`a && b || c && d` runs `d` too), which
# made `WINPATH` expand to two converters on any host that has both installed.
ifeq ($(WINE),)
  WINPATH :=
else
  WINPATH := $(shell if command -v wslpath >/dev/null 2>&1; then echo wslpath; elif command -v winepath >/dev/null 2>&1; then echo winepath; fi)
endif
