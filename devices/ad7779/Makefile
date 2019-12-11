#
# Makefile for Incotex DCU ad7779 driver
#
# Copyright 2019 Phoenix Systems
#

$(PREFIX_PROG)ad7779: $(addprefix $(PREFIX_O)ad7779/, ad7779.o ad7779_test.o) #ad7779-driver.o)
	$(LINK)

#$(PREFIX_H)adc-api.h: ad7779/adc-api.h
#	$(HEADER)

all: $(PREFIX_PROG_STRIPPED)ad7779 #$(PREFIX_H)adc-api.h
