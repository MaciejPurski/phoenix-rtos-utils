#
# Makefile for MSP430 programming tool
#
# Copyright 2018, 2019 Phoenix Systems
#

CFLAGS += -I./tools/msp-mon-prog/log

$(PREFIX_PROG)msp-mon-prog: $(addprefix $(PREFIX_O)tools/msp-mon-prog/, main.o ihex/ihex.o bsl/src/bsl.o bsl/hal/phrtos3/hal.o log/log.o)
	$(LINK)

all: $(PREFIX_PROG_STRIPPED)msp-mon-prog
