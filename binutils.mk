# Supply a GNU ARM Embedded toolchain for objcopy, ar, etc.

ifdef DEVKITARM
TOOLCHAIN := $(DEVKITARM)
endif

ifdef TOOLCHAIN
export PATH := $(TOOLCHAIN)/bin:$(PATH)
endif

PREFIX := arm-none-eabi-

# The cross binutils are not part of the MSYS runtime; on MSYS2 they are installed into a MinGW
# runtime directory that the MSYS shell does not have on PATH. Resolve them by lookup instead.
CROSS_BINDIRS := /ucrt64/bin /mingw64/bin /clang64/bin
find_cross_tool = $(firstword $(shell command -v $(1) 2>/dev/null) $(firstword $(wildcard $(addsuffix /$(1).exe,$(CROSS_BINDIRS)))))

OBJCOPY := $(call find_cross_tool,$(PREFIX)objcopy)
AR      := $(call find_cross_tool,$(PREFIX)ar)

# tools/object_aliases.py and the section-renaming helpers look the cross binutils up by name on
# PATH. The directory is appended, never prepended: the MSYS host tools must keep resolving the
# MSYS runtime first, otherwise their cc1 loads the wrong DLLs and dies without a message.
ifneq ($(OBJCOPY),)
CROSS_BINDIR := $(patsubst %/,%,$(dir $(OBJCOPY)))
export PATH := $(PATH):$(CROSS_BINDIR)
endif
