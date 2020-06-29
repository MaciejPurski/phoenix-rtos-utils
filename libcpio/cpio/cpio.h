/*
 * Phoenix-RTOS
 *
 * i.MX RT cpio reader
 *
 * Copyright 2020 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <phoenix/arch/imxrt.h>


typedef struct {
    uint32_t offs;
    uint32_t currPos;
    uint32_t size;

    oid_t oid;
} cpio_file_t;


enum { cpio_seek_set = 0 /* beginnig of file */, cpio_seek_cur /* current position of file */, cpio_seek_end /* end of file */};


int cpio_findFile(const char *filepath, cpio_file_t *fd);

ssize_t cpio_readFile(char *buff, size_t size, cpio_file_t *fd);

int cpio_seekFile(cpio_file_t *fd, uint32_t offset, int origin);
