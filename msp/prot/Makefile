#
# Makefile for library for communication between MSP430 and the host processor
#
# Copyright 2018, 2019 Phoenix Systems
#

CFLAGS := -Wextra -Wunused -g -Iinclude $(CFLAGS)
# -DMMP_DEBUG_LOGS

$(PREFIX_A)libmmp.a: $(addprefix $(PREFIX_O)prot/src/, crc16.o msp_mon_prot.o)
	$(ARCH)

$(PREFIX_H)msp_mon_prot.h: prot/include/msp_mon_prot.h
	$(HEADER)

all: $(PREFIX_A)libmmp.a $(PREFIX_H)msp_mon_prot.h
