/*
 * imxwdg.h
 *
 *  Created on: Jul 20, 2018
 *      Author: phoenix
 */

#ifndef IMXWDG_IMXWDG_H_
#define IMXWDG_IMXWDG_H_

#include <board.h>

#define IMXWDG_MIN_REFRESH_RATE     (30)

void imxwdg_init(void);
void imxwdg_refresh(void);
int imxwdg_update(void);

#endif /* IMXWDG_IMXWDG_H_ */
