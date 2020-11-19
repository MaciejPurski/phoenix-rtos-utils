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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/msg.h>

#include "cpio.h"


#define END_OF_ARCHIVE      "TRAILER!!!"
#define MAGIC_BYTES_NEWC    "070701"

#define MAX_FILE_NAME_SIZE   0x100
#define MAX_REPETITION_NB    0x6
#define CPIO_PAD             0x3


typedef struct {
	char c_magic[6];
	char c_ino[8];
	char c_mode[8];
	char c_uid[8];
	char c_gid[8];
	char c_nlink[8];
	char c_mtime[8];
	char c_filesize[8];
	char c_devmajor[8];
	char c_devminor[8];
	char c_rdevmajor[8];
	char c_rdevminor[8];
	char c_namesize[8];
	char c_check[8];
	char name[];
} cpio_newc_t;


static uint32_t cpio_a2i(const char *s)
{
	uint32_t i, k = 28, v = 0;
	char d;

	for (i = 0; (i < 8) && s[i]; i++) {
		d = s[i] - '0';

		if ((d > 16) && (d < 23))
			d -= 7;
		else if ((d > 48) && (d < 55))
			d -= 39;
		else if (d > 9)
			return -1;

		v += (d << k);
		k -= 4;
	}

	return v;
}


static int cpio_readData(oid_t oid, size_t offs, size_t size, void *data)
{
	msg_t msg;

	msg.type = mtRead;
	msg.i.io.oid = oid;
	msg.i.io.offs = offs;
	msg.i.data = NULL;
	msg.i.size = 0;
	msg.o.data = data;
	msg.o.size = size;

	if (msgSend(oid.port, &msg) != 0 )
		return -1;

	if (msg.o.io.err < size)
		return -1;

	return 0;
}


int cpio_findFile(const char *filepath, cpio_file_t *fd)
{
	cpio_newc_t cpio;
	int firstPass = 1;
	uint32_t offs = 0, fs = 0, ns = 0, rpt = 0;
	char *dir, *filename, *pathBuff;
	char nameBuff[MAX_FILE_NAME_SIZE];

	if ((pathBuff = strdup(filepath)) == NULL)
		return -1;

	memset(nameBuff, 0, MAX_FILE_NAME_SIZE);

	filename = basename(pathBuff);
	dir = dirname(pathBuff);

	/* Connect to flashsrv */
	while (lookup(dir, NULL, &fd->oid) < 0) {
		usleep(10000);
		if (rpt++ > MAX_REPETITION_NB) {
			free(pathBuff);
			return -1;
		}
	}

	/* Read headers */
	for (;;) {
		cpio_readData(fd->oid, offs, sizeof(cpio_newc_t), &cpio);
		if (strncmp(cpio.c_magic, MAGIC_BYTES_NEWC, 6) != 0) {
			free(pathBuff);
			return -1;
		}

		ns = cpio_a2i(cpio.c_namesize);
		fs = cpio_a2i(cpio.c_filesize);

		offs += sizeof(cpio_newc_t);
		if (ns > MAX_FILE_NAME_SIZE || cpio_readData(fd->oid, offs, ns, nameBuff) < 0) {
			free(pathBuff);
			return -1;
		}

		if (strcmp(nameBuff, END_OF_ARCHIVE) == 0) {
			free(pathBuff);
			return -1;
		}

		offs = (offs + ns + CPIO_PAD) & ~CPIO_PAD;

		if (firstPass) {
			firstPass = 0;
			continue;
		}

		if (strncmp(nameBuff, filename, ns) == 0) {
			fd->size = fs;
			fd->offs = offs;
			fd->currPos = 0;
			break;
		}

		offs = (offs + fs + CPIO_PAD) & ~CPIO_PAD;
	}

	free(pathBuff);

	return 0;
}


ssize_t cpio_readFile(char *buff, size_t size, cpio_file_t *fd)
{
	uint32_t sz;

	if (fd->currPos + size > fd->size)
		sz = fd->size - fd->currPos;
	else
		sz = size;

	if (cpio_readData(fd->oid, fd->offs + fd->currPos, sz, buff) < 0)
		return -1;

	fd->currPos += sz;

	return sz;
}


int cpio_seekFile(cpio_file_t *fd, uint32_t offset, int origin)
{
	switch (origin) {
		case cpio_seek_cur:
			if (fd->currPos + offset > fd->size)
				return -1;
			fd->currPos += offset;
			break;

		case cpio_seek_end:
			if (offset > fd->size)
				return -1;
			fd->currPos = fd->size - offset;
			break;

		case cpio_seek_set:
			if (offset > fd->size)
				return -1;
			fd->currPos = offset;
			break;

		default:
			return -1;
	}

	return 0;
}


int cpio_endOfFile(const cpio_file_t *fd)
{
	return fd->size == fd->currPos;
}
