## Common defines for ARM9 and ARM7 Makefiles ##

COMPARE ?= 1

default: all

PROJECT_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

ifeq ($(OS),Windows_NT)
REALPATH := realpath
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
REALPATH ?= grealpath
else
REALPATH := realpath
endif
endif

# Because mwldarm expects absolute paths to be WIN32 paths,
# all paths referring up from BUILD_DIR must be relative.
WORK_DIR   := $(shell $(REALPATH) --relative-to $(CURDIR) $(PROJECT_ROOT))
$(shell mkdir -p $(BUILD_DIR))
BACK_REL   := $(shell $(REALPATH) --relative-to $(BUILD_DIR) $(CURDIR))

# Recursive wildcard function
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

TOOLSDIR     := $(PROJECT_ROOT)/tools
TOOLSREL     := $(BACK_REL)/tools

include $(WORK_DIR)/platform.mk
include $(WORK_DIR)/binutils.mk

# Toolchain: metroskrew (pret/pokeheartgold's CodeWarrior-for-ARM distribution). The
# `-wrap:sdk <version>` selector picks the compiler/linker/assembler build plus the matching
# Metrowerks runtime set, so every per-object MWCCVER pin in the Makefiles keeps working with
# no proprietary CodeWarrior install, no license.dat and no Wine on Linux.
SKREWDIR      := $(TOOLSDIR)/metroskrew
SKREW_BIN     := $(SKREWDIR)/bin
SKREW_SDK     := $(SKREWDIR)/lib/metroskrew/sdk/ds

MWCC          = $(SKREW_BIN)/mwccarm$(EXE)
MWAS          = $(SKREW_BIN)/mwasmarm$(EXE)
MWLD          = $(SKREW_BIN)/mwldarm$(EXE)
# Recursive on purpose: the per-object `MWCCVER :=` pins in the Makefiles must reach the wrapper
# selector, and the partition producers (tools/*/build.py) receive $(MWCFLAGS) verbatim.
MWWRAP        = -wrap:sdk $(MWCCVER)

# Open reimplementations from pret/pokeheartgold: nitrorom replaces makerom.exe/makebanner.exe,
# makelcf replaces makelcf.exe. ntrcomp.exe was never used by these makefiles.
MAKEROM      := $(TOOLSDIR)/nitrorom/nitrorom$(EXE)
MAKELCF      := $(TOOLSDIR)/makelcf/makelcf$(EXE)
MAKEBNR      :=
NTRCOMP      :=

# metroskrew runs the toolchain natively on Linux; the macOS build is the Wine one.
ifneq ($(OS),Windows_NT)
  ifneq ($(UNAME_S),Darwin)
    WINE :=
  endif
endif

# Native tools
JSONPROC     := $(TOOLSDIR)/jsonproc/jsonproc$(EXE)
GFX          := $(TOOLSDIR)/nitrogfx/nitrogfx$(EXE)
FIXROM       := $(TOOLSDIR)/fixrom/fixrom$(EXE)
NARC         := $(TOOLSDIR)/nitroarc/nitroarc$(EXE)
O2NARC       := $(TOOLSDIR)/o2narc/o2narc$(EXE)
MSGENC       := $(TOOLSDIR)/msgenc/msgenc$(EXE)
ASPATCH      := $(TOOLSDIR)/mwasmarm_patcher/mwasmarm_patcher$(EXE)
CSV2BIN      := $(TOOLSDIR)/csv2bin/csv2bin$(EXE)
MKFXCONST    := $(TOOLSDIR)/gen_fx_consts/gen_fx_consts$(EXE)
ELFCODER     := $(TOOLSDIR)/elfcoder/elfcoder$(EXE)

# Native libraries
LIBNARC      := $(TOOLSDIR)/nitroarc/libnitroarc.a

# Decompiled NitroSDK tools
COMPSTATIC   := $(TOOLSDIR)/compstatic/compstatic$(EXE)

NTRMERGE      := $(TOOLSDIR)/ntr_merge_elf/ntr_merge_elf.sh
ASM_PROCESSOR := $(TOOLSDIR)/asm_processor/compile.sh

