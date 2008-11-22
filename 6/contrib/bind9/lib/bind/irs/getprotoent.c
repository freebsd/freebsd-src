/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: getprotoent.c,v 1.2.206.1 2004/03/09 08:33:36 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#if !defined(__BIND_NOSTATIC)

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct net_data *init(void);

/* Public */

struct protoent *
getprotoent() {
	struct net_data *net_data = init();

	return (getprotoent_p(net_data));
}

struct protoent *
getprotobyname(const char *name) {
	struct net_data *net_data = init();

	return (getprotobyname_p(name, net_data));
}

struct protoent *
getprotobynumber(int proto) {
	struct net_data *net_data = init();

	return (getprotobynumber_p(proto, net_data));
}

void
setprotoent(int stayopen) {
	struct net_data *net_data = init();

	setprotoent_p(stayopen, net_data);
}

void
endprotoent() {
	struct net_data *net_data = init();

	endprotoent_p(net_data);
}

/* Shared private. */

struct protoent *
getprotoent_p(struct net_data *net_data) {
	struct irs_pr *pr;

	if (!net_data || !(pr = net_data->pr))
		return (NULL);
	net_data->pr_last = (*pr->next)(pr);
	return (net_data->pr_last);
}

struct protoent *
getprotobyname_p(const char *name, struct net_data *net_data) {
	struct irs_pr *pr;
	char **pap;

	if (!net_data || !(pr = net_data->pr))
		return (NULL);
	if (net_data->pr_stayopen && net_data->pr_last) {
		if (!strcmp(net_data->pr_last->p_name, name))
			return (net_data->pr_last);
		for (pap = net_data->pr_last->p_aliases; pap && *pap; pap++)
			if (!strcmp(name, *pap))
				return (net_data->pr_last);
	}
	net_data->pr_last = (*pr->byname)(pr, name);
	if (!net_data->pr_stayopen)
		endprotoent();
	return (net_data->pr_last);
}

struct protoent *
getprotobynumber_p(int proto, struct net_data *net_data) {
	struct irs_pr *pr;

	if (!net_data || !(pr = net_data->pr))
		return (NULL);
	if (net_data->pr_stayopen && net_data->pr_last)
		if (net_data->pr_last->p_proto == proto)
			return (net_data->pr_last);
	net_data->pr_last = (*pr->bynumber)(pr, proto);
	if (!net_data->pr_stayopen)
		endprotoent();
	return (net_data->pr_last);
}

void
setprotoent_p(int stayopen, struct net_data *net_data) {
	struct irs_pr *pr;

	if (!net_data || !(pr = net_data->pr))
		return;
	(*pr->rewind)(pr);
	net_data->pr_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
}

void
endprotoent_p(struct net_data *net_data) {
	struct irs_pr *pr;

	if ((net_data != NULL) && ((pr = net_data->pr) != NULL))
		(*pr->minimize)(pr);
}

/* Private */

static struct net_data *
init() {
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->pr) {
		net_data->pr = (*net_data->irs->pr_map)(net_data->irs);

		if (!net_data->pr || !net_data->res) {
 error:		
			errno = EIO;
			return (NULL);
		}
		(*net_data->pr->res_set)(net_data->pr, net_data->res, NULL);
	}
	
	return (net_data);
}

#endif /*__BIND_NOSTATIC*/
