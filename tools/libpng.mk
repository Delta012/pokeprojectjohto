# Included by the host tools that link against libpng (nitrogfx, nitrorom).
#
# On MSYS2 libpng exists only in the MinGW runtimes, never in the MSYS one, and a MinGW compiler
# silently dies if MSYS's /usr/bin precedes its own bin directory in PATH (its cc1 then loads the
# wrong runtime DLLs). Point the compiler, pkg-config and PATH at the first MinGW runtime present.
# On Linux and in MSYS shells without a MinGW runtime installed, this file changes nothing.
MINGW_GCC := $(firstword $(wildcard /ucrt64/bin/gcc /mingw64/bin/gcc /clang64/bin/gcc))
ifneq ($(MINGW_GCC),)
    ifeq ($(origin CC),default)
        CC := $(MINGW_GCC)
    endif
    ifeq ($(origin CXX),default)
        CXX := $(MINGW_GCC)
    endif
    PKG_CONFIG_PATH := $(patsubst %/bin/gcc,%/lib/pkgconfig,$(MINGW_GCC))
    export PKG_CONFIG_PATH
    PATH := $(patsubst %/,%,$(dir $(MINGW_GCC))):$(PATH)
    export PATH
    # The MinGW runtime directories are not on PATH in the MSYS shell, so the resulting tools link
    # their runtime statically and stay runnable from whatever shell runs the build.
    LDFLAGS += -static
endif