NATIVE_TOOLS := \
	$(JSONPROC) \
	$(GFX) \
	$(FIXROM) \
	$(NARC) \
	$(O2NARC) \
	$(MSGENC) \
	$(ASPATCH) \
	$(CSV2BIN) \
	$(MKFXCONST) \
	$(COMPSTATIC) \
	$(ELFCODER) \
	$(MAKEROM) \
	$(MAKELCF)

TOOLDIRS := $(foreach tool,$(NATIVE_TOOLS),$(dir $(tool)))

# Directories
NITROSDK_SRC_SUBDIRS      := os mi fx gx card cp ctrdg fs math pxi rtc snd mb

LIB_SUBDIRS               := cw NitroSDK NitroSystem NitroDWC NitroWiFi libCPS libVCT MSL_C
SRC_SUBDIR                := src
ASM_SUBDIR                := asm
LIB_SRC_SUBDIR            := lib/src $(LIB_SUBDIRS:%=lib/%/src) $(NITROSDK_SRC_SUBDIRS:%=lib/NitroSDK/src/%)
LIB_ASM_SUBDIR            := lib/asm $(LIB_SUBDIRS:%=lib/%/asm)
ALL_SUBDIRS               := $(SRC_SUBDIR) $(ASM_SUBDIR) $(LIB_SRC_SUBDIR) $(LIB_ASM_SUBDIR)

SRC_BUILDDIR              := $(addprefix $(BUILD_DIR)/,$(SRC_SUBDIR))
ASM_BUILDDIR              := $(addprefix $(BUILD_DIR)/,$(ASM_SUBDIR))
LIB_SRC_BUILDDIR          := $(addprefix $(BUILD_DIR)/,$(LIB_SRC_SUBDIR))
LIB_ASM_BUILDDIR          := $(addprefix $(BUILD_DIR)/,$(LIB_ASM_SUBDIR))

