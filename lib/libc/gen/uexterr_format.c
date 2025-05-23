/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/exterrvar.h>
#include <exterr.h>
#include <stdio.h>
#include <string.h>

int
__uexterr_format(const struct uexterror *ue, char *buf, size_t bufsz)
{
	if (bufsz > UEXTERROR_MAXLEN)
		bufsz = UEXTERROR_MAXLEN;
	if (ue->error == 0) {
		strlcpy(buf, "No error", bufsz);
		return (0);
	}
	snprintf(buf, bufsz,
	    "errno %d category %u (src line %u) p1 %#jx p2 %#jx %s",
	    ue->error, ue->cat, ue->src_line,
	    (uintmax_t)ue->p1, (uintmax_t)ue->p2, ue->msg);
	return (0);
}
