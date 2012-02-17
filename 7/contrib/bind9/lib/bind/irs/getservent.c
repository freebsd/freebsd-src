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
static const char rcsid[] = "$Id: getservent.c,v 1.3.18.1 2005-04-27 05:00:59 sra Exp $";
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

struct servent *
getservent(void) {
	struct net_data *net_data = init();

	return (getservent_p(net_data));
}

struct servent *
getservbyname(const char *name, const char *proto) {
	struct net_data *net_data = init();

	return (getservbyname_p(name, proto, net_data));
}

struct servent *
getservbyport(int port, const char *proto) {
	struct net_data *net_data = init();

	return (getservbyport_p(port, proto, net_data));
}

void
setservent(int stayopen) {
	struct net_data *net_data = init();

	setservent_p(stayopen, net_data);
}

void
endservent() {
	struct net_data *net_data = init();

	endservent_p(net_data);
}

/* Shared private. */

struct servent *
getservent_p(struct net_data *net_data) {
	struct irs_sv *sv;

	if (!net_data || !(sv = net_data->sv))
		return (NULL);
	net_data->sv_last = (*sv->next)(sv);
	return (net_data->sv_last);
}

struct servent *
getservbyname_p(const char *name, const char *proto,
		struct net_data *net_data) {
	struct irs_sv *sv;
	char **sap;

	if (!net_data || !(sv = net_data->sv))
		return (NULL);
	if (net_data->sv_stayopen && net_data->sv_last)
		if (!proto || !strcmp(net_data->sv_last->s_proto, proto)) {
			if (!strcmp(net_data->sv_last->s_name, name))
				return (net_data->sv_last);
			for (sap = net_data->sv_last->s_aliases;
			     sap && *sap; sap++)
				if (!strcmp(name, *sap))
					return (net_data->sv_last);
		}
	net_data->sv_last = (*sv->byname)(sv, name, proto);
	if (!net_data->sv_stayopen)
		endservent();
	return (net_data->sv_last);
}

struct servent *
getservbyport_p(int port, const char *proto, struct net_data *net_data) {
	struct irs_sv *sv;

	if (!net_data || !(sv = net_data->sv))
		return (NULL);
	if (net_data->sv_stayopen && net_data->sv_last)
		if (port == net_data->sv_last->s_port &&
		    ( !proto ||
		     !strcmp(net_data->sv_last->s_proto, proto)))
			return (net_data->sv_last);
	net_data->sv_last = (*sv->byport)(sv, port, proto);
	return (net_data->sv_last);
}

void
setservent_p(int stayopen, struct net_data *net_data) {
	struct irs_sv *sv;

	if (!net_data || !(sv = net_data->sv))
		return;
	(*sv->rewind)(sv);
	net_data->sv_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
}

void
endservent_p(struct net_data *net_data) {
	struct irs_sv *sv;

	if ((net_data != NULL) && ((sv = net_data->sv) != NULL))
		(*sv->minimize)(sv);
}

/* Private */

static struct net_data *
init() {
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->sv) {
		net_data->sv = (*net_data->irs->sv_map)(net_data->irs);

		if (!net_data->sv || !net_data->res) {
 error:		
			errno = EIO;
			return (NULL);
		}
		(*net_data->sv->res_set)(net_data->sv, net_data->res, NULL);
	}
	
	return (net_data);
}

#endif /*__BIND_NOSTATIC*/

/*! \file */