C_SRCS                    := $(call rwildcard,src,*.c)
CXX_SRCS                  := $(call rwildcard,src,*.cpp)
ASM_SRCS                  := $(foreach dname,$(ASM_SUBDIR),$(wildcard $(dname)/*.s))
GLOBAL_ASM_SRCS           != grep -rl 'GLOBAL_ASM(' $(C_SRCS)
LIB_C_SRCS                := $(foreach dname,$(LIB_SRC_SUBDIR),$(wildcard $(dname)/*.c))
LIB_CXX_SRCS              := $(foreach dname,$(LIB_SRC_SUBDIR),$(wildcard $(dname)/*.cpp))
LIB_ASM_SRCS              := $(foreach dname,$(LIB_ASM_SUBDIR),$(wildcard $(dname)/*.s))
ALL_SRCS                  := $(C_SRCS) $(CXX_SRCS) $(ASM_SRCS) $(GLOBAL_ASM_SRCS) $(LIB_C_SRCS) $(LIB_CXX_SRCS) $(LIB_ASM_SRCS)

C_OBJS                    = $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
CXX_OBJS                  = $(CXX_SRCS:%.cpp=$(BUILD_DIR)/%.o)
ASM_OBJS                  = $(ASM_SRCS:%.s=$(BUILD_DIR)/%.o)
GLOBAL_ASM_OBJS           = $(GLOBAL_ASM_SRCS:%.c=$(BUILD_DIR)/%.o)
LIB_C_OBJS                = $(LIB_C_SRCS:%.c=$(BUILD_DIR)/%.o)
LIB_CXX_OBJS              = $(LIB_CXX_SRCS:%.cpp=$(BUILD_DIR)/%.o)
LIB_ASM_OBJS              = $(LIB_ASM_SRCS:%.s=$(BUILD_DIR)/%.o)
ALL_GAME_OBJS             = $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS) $(GLOBAL_ASM_OBJS)
ifeq ($(PROC),arm946e)
LIB_ASM_OBJS += $(BUILD_DIR)/lib/NitroSDK/asm/os_exception_work.o $(BUILD_DIR)/lib/NitroSDK/asm/os_exception_handler.o
endif
ALL_LIB_OBJS              = $(LIB_C_OBJS) $(LIB_CXX_OBJS) $(LIB_ASM_OBJS)
ALL_OBJS                  = $(ALL_GAME_OBJS) $(ALL_LIB_OBJS)

$(ALL_LIB_OBJS): DEFINES = $(GLB_DEFINES)

ALL_BUILDDIRS             := $(sort $(ALL_BUILDDIRS) $(foreach obj,$(ALL_OBJS),$(dir $(obj))))

ELF               := $(BUILD_DIR)/$(ELFNAME).elf
LCF               := $(ELF:%.elf=%.lcf)
RESPONSE          := $(ELF:%.elf=%.response)
SBIN              := $(ELF:%.elf=%.sbin)
XMAP              := $(ELF).xMAP

EXCCFLAGS         := -Cpp_exceptions off

MWCFLAGS           = $(MWWRAP) $(DEFINES) $(OPTFLAGS) -sym on -enum int -lang c99 $(EXCCFLAGS) -gccext,on -proc $(PROC) -msgstyle gcc -gccinc -i ./src -i ./include -i ./include/library -i $(WORK_DIR)/files -I$(WORK_DIR)/lib/include -i $(WORK_DIR)/lib/NitroSDK/include -i $(WORK_DIR)/lib/NitroSDK/include-mw -i $(WORK_DIR)/lib/NitroSDK/include-pokediamond -ipa file -interworking -inline on,noauto -char signed -W all -W pedantic -W noimpl_signedunsigned -W noimplicitconv -W nounusedarg -W nomissingreturn -W error

MWASFLAGS          = $(MWWRAP) $(DEFINES) -proc $(PROC_S) -g -gccinc -i . -i ./include -i $(WORK_DIR)/asm/include -i $(WORK_DIR)/files -i $(WORK_DIR)/lib/asm/include -i $(WORK_DIR)/lib/NitroDWC/asm/include -i $(WORK_DIR)/lib/MSL_C/asm/include -i $(WORK_DIR)/lib/NitroSDK/asm/include -i $(WORK_DIR)/lib/syscall/asm/include -i $(WORK_DIR)/asm -i $(WORK_DIR)/files/msgdata -I$(WORK_DIR)/lib/include -DSDK_ASM
MWLDFLAGS         := -proc $(PROC) -sym on -nopic -nopid -interworking -map closure,unused -symtab sort -m _start -msgstyle gcc
ARFLAGS           := rcS

MW_COMPILE = $(WINE) $(MWCC) $(MWCFLAGS)
MW_ASSEMBLE = $(WINE) $(MWAS) $(MWASFLAGS)

# CodeWarrior runtime archives that used to be pulled in through the SDK's
# mwldarm.response.template. metroskrew ships the same archives, keyed by the runtime set that
# its version map resolves for MWCCVER (ds/2.0/sp2p2 reuses the ds/2.0/sp2 runtime, for example).
SKREW_SDKMAP := $(SKREWDIR)/share/metroskrew/sdk/ds
SKREW_LIBVER := $(shell sed -e 's|^ds/||' $(SKREW_SDKMAP)/$(MWCCVER)/lib.txt 2>/dev/null)
SKREW_LIBVER := $(if $(SKREW_LIBVER),$(SKREW_LIBVER),$(MWCCVER))
# The linker takes -proc v5te while the runtime archives are named v5t (and v4t).
FP_PROC      := $(if $(filter v5te,$(PROC_LD)),v5t,$(PROC_LD))
# The ARM9 links the fastI runtime (as its link map shows); the ARM7 autoload takes fullI.
MWLD_FP      ?= fastI
MWLD_LIBS    := $(wildcard $(SKREWDIR)/lib/metroskrew/sdk/ds/$(SKREW_LIBVER)/Mathlib/lib/FP_$(MWLD_FP)_$(FP_PROC)_LE.a)

export MWCIncludes := lib/include

LSF               := $(addsuffix .lsf,$(ELFNAME))
ifneq ($(LSF),)
OVERLAYS          := $(shell $(GREP) -o "^Overlay \w+" $(LSF) | cut -d' ' -f2)
else
OVERLAYS          :=
endif

# Make sure build directories exist before compiling anything
DUMMY := $(shell mkdir -p $(ALL_BUILDDIRS))

.SECONDARY:
.SECONDEXPANSION:
.DELETE_ON_ERROR:
.PHONY: all tidy clean tools clean-tools patch_mwasmarm $(TOOLDIRS)
.PRECIOUS: $(SBIN)

# metroskrew ships assembler builds that are already patched/relinked; the byte patches in
# tools/mwasmarm_patcher target the original PE binaries and must not be applied to them.
patch_mwasmarm:
	@:

$(BUILD_DIR)/lib/NitroSDK/%.o: private MWCCVER := 2.0/sp2p3
$(BUILD_DIR)/lib/MSL_C/%.o: private MWCCVER := 2.0/sp2p3

ifeq ($(NODEP),)
ifneq ($(WINPATH),)
PROJECT_ROOT_NT := $(shell $(WINPATH) -w $(PROJECT_ROOT) | $(SED) 's/\\/\//g')
define fixdep
$(SED) -i 's/\r//g; s/\\/\//g; s/\/$$/\\/g; s#$(PROJECT_ROOT_NT)#$(PROJECT_ROOT)#g' $(1)
touch -r $(1:%.d=%.o) $(1)
endef
else
define fixdep
$(SED) -i 's/\r//g; s/\\/\//g; s/\/$$/\\/g' $(1)
touch -r $(1:%.d=%.o) $(1)
endef
endif
DEPFLAGS := -gccdep -MD
DEPFILES := $(ALL_OBJS:%.o=%.d)
MW_COMPILE += $(DEPFLAGS)
$(GLOBAL_ASM_OBJS): private BUILD_C := $(ASM_PROCESSOR) "$(MW_COMPILE)" "$(MW_ASSEMBLE)"
BUILD_C ?= $(MW_COMPILE) -c -o
BUILD_CPP ?= $(subst -lang c99,-lang c++,$(MW_COMPILE)) -c -o

$(DEPFILES):

ifeq ($(PROC),arm946e)

# Derive retained native exception groups from the unchanged SDK producer.
$(BUILD_DIR)/lib/NitroSDK/asm/os_exception_work.o: $(BUILD_DIR)/lib/NitroSDK/src/os/os_exception.o tools/nitro-section-renames/partition_os_exception.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/nitro-section-renames/partition_os_exception.py $< $@ anchor
$(BUILD_DIR)/lib/NitroSDK/asm/os_exception_handler.o: $(BUILD_DIR)/lib/NitroSDK/src/os/os_exception.o tools/nitro-section-renames/partition_os_exception.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/nitro-section-renames/partition_os_exception.py $< $@ residual
endif
$(BUILD_DIR)/lib/NitroSDK/src/os/os_thread.o: lib/NitroSDK/src/os/os_thread.os-thread-section-map.json tools/nitro-section-renames/rename_os_thread_sections.py
$(BUILD_DIR)/lib/NitroSDK/src/os/os_reset_core.o: lib/NitroSDK/src/os/os_reset_core.os-reset-section-map.json tools/nitro-section-renames/rename_os_reset_sections.py

$(BUILD_DIR)/%.o: %.c
$(BUILD_DIR)/%.o: %.c $(BUILD_DIR)/%.d
	@echo $(BUILD_C) $@ $<
	@$(BUILD_C) $@ $< || { rm -f $(BUILD_DIR)/%.d; exit 1; }
	@if test -f "$(basename $<).snd-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_snd_sections.py "$@" "$@" "$(basename $<).snd-section-map.json"; fi
	@if test -f "$(basename $<).card-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_card_sections.py "$@" "$@" "$(basename $<).card-section-map.json"; fi
	@if test -f "$(basename $<).os-thread-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_thread_sections.py "$@" "$@" "$(basename $<).os-thread-section-map.json"; fi
	@if test -f "$(basename $<).os-reset-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_reset_sections.py "$@" "$@" "$(basename $<).os-reset-section-map.json"; fi
	@if test -f "$(basename $<).alias-map.json"; then $(PYTHON) $(WORK_DIR)/tools/object_aliases.py "$@" "$(basename $<).alias-map.json"; fi
	@$(call fixdep,$(BUILD_DIR)/$*.d)

$(BUILD_DIR)/%.o: %.cpp
$(BUILD_DIR)/%.o: %.cpp $(BUILD_DIR)/%.d
	@echo $(BUILD_CPP) $@ $<
	@$(BUILD_CPP) $@ $< || { rm -f $(BUILD_DIR)/%.d; exit 1; }
	@if test -f "$(basename $<).snd-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_snd_sections.py "$@" "$@" "$(basename $<).snd-section-map.json"; fi
	@if test -f "$(basename $<).card-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_card_sections.py "$@" "$@" "$(basename $<).card-section-map.json"; fi
	@if test -f "$(basename $<).os-thread-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_thread_sections.py "$@" "$@" "$(basename $<).os-thread-section-map.json"; fi
	@if test -f "$(basename $<).os-reset-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_reset_sections.py "$@" "$@" "$(basename $<).os-reset-section-map.json"; fi
	@if test -f "$(basename $<).alias-map.json"; then $(PYTHON) $(WORK_DIR)/tools/object_aliases.py "$@" "$(basename $<).alias-map.json"; fi
	@$(call fixdep,$(BUILD_DIR)/$*.d)

$(BUILD_DIR)/%.o: %.s
$(BUILD_DIR)/%.o: %.s $(BUILD_DIR)/%.d
	@echo $(WINE) $(MWAS) $(MWASFLAGS) $(DEPFLAGS) -o $@ $<
	@$(WINE) $(MWAS) $(MWASFLAGS) $(DEPFLAGS) -o $@ $< || { rm -f $(BUILD_DIR)/%.d; exit 1; }
	@$(call fixdep,$(BUILD_DIR)/$*.d)

include $(wildcard $(DEPFILES))
else
$(GLOBAL_ASM_OBJS): private BUILD_C := $(ASM_PROCESSOR) "$(MW_COMPILE)" "$(MW_ASSEMBLE)"
BUILD_C ?= $(MW_COMPILE) -c -o
BUILD_CPP ?= $(subst -lang c99,-lang c++,$(MW_COMPILE)) -c -o

$(BUILD_DIR)/%.o: %.c
	$(BUILD_C) $@ $<
	if test -f "$(basename $<).snd-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_snd_sections.py "$@" "$@" "$(basename $<).snd-section-map.json"; fi
	if test -f "$(basename $<).card-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_card_sections.py "$@" "$@" "$(basename $<).card-section-map.json"; fi
	if test -f "$(basename $<).os-thread-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_thread_sections.py "$@" "$@" "$(basename $<).os-thread-section-map.json"; fi
	if test -f "$(basename $<).os-reset-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_reset_sections.py "$@" "$@" "$(basename $<).os-reset-section-map.json"; fi
	if test -f "$(basename $<).alias-map.json"; then $(PYTHON) $(WORK_DIR)/tools/object_aliases.py "$@" "$(basename $<).alias-map.json"; fi

$(BUILD_DIR)/%.o: %.cpp
	$(BUILD_CPP) $@ $<
	if test -f "$(basename $<).snd-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_snd_sections.py "$@" "$@" "$(basename $<).snd-section-map.json"; fi
	if test -f "$(basename $<).card-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_card_sections.py "$@" "$@" "$(basename $<).card-section-map.json"; fi
	if test -f "$(basename $<).os-thread-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_thread_sections.py "$@" "$@" "$(basename $<).os-thread-section-map.json"; fi
	if test -f "$(basename $<).os-reset-section-map.json"; then $(PYTHON) tools/nitro-section-renames/rename_os_reset_sections.py "$@" "$@" "$(basename $<).os-reset-section-map.json"; fi
	if test -f "$(basename $<).alias-map.json"; then $(PYTHON) $(WORK_DIR)/tools/object_aliases.py "$@" "$(basename $<).alias-map.json"; fi

$(BUILD_DIR)/%.o: %.s
	$(WINE) $(MWAS) $(MWASFLAGS) -o $@ $<
endif

$(NATIVE_TOOLS): tools

tools: $(TOOLDIRS)

$(TOOLDIRS):
	@$(MAKE) -C $@

clean-tools:
	$(foreach tool,$(TOOLDIRS),$(MAKE) -C $(tool) clean;)
	@test ! -f "$(TOOLSDIR)/asmdiff/asmdiff.sh" || sh "$(TOOLSDIR)/asmdiff/asmdiff.sh" -c

ifeq ($(PROC),arm946e)
$(LCF): tools/nitro-section-renames/nitro_link_order.json tools/nitro-section-renames/patch_nitro_lcf.py
$(LCF): tools/nitro-section-renames/patch_os_thread_lcf.py lib/NitroSDK/src/os/os_thread.os-thread-section-map.json
$(LCF): tools/nitro-section-renames/patch_os_reset_lcf.py lib/NitroSDK/src/os/os_reset_core.os-reset-section-map.json
endif

$(LCF): $(LSF) $(LCF_TEMPLATE) $(MAKEFILE_LIST)
	$(WINE) $(MAKELCF) $(MAKELCF_FLAGS) $(LSF) $(LCF_TEMPLATE) $@
# The ARM7 side needs no post-pass: the linker command file already computes
# SDK_SUBPRIV_ARENA_LO. The fork used to append its own assignment here because the proprietary
# SDK's ARM7 template omitted that line; with the template kept here it would be assigned twice.
ifeq ($(PROC),arm946e)
	$(SED) -i '1i KEEP_SECTION\n{\n\t.exceptix\n}' $@
	python3 tools/nitro-section-renames/patch_nitro_lcf.py $@
	python3 tools/nitro-section-renames/patch_os_thread_lcf.py $@
	python3 tools/nitro-section-renames/patch_os_reset_lcf.py $@
endif

# mwldarm loads its inputs from an options file: the linker command file places sections by
# object basename, so every object it names has to be loaded too. The link response is the
# authority on which objects take part: this tree also builds variants the response leaves out
# (os_exception.o next to lib/NitroSDK/asm/os_exception_handler.o, os_alarm.o, os_thread.compiler.o,
# the .producer/.compiler intermediates), and loading those makes mwldarm abort on multiply-defined
# symbols. The proprietary SDK generated this list from mwldarm.response.template, which is not in
# this tree; it is regenerated from the response itself instead. Order is kept as written.
# The response lists objects both bare (`Objects lib/…/x.o`) and section-qualified
# (`Object lib/…/x.o (.itcm)`); the qualifier only says which section to place, the object still
# has to be loaded, so the first token of every line is the input list, in order, without repeats.
# Empty $(LSF) would leave sed with no file argument: it would inherit make's stdin
# and block forever wherever that stdin never reaches EOF (interactive pts, open
# supervisor pipes). Guard the expansion instead; hosts where stdin was /dev/null
# happened to see an empty list, which is exactly the guarded result.
LSF_OBJECTS      := $(if $(LSF),$(shell sed -n 's/^[[:space:]]*[Oo][Bb][Jj][Ee][Cc][Tt][sS]*[[:space:]]\{1,\}\([^[:space:]]\{1,\}\).*/\1/p' $(LSF) | awk '!seen[$$0]++'))
RESPONSE_OBJECTS  = $(filter-out $(CRT0_OBJ),$(LSF_OBJECTS))

$(RESPONSE): $(LSF) $(MAKEFILE_LIST)
	@$(file >$@,)
	@$(foreach obj,$(RESPONSE_OBJECTS),$(file >>$@,$(obj)))

# The proprietary SDK supplied mwldarm.response.template (link-time library list). metroskrew
# ships the same CodeWarrior runtime archives, so the archives are named here instead.
MWLD_EXTRA_ARGS ?=

# Locate crt0.o (the ARM7 override lives in sub/Makefile). It is passed to the link explicitly,
# so it is kept out of the options file, and the linker command file names it by basename.
CRT0_OBJ := lib/NitroSDK/src/crt0.o

.INTERMEDIATE: $(BUILD_DIR)/obj.list

$(SBIN): build/%.sbin: build/%.elf
ifeq ($(COMPARE),1)
	$(SHA1SUM) --quiet -c $*.sha1
endif

$(ELF): $(ALL_OBJS) $(LCF) $(RESPONSE)
	$(MAKE) $(LCF)
	cd $(BUILD_DIR) && $(WINE) $(MWLD) $(MWWRAP) $(MWLDFLAGS) $(LIBS) $(MWLD_EXTRA_ARGS) $(MWLD_LIBS) -o $(BACK_REL)/$(ELF) $(LCF:$(BUILD_DIR)/%=%) @$(RESPONSE:$(BUILD_DIR)/%=%) $(CRT0_OBJ)
#	$(NTRMERGE) $*

print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true
