/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* Make sure this still passes if both endian.h and byteswap.h included */
#include <endian.h>
#include "byteswap_test.c"
