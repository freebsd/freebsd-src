/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Make sure this still passes if both endian.h and sys/endian.h are included
 * with sys/endian.h first
 */
#include <sys/endian.h>
#include <endian.h>
#include "sys_endian_test.c"
