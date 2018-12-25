/*
* Copyright (C) 2012 Spreadtrum Communications Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "sin_cos.h"
#define SINCOS_BIT_31  (1 << 31)
#define SINCOS_BIT_30  (1 << 30)
#define SMULL(x, y) ((int64_t)(x) * (int64_t)(y))

static const uint32_t cossin_tbl[64] = {
    0x3ffec42d, 0x00c90e90, 0x3ff4e5e0, 0x025b0caf,
    0x3fe12acb, 0x03ecadcf, 0x3fc395f9, 0x057db403,
    0x3f9c2bfb, 0x070de172, 0x3f6af2e3, 0x089cf867,
    0x3f2ff24a, 0x0a2abb59, 0x3eeb3347, 0x0bb6ecef,
    0x3e9cc076, 0x0d415013, 0x3e44a5ef, 0x0ec9a7f3,
    0x3de2f148, 0x104fb80e, 0x3d77b192, 0x11d3443f,
    0x3d02f757, 0x135410c3, 0x3c84d496, 0x14d1e242,
    0x3bfd5cc4, 0x164c7ddd, 0x3b6ca4c4, 0x17c3a931,
    0x3ad2c2e8, 0x19372a64, 0x3a2fcee8, 0x1aa6c82b,
    0x3983e1e8, 0x1c1249d8, 0x38cf1669, 0x1d79775c,
    0x3811884d, 0x1edc1953, 0x374b54ce, 0x2039f90f,
    0x367c9a7e, 0x2192e09b, 0x35a5793c, 0x22e69ac8,
    0x34c61236, 0x2434f332, 0x33de87de, 0x257db64c,
    0x32eefdea, 0x26c0b162, 0x31f79948, 0x27fdb2a7,
    0x30f8801f, 0x29348937, 0x2ff1d9c7, 0x2a650525,
    0x2ee3cebe, 0x2b8ef77d, 0x2dce88aa, 0x2cb2324c
};

static int32_t sin_core(int32_t arc_q33, int32_t sign)
{
    int32_t a, b, B;
    int32_t sin_a, cos_a;
    int32_t S1, S2, S3, S4, T1;
    int32_t R = 0;
    int32_t C1 = 0x6487ed51;
    int32_t C2 = -715827882;

    a = arc_q33 >> 25;
    cos_a = cossin_tbl[2 * a];
    sin_a = cossin_tbl[2 * a + 1];
    cos_a ^= (sign >> 31);
    sin_a ^= (sign >> 31);
    a = a << 25;
    b = arc_q33 - a;
    b -= (1 << 24);
    B = SMULL((b << 3), C1) >> 32;
    T1 = SMULL(B, B) >> 32;
    S1 = sin_a;
    S2 = SMULL(sin_a, T1 >> 1) >> 32;
    S3 = SMULL(cos_a, B) >> 32;
    S4 = SMULL(T1, C2) >> 32;
    S4 = SMULL(S3, S4) >> 32;
    R = S1 - S2 + S3 + S4;
    return R;
}

static int32_t cos_core(int32_t arc_q33, int32_t sign)
{
    int32_t a, b, B;
    int32_t sin_a, cos_a;
    int32_t S1, S2, S3, S4, T1;
    int32_t R = 0;
    int32_t C1 = 0x6487ed51;    //round(2 * PI * 2 ^ 28)
    int32_t C2 = 0x2aaaaaab;    //round(2^32/6)

    a = arc_q33 >> 25;
    cos_a = cossin_tbl[a * 2];
    sin_a = cossin_tbl[a * 2 + 1];
    cos_a ^= (sign >> 31);
    sin_a ^= (sign >> 31);
    a = a << 25;
    b = arc_q33 - a;
    b -= (1 << 24);
    B = SMULL((b << 3), C1) >> 32;
    T1 = SMULL(B, B) >> 32;
    S1 = cos_a;
    S2 = SMULL(cos_a, T1 >> 1) >> 32;
    S3 = SMULL(sin_a, B) >> 32;
    S4 = SMULL(T1, C2) >> 32;
    S4 = SMULL(S3, S4) >> 32;
    R = S1 - S2 - S3 + S4;
    return R;
}

int32_t __attribute__((weak))sin_32(int32_t n)
{
    int32_t s = n & SINCOS_BIT_31;
    n = n << 1;
    if (0 == n) {
        return 0;
    }
    if (SINCOS_BIT_31 == (n & SINCOS_BIT_31)) {
        n &= ~SINCOS_BIT_31;
        if (n < SINCOS_BIT_30) {
            return cos_core(n, s);
        } else if (n == SINCOS_BIT_30) {
            n -= 1;
        } else if (n > SINCOS_BIT_30) {
            n = SINCOS_BIT_31 - n;
        }
        return sin_core(n, s);
    } else {
        if (n < SINCOS_BIT_30) {
            return sin_core(n, s);
        }
        if (n == SINCOS_BIT_30) {
            n -= 1;
        } else if (n > SINCOS_BIT_30) {
            n = SINCOS_BIT_31 - n;
        }
        return cos_core(n, s);
    }
}

int32_t __attribute__((weak))cos_32(int32_t n)
{
    int32_t s = n & SINCOS_BIT_31;

    n = n << 1;
    if (n == SINCOS_BIT_31) {
        return 0;
    }
    if (SINCOS_BIT_31 == (n & SINCOS_BIT_31)) {
        n &= ~SINCOS_BIT_31;
        if (n < SINCOS_BIT_30) {
            return -sin_core(n, s);
        } else if (n == SINCOS_BIT_30) {
            n -= 1;
        } else if (n > SINCOS_BIT_30) {
            n = SINCOS_BIT_31 - n;
        }
        return -cos_core(n, s);
    } else {
        if (n < SINCOS_BIT_30) {
            return cos_core(n, s);
        }
        if (n == SINCOS_BIT_30) {
            n -= 1;
        } else if (n > SINCOS_BIT_30) {
            n = SINCOS_BIT_31 - n;
        }
        return sin_core(n, s);
    }
}
