#
# Makefile for Incotex DCU MSP430 Monitor Daemon
#
# Copyright 2018, 2019 Phoenix Systems
#

$(PREFIX_PROG)lemond: $(PREFIX_O)tools/lemond/main.o $(addprefix $(PREFIX_A), libmmp.a ps_dcsap_logger.a ps_dlms.a ps_dcu_cosem.a ps_util.a hal.a)
	$(LINK)

$(PREFIX_H)rtc.h: tools/lemond/rtc.h
	$(HEADER)

all: $(PREFIX_H)rtc.h $(PREFIX_PROG_STRIPPED)lemond


