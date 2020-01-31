#
# Makefile for Incotex DCU ad7779 driver
#
# Copyright 2019 Phoenix Systems
# Author: Pawel Pisarczyk
#


OLED_CORE = oled-phy.o gpio.o
OLED_GRAPHIC = oled-graphic.o fonts/font.o fonts/5x7.o
OLED_MENU_FUNCTIONS = oled-functions.o $(addprefix functions/, action.o eth.o info.o meters.o plc.o sensor.o time.o helper.o)

OLED_MENU_LIBS = libjansson.a libsystem.a ps_dcsap_logger.a ps_dcu_cosem.a ps_dlms.a hal.a

# Needed because some DLMS headers contain static functions
CFLAGS += -Wno-unused-function

$(PREFIX_PROG)oled-128O064G: $(addprefix $(PREFIX_O)oled-128O064G/, oled-dev.o $(OLED_GRAPHIC) $(OLED_CORE))
	$(LINK)

$(PREFIX_PROG)oled-test: $(addprefix $(PREFIX_O)oled-128O064G/, oled-test.o $(OLED_CORE))
	$(LINK)

$(PREFIX_PROG)oled-menu: $(addprefix $(PREFIX_O)oled-128O064G/, oled-menu.o $(OLED_MENU_FUNCTIONS) $(OLED_GRAPHIC) $(OLED_CORE)) $(addprefix $(PREFIX_A), $(OLED_MENU_LIBS))
	$(LINK)

all: $(addprefix $(PREFIX_PROG_STRIPPED), oled-128O064G oled-test oled-menu)
