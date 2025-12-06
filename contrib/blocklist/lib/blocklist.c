/*	$NetBSD: blocklist.c,v 1.4 2025/02/11 17:48:30 christos Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
__RCSID("$NetBSD: blocklist.c,v 1.4 2025/02/11 17:48:30 christos Exp $");

#include <stdio.h>
#include <bl.h>

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

int
blocklist_sa(int action, int rfd, const struct sockaddr *sa, socklen_t salen,
    const char *msg)
{
	struct blocklist *bl;
	int rv;
	if ((bl = blocklist_open()) == NULL)
		return -1;
	rv = blocklist_sa_r(bl, action, rfd, sa, salen, msg);
	blocklist_close(bl);
	return rv;
}

int
blocklist_sa_r(struct blocklist *bl, int action, int rfd,
	const struct sockaddr *sa, socklen_t slen, const char *msg)
{
	bl_type_t internal_action;

	/* internal values are not the same as user application values */
	switch (action) {
	case BLOCKLIST_AUTH_FAIL:
		internal_action = BL_ADD;
		break;
	case BLOCKLIST_AUTH_OK:
		internal_action = BL_DELETE;
		break;
	case BLOCKLIST_ABUSIVE_BEHAVIOR:
		internal_action = BL_ABUSE;
		break;
	case BLOCKLIST_BAD_USER:
		internal_action = BL_BADUSER;
		break;
	default:
		internal_action = BL_INVALID;
		break;
	}
	return bl_send(bl, internal_action, rfd, sa, slen, msg);
}

int
blocklist(int action, int rfd, const char *msg)
{
	return blocklist_sa(action, rfd, NULL, 0, msg);
}

int
blocklist_r(struct blocklist *bl, int action, int rfd, const char *msg)
{
	return blocklist_sa_r(bl, action, rfd, NULL, 0, msg);
}

struct blocklist *
blocklist_open(void) {
	return bl_create(false, NULL, vsyslog_r);
}

struct blocklist *
blocklist_open2(
    void (*logger)(int, struct syslog_data *, const char *, va_list))
{
	return bl_create(false, NULL, logger);
}

void
blocklist_close(struct blocklist *bl)
{
	bl_destroy(bl);
}
