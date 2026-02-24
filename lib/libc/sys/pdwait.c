/*
 * Copyright 2026 The FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/procdesc.h>
#include "libc_private.h"

#pragma weak pdwait
int
pdwait(int fd, int *status, int options, struct __wrusage *ru,
    struct __siginfo *infop)
{
	return (INTERPOS_SYS(pdwait, fd, status, options, ru, infop));
}
