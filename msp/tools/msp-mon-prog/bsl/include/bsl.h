/*
 * MSP430 BSL programming library
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP430_BSL_PROG_BSL_H
#define MSP430_BSL_PROG_BSL_H

#include <stdint.h>

/* Initializes BSL HAL, opens serial connection, optionally generates entry
 * sequence and finally tries to unlock the BSL with provided password.
 * If no password is provided, mass erase is performed and then default password
 * is used. */
int bsl_open(const char *device,
             const uint8_t* password,
             int no_entry_seq,
             int invert_tst,
             int invert_rst);

/* Generates BSL entry sequence on TEST and RESET lines. */
int bsl_entry_sequence(int invert_tst, int invert_rst);

int bsl_unlock(const uint8_t *password);
int bsl_mass_erase(void);
int bsl_erase_check(uint32_t addr, uint16_t len);
int bsl_program(uint32_t addr, const uint8_t *data, uint16_t len);
int bsl_verify(uint32_t addr, const uint8_t *data, uint16_t len);
int bsl_load_pc(uint32_t addr);

int bsl_close(void);

#endif // #ifndef MSP430_BSL_PROG_BSL_H
