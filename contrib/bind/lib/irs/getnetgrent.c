/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: getnetgrent.c,v 1.9 1997/12/04 04:57:53 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#include <errno.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct irs_ng *	init(void);

/* Public */

void
setnetgrent(const char *netgroup) {
	struct irs_ng *ng = init();
	
	if (ng != NULL)
		(*ng->rewind)(ng, netgroup);
}

void 
endnetgrent(void) {
	struct irs_ng *ng = init();
	
	if (ng)
		(*ng->close)(ng);
	net_data.ng = NULL;
}

int
innetgr(const char *netgroup, const char *host,
	const char *user, const char *domain) {
	struct irs_ng *ng = init();
	
	if (!ng)
		return (0);
	return ((*ng->test)(ng, netgroup, host, user, domain));
}

int
getnetgrent(char **host, char **user, char **domain) {
	struct irs_ng *ng = init();
	struct netgrp *ngent;
	
	if (!ng)
		return (0);
	return ((*ng->next)(ng, host, user, domain));
}

/* Private */

static struct irs_ng *
init(void) {
	
	if (!net_data_init())
		goto error;
	if (!net_data.ng)
		net_data.ng = (*net_data.irs->ng_map)(net_data.irs);
	if (!net_data.ng) {
error:
		errno = EIO;
		return (NULL);
	}
	return (net_data.ng);
}
