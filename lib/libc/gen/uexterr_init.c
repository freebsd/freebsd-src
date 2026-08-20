/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#define	_WANT_P_OSREL
#include <sys/param.h>
#include <sys/exterrvar.h>
#include "libc_private.h"

struct uexterror uexterr = {
	.ver = UEXTERROR_VER,
};

static void uexterr_ctr(void) __attribute__((constructor));
static void
uexterr_ctr(void)
{
	if (__getosreldate() >= P_OSREL_EXTERRCTL)
		exterrctl(EXTERRCTL_ENABLE, 0, &uexterr);
}
