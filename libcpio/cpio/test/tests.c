/*
 * Phoenix-RTOS
 *
 * i.MX RT cpio tests
 *
 * Copyright 2020 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "../cpio.h"


int test_findFiles(void)
{
    cpio_file_t fd;

    const char *f1 = "/dev/flash1.raw1/file_1";
    const char *f2 = "/dev/flash1.raw1/file_2";
    const char *f3 = "/dev/flash1.raw1/file_3";

    /* Check file_1 */
    if (cpio_findFile(f1, &fd) < 0)
        return -1;

    if (fd.currPos != 0 || fd.size != 26)
        return -1;

    /* Check file_2 */
    if (cpio_findFile(f2, &fd) < 0)
        return -1;

    if (fd.currPos != 0 || fd.size != 575)
        return -1;

    /* Check file_3 */
    if (cpio_findFile(f3, &fd) < 0)
        return -1;

    if (fd.currPos != 0 || fd.size != 6)
        return -1;

    return 0;
}


int test_readWholeFile(void)
{
    cpio_file_t fd;

    char buff[0x20];

    const char *name = "/dev/flash1.raw1/file_3";
    const char *data = "test3";

    if (cpio_findFile(name, &fd) < 0)
        printf("Failed\n");

    memset(buff, 0, 0x20);

    if (cpio_readFile(buff, fd.size, &fd) != fd.size)
        return -1;

    if (strncmp(data, buff, strlen(data)) != 0)
        return -1;

    return 0;
}


int test_exceedFile(void)
{
    cpio_file_t fd;

    char buff[0x20];

    const char *name = "/dev/flash1.raw1/file_1";
    const char *data = "test1test1test1test1";

    if (cpio_findFile(name, &fd) < 0)
        printf("Failed\n");

    memset(buff, 0, 0x20);

    /* Read first part of file */
    if (cpio_readFile(buff, 0x5, &fd) != 0x5)
        return -1;

    /* Exceed file size */
    if (cpio_readFile(buff, fd.size, &fd) != (fd.size - 0x5))
        return -1;

    if (strncmp(data, buff, strlen(data)) != 0)
        return -1;

    /* Exceed file size, function should not ready any value */
    if (cpio_readFile(buff, fd.size, &fd) != 0 && fd.currPos != fd.size)
        return -1;

    return 0;
}


int test_seekFile(void)
{
    cpio_file_t fd;

    char buff[0x20];

    const char *name = "/dev/flash1.raw1/file_2";
    const char *text1 = "Ipsum";
    const char *text2 = "simply";
    const char *text3 = "Lorem";

    if (cpio_findFile(name, &fd) < 0)
        printf("Failed\n");

    memset(buff, 0, 0x20);
    /* Seek from begin of file */
    if (cpio_seekFile(&fd, 6, cpio_seek_set) < 0)
        return -1;

    if (cpio_readFile(buff, strlen(text1), &fd) != strlen(text1))
        return -1;

    if (strncmp(text1, buff, strlen(text1)) != 0)
        return -1;

    memset(buff, 0, 0x20);
    /* Seek from current position */
    if (cpio_seekFile(&fd, 4, cpio_seek_cur) < 0)
        return -1;

    if (cpio_readFile(buff, strlen(text2), &fd) != strlen(text2))
        return -1;

    if (strncmp(text2, buff, strlen(text2)) != 0)
        return -1;

    memset(buff, 0, 0x20);
    /* Seek form end of the file */
    if (cpio_seekFile(&fd, 13, cpio_seek_end) < 0)
        return -1;

    if (cpio_readFile(buff, strlen(text3), &fd) != strlen(text3))
        return -1;

    if (strncmp(text3, buff, strlen(text3)) != 0)
        return -1;

    return 0;
}



int main(int argc, char **argv)
{
    sleep(2);

    TEST_CATEGORY("CPIO TESTS");

    TEST_CASE(test_findFiles());
    TEST_CASE(test_readWholeFile());
    TEST_CASE(test_exceedFile());
    TEST_CASE(test_seekFile());

    return 0;
}
