#
# Phoenix-RTOS
#
# flash driver
#
# Copyright 2019 Phoenix Systems
# Author: Hubert Buczynski
#

$(PREFIX_A)libcpio.a: $(addprefix $(PREFIX_O)cpio/, cpio.o)
	$(ARCH)

$(PREFIX_H)cpio.h: cpio/cpio.h 
	$(HEADER)

all: $(PREFIX_A)libcpio.a $(PREFIX_H)cpio.h 



