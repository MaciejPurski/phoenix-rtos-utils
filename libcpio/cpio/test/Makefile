#
# Phoenix-RTOS
#
#  i.MX RT storage tests
#
# Copyright 2020 Phoenix Systems
# Author: Hubert Buczynski
#

$(PREFIX_PROG)cpio-tests: $(addprefix $(PREFIX_O)cpio/test/, tests.o) $(PREFIX_A)libcpio.a
	$(LINK)

$(PREFIX_O)cpio/test/cpio-tests.o: $(PREFIX_H)libcpio.h

all: $(PREFIX_PROG_STRIPPED)cpio-tests

