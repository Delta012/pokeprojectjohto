CC := gcc
BISON := bison
FLEX := flex

prog := makelcf

CFLAGS := -O3 -std=gnu11 -w -DSDK_WIN32 -Isdk-include

PARSER_SRCS = tlcf.tab.c tlcf.yy.c spec.tab.c spec.yy.c
SRCS = container.c createlcf.c defval.c makelcf.c misc.c $(PARSER_SRCS)
OBJS = $(SRCS:%.c=%.o)

.PHONY: all clean

all: $(prog)
	@:

clean:
	$(RM) $(OBJS) $(prog) $(prog).exe tlcf.tab.c tlcf.tab.h spec.tab.c spec.tab.h tlcf.yy.c spec.yy.c

$(prog): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.tab.c %.tab.h: %.y
	$(BISON) -d -p $*_yy -o $*.tab.c $<

%.yy.c: %.l
	$(FLEX) --nowarn -P$*_yy -o $@ $<

tlcf.tab.o: tlcf.tab.h
spec.tab.o: spec.tab.h
createlcf.o: tlcf.tab.h spec.tab.h
