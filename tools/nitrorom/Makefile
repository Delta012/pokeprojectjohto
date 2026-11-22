-include ../libpng.mk

ifeq ($(origin CC),default)
CC := gcc
endif
PKG_CONFIG ?= pkg-config

prog := nitrorom

CFLAGS := -O3 -std=gnu17 -w -Wno-unused-result -Wno-stringop-overflow -Iinclude
CFLAGS += $(shell $(PKG_CONFIG) --cflags libpng zlib)
LDLIBS += $(shell $(PKG_CONFIG) --libs libpng zlib)

SRCS = $(wildcard src/*.c) $(wildcard src/libs/*.c) $(wildcard src/parse/*.c)
OBJS = $(SRCS:%.c=%.o)

.PHONY: all clean

all: $(prog)
	@:

clean:
	$(RM) $(OBJS) $(prog) $(prog).exe

$(prog): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
