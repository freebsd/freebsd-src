/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 */

/*
 * This is a pure C function generously donated by ARM.
 * See src/contrib/arm-optimized-routines/math/tgamma128.[ch].
 */
#define tgamma128 tgammal
#include "tgamma128.c"
