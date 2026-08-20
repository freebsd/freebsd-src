/*-
 * Copyright (c) 2026 Capabilities Limited
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Capabilities Limited with funding from
 * Innovate UK and the Department for Science, Innovation and Technology
 * for the adoption and diffusion of CHERI technology under project
 * 10168042 (“CheriBSD feature extraction, maturity, and testing”).
 */

#include <sys/exterrvar.h>
#include <errno.h>
#include <exterr.h>
#include <string.h>
#include "libc_private.h"

void
__uexterr_set_ue(struct uexterror *ue, int error, int category,
    const char *mmsg, uint64ptr_t pp1, uint64ptr_t pp2, int line)
{
	memset((char *)ue + offsetof(struct uexterror, error), 0,
	    sizeof(struct uexterror) - offsetof(struct uexterror, error));
	ue->error = error;
	ue->cat = category | EXTERR_CAT_SRC_USER;
	ue->src_line = line;
	ue->p1 = pp1;
	ue->p2 = pp2;
	if (mmsg != NULL)
		strlcpy(ue->msg, mmsg, sizeof(ue->msg));
	else
		ue->msg[0] = '\0';
	errno = error;
}

void
__libc_uexterr_set(int error, int category, const char *mmsg, uint64ptr_t pp1,
    uint64ptr_t pp2, int line)
{
	__uexterr_set_ue(&uexterr, error, category, mmsg, pp1, pp2, line);
}

void
uexterr_set(int error, int category, const char *mmsg, uint64ptr_t pp1,
    uint64ptr_t pp2, int line)
{
	((void (*)(int, int, const char *, uint64ptr_t, uint64ptr_t, int))
	    __libc_interposing[INTERPOS_uexterr_set])(error, category,
	    mmsg, pp1, pp2, line);
}
