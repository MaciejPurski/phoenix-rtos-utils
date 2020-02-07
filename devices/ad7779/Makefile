#
# Makefile for BeSmart ad7779 driver
#
# Copyright 2019 Phoenix Systems
#

$(PREFIX_PROG)ad7779-test: $(addprefix $(PREFIX_O)ad7779/, ad7779-test.o)
	$(LINK)

$(PREFIX_PROG)ad7779-driver: $(addprefix $(PREFIX_O)ad7779/, edma.o ad7779.o ad7779-driver.o)
	$(LINK)

$(PREFIX_H)adc-api.h: ad7779/adc-api.h
	$(HEADER)

all: $(PREFIX_PROG_STRIPPED)ad7779-test $(PREFIX_PROG_STRIPPED)ad7779-driver $(PREFIX_H)adc-api.h
