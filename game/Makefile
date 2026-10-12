# Builds rogue.lst, the game in the listing format the emulators load.
# Needs the pdp11-aout cross compiler (see build-toolchain.sh) on PATH.

PREFIX  ?= pdp11-aout-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

# -m40: an 11/40-class CPU, which has the EIS instructions (MUL, DIV, ASH,
# ASHC) like the emulated 11/03 with EIS, and no floating point unit.
# -fno-bit-tests: GCC 14.2's PDP-11 code for a comparison against a set of
# characters done as a bit-mask test is wrong.  For
#     c == '.' || c == '#' || c == '+' || c == '>'
# it shifts the mask left instead of right when c is '#', so '#' tests
# false; inlined into the game, all but '>' did.  hosttest.py checks the
# game against a host build to catch anything like it.
CFLAGS  = -O2 -fno-bit-tests -m40 -msoft-float -ffreestanding -Wall -Wextra
# Load at 0o1000 (0x200), above the trap vectors; -N puts the data right
# after the code, so the image is one contiguous block.
LDFLAGS = -nostdlib -Wl,-N,-Ttext,0x200,-e,start
LOAD    = 01000

# Instructions the emulators do not implement (they stop on them).
MISSING = setd|seti|setf|setl|cfcc|ldfps|stfps|stst|ldf|stf|addf|subf|mulf|divf|cmpf|tstf|absf|negf|ldcif|stcfi|ldexp|stexp|mtps|mfps|mfpi|mtpi|mfpd|mtpd|mark|spl|emt|trap|wait|reset

all: rogue.lst

rogue.out: crt0.s console.c rogue.c console.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.s console.c rogue.c -lgcc

rogue.bin: rogue.out
	$(OBJCOPY) -O binary $< $@

rogue.lst: rogue.bin tolisting.py
	python3 tolisting.py rogue.bin $(LOAD) > $@

# The program must start at the load address, and use only instructions
# the emulators have.
check: rogue.out
	@test "$$($(PREFIX)nm rogue.out | awk '$$3 == "start" { print $$1 }')" = 00000200 \
		|| { echo "start is not at 0o1000"; exit 1; }
	@if $(OBJDUMP) -d rogue.out | grep -Ew '$(MISSING)'; then \
		echo "uses instructions the emulators do not implement"; exit 1; fi
	@$(PREFIX)size rogue.out
	@echo "check passed"

disassembly: rogue.out
	$(OBJDUMP) -d rogue.out

clean:
	rm -f rogue.out rogue.bin

.PHONY: all check disassembly clean
