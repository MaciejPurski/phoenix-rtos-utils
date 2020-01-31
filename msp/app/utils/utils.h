/*
 * utils.h
 *
 *  Created on: Aug 1, 2018
 *      Author: phoenix
 */

#ifndef UTILS_UTILS_H_
#define UTILS_UTILS_H_

#include <math.h>

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} vector3_t;

static inline vector3_t vector3_sub(vector3_t a, vector3_t b)
{
    vector3_t c;
    c.x = a.x - b.x;
    c.y = a.y - b.y;
    c.z = a.z - b.z;
    return c;
}

static inline float vector3_magnitude(vector3_t v)
{
    return sqrtf((float)v.x * v.x + (float)v.y * v.y + (float)v.z * v.z);
}

static inline int16_t _twosComplementToInt16(uint16_t x)
{
    if (x & (1 << 15))
        return (int16_t)(x & (~(1 << 15))) + INT16_MIN;

    return x;
}

#endif /* UTILS_UTILS_H_ */
