/*
 * Log module - null implementation
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

int log_init(void)
{
    return 0;
}

int log_printf(const char* fmt, ...)
{
    (void)fmt;

    return 0;
}

int log_update(void)
{
    return 0;
}
