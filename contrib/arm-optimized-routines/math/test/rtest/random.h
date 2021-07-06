/*
 * random.h - header for random.c
 *
 * Copyright (c) 2009-2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#include "types.h"

uint32 random32(void);
uint32 random_upto(uint32 limit);
uint32 random_upto_biased(uint32 limit, int bias);
