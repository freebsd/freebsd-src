/*
 * Copyright (c) 1996 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: getprotoent.c,v 1.9 1997/12/04 04:57:53 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct irs_pr *	init(void);

/* Public */

struct protoent *
getprotoent() {
	struct irs_pr *pr = init();
	
	if (!pr)
		return (NULL);
	net_data.pr_last = (*pr->next)(pr);
	return (net_data.pr_last);
}

struct protoent *
getprotobyname(const char *name) {
	struct irs_pr *pr = init();
	char **pap;

	if (!pr)
		return (NULL);
	if (net_data.pr_stayopen && net_data.pr_last) {
		if (!strcmp(net_data.pr_last->p_name, name))
			return (net_data.pr_last);
		for (pap = net_data.pr_last->p_aliases; pap && *pap; pap++)
			if (!strcmp(name, *pap))
				return (net_data.pr_last);
	}
	net_data.pr_last = (*pr->byname)(pr, name);
	if (!net_data.pr_stayopen)
		endprotoent();
	return (net_data.pr_last);
}

struct protoent *
getprotobynumber(int proto) {
	struct irs_pr *pr = init();

	if (!pr)
		return (NULL);
	if (net_data.pr_stayopen && net_data.pr_last)
		if (net_data.pr_last->p_proto == proto)
			return (net_data.pr_last);
	net_data.pr_last = (*pr->bynumber)(pr, proto);
	if (!net_data.pr_stayopen)
		endprotoent();
	return (net_data.pr_last);
}

void
setprotoent(int stayopen) {
	struct irs_pr *pr = init();

	if (!pr)
		return;
	(*pr->rewind)(pr);
	net_data.pr_stayopen = (stayopen != 0);
}

void
endprotoent() {
	struct irs_pr *pr = init();

	if (pr != NULL)
		(*pr->minimize)(pr);
}

/* Private */

static struct irs_pr *
init() {
	if (!net_data_init())
		goto error;
	if (!net_data.pr)
		net_data.pr = (*net_data.irs->pr_map)(net_data.irs);
	if (!net_data.pr) {
 error:		
		errno = EIO;
		return (NULL);
	}
	return (net_data.pr);
}
