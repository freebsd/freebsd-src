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
#include <exterr.h>
#include <string.h>
#include "libc_private.h"

static struct uexterror uexterr = {
	.ver = UEXTERROR_VER,
};

int __getosreldate(void);

static void uexterr_ctr(void) __attribute__((constructor));
static void
uexterr_ctr(void)
{
	if (__getosreldate() >= P_OSREL_EXTERRCTL)
		exterrctl(EXTERRCTL_ENABLE, 0, &uexterr);
}

int
__libc_uexterr_gettext(char *buf, size_t bufsz)
{
	return (__uexterr_format(&uexterr, buf, bufsz));
}

int
uexterr_gettext(char *buf, size_t bufsz)
{
	return (((int (*)(char *, size_t))
	    __libc_interposing[INTERPOS_uexterr_gettext])(buf, bufsz));
}
