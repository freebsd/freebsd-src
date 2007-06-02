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
static const char rcsid[] = "$Id: getnetent.c,v 1.6.18.1 2005/04/27 05:00:58 sra Exp $";
#endif

/* Imports */

#include "port_before.h"

#if !defined(__BIND_NOSTATIC)

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "irs_data.h"

/* Definitions */

struct pvt {
	struct netent	netent;
	char *		aliases[1];
	char		name[MAXDNAME + 1];
};

/* Forward */

static struct net_data *init(void);
static struct netent   *nw_to_net(struct nwent *, struct net_data *);
static void		freepvt(struct net_data *);
static struct netent   *fakeaddr(const char *, int af, struct net_data *);

/* Portability */

#ifndef INADDR_NONE
# define INADDR_NONE 0xffffffff
#endif

/* Public */

struct netent *
getnetent() {
	struct net_data *net_data = init();

	return (getnetent_p(net_data));
}

struct netent *
getnetbyname(const char *name) {
	struct net_data *net_data = init();

	return (getnetbyname_p(name, net_data));
}

struct netent *
getnetbyaddr(unsigned long net, int type) {
	struct net_data *net_data = init();

	return (getnetbyaddr_p(net, type, net_data));
}

void
setnetent(int stayopen) {
	struct net_data *net_data = init();

	setnetent_p(stayopen, net_data);
}


void
endnetent() {
	struct net_data *net_data = init();

	endnetent_p(net_data);
}

/* Shared private. */

struct netent *
getnetent_p(struct net_data *net_data) {
	struct irs_nw *nw;

	if (!net_data || !(nw = net_data->nw))
		return (NULL);
	net_data->nww_last = (*nw->next)(nw);
	net_data->nw_last = nw_to_net(net_data->nww_last, net_data);
	return (net_data->nw_last);
}

struct netent *
getnetbyname_p(const char *name, struct net_data *net_data) {
	struct irs_nw *nw;
	struct netent *np;
	char **nap;

	if (!net_data || !(nw = net_data->nw))
		return (NULL);
	if (net_data->nw_stayopen && net_data->nw_last) {
		if (!strcmp(net_data->nw_last->n_name, name))
			return (net_data->nw_last);
		for (nap = net_data->nw_last->n_aliases; nap && *nap; nap++)
			if (!strcmp(name, *nap))
				return (net_data->nw_last);
	}
	if ((np = fakeaddr(name, AF_INET, net_data)) != NULL)
		return (np);
	net_data->nww_last = (*nw->byname)(nw, name, AF_INET);
	net_data->nw_last = nw_to_net(net_data->nww_last, net_data);
	if (!net_data->nw_stayopen)
		endnetent();
	return (net_data->nw_last);
}

struct netent *
getnetbyaddr_p(unsigned long net, int type, struct net_data *net_data) {
	struct irs_nw *nw;
	u_char addr[4];
	int bits;

	if (!net_data || !(nw = net_data->nw))
		return (NULL);
	if (net_data->nw_stayopen && net_data->nw_last)
		if (type == net_data->nw_last->n_addrtype &&
		    net == net_data->nw_last->n_net)
			return (net_data->nw_last);

	/* cannonize net(host order) */
	if (net < 256UL) {
		net <<= 24;
		bits = 8;
	} else if (net < 65536UL) {
		net <<= 16;
		bits = 16;
	} else if (net < 16777216UL) {
		net <<= 8;
		bits = 24;
	} else
		bits = 32;

	/* convert to net order */
	addr[0] = (0xFF000000 & net) >> 24;
	addr[1] = (0x00FF0000 & net) >> 16;
	addr[2] = (0x0000FF00 & net) >> 8;
	addr[3] = (0x000000FF & net);

	/* reduce bits to as close to natural number as possible */
	if ((bits == 32) && (addr[0] < 224) && (addr[3] == 0)) {
		if ((addr[0] < 192) && (addr[2] == 0)) {
			if ((addr[0] < 128) && (addr[1] == 0))
				bits = 8;
			else
				bits = 16;
		} else {
			bits = 24;
		}
	}

	net_data->nww_last = (*nw->byaddr)(nw, addr, bits, AF_INET);
	net_data->nw_last = nw_to_net(net_data->nww_last, net_data);
	if (!net_data->nw_stayopen)
		endnetent();
	return (net_data->nw_last);
}




void
setnetent_p(int stayopen, struct net_data *net_data) {
	struct irs_nw *nw;

	if (!net_data || !(nw = net_data->nw))
		return;
	freepvt(net_data);
	(*nw->rewind)(nw);
	net_data->nw_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
}

void
endnetent_p(struct net_data *net_data) {
	struct irs_nw *nw;

	if ((net_data != NULL) && ((nw	= net_data->nw) != NULL))
		(*nw->minimize)(nw);
}

/* Private */

static struct net_data *
init() {
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->nw) {
		net_data->nw = (*net_data->irs->nw_map)(net_data->irs);

		if (!net_data->nw || !net_data->res) {
 error:		
			errno = EIO;
			return (NULL);
		}
		(*net_data->nw->res_set)(net_data->nw, net_data->res, NULL);
	}
	
	return (net_data);
}

static void
freepvt(struct net_data *net_data) {
	if (net_data->nw_data) {
		free(net_data->nw_data);
		net_data->nw_data = NULL;
	}
}

static struct netent *
fakeaddr(const char *name, int af, struct net_data *net_data) {
	struct pvt *pvt;
	const char *cp;
	u_long tmp;

	if (af != AF_INET) {
		/* XXX should support IPv6 some day */
		errno = EAFNOSUPPORT;
		RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
		return (NULL);
	}
	if (!isascii((unsigned char)(name[0])) ||
	    !isdigit((unsigned char)(name[0])))
		return (NULL);
	for (cp = name; *cp; ++cp)
		if (!isascii(*cp) || (!isdigit((unsigned char)*cp) && *cp != '.'))
			return (NULL);
	if (*--cp == '.')
		return (NULL);

	/* All-numeric, no dot at the end. */

	tmp = inet_network(name);
	if (tmp == INADDR_NONE) {
		RES_SET_H_ERRNO(net_data->res, HOST_NOT_FOUND);
		return (NULL);
	}

	/* Valid network number specified.
	 * Fake up a netent as if we'd actually
	 * done a lookup.
	 */
	freepvt(net_data);
	net_data->nw_data = malloc(sizeof (struct pvt));
	if (!net_data->nw_data) {
		errno = ENOMEM;
		RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
		return (NULL);
	}
	pvt = net_data->nw_data;

	strncpy(pvt->name, name, MAXDNAME);
	pvt->name[MAXDNAME] = '\0';
	pvt->netent.n_name = pvt->name;
	pvt->netent.n_addrtype = AF_INET;
	pvt->netent.n_aliases = pvt->aliases;
	pvt->aliases[0] = NULL;
	pvt->netent.n_net = tmp;

	return (&pvt->netent);
}

static struct netent *
nw_to_net(struct nwent *nwent, struct net_data *net_data) {
	struct pvt *pvt;
	u_long addr = 0;
	int i;
	int msbyte;

	if (!nwent || nwent->n_addrtype != AF_INET)
		return (NULL);
	freepvt(net_data);
	net_data->nw_data = malloc(sizeof (struct pvt));
	if (!net_data->nw_data) {
		errno = ENOMEM;
		RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
		return (NULL);
	}
	pvt = net_data->nw_data;
	pvt->netent.n_name = nwent->n_name;
	pvt->netent.n_aliases = nwent->n_aliases;
	pvt->netent.n_addrtype = nwent->n_addrtype;

/*%
 * What this code does: Converts net addresses from network to host form.
 *
 * msbyte: the index of the most significant byte in the n_addr array.
 *
 * Shift bytes in significant order into addr. When all signicant
 * bytes are in, zero out bits in the LSB that are not part of the network.
 */
	msbyte = nwent->n_length / 8 +
		((nwent->n_length % 8) != 0 ? 1 : 0) - 1;
	for (i = 0; i <= msbyte; i++)
		addr = (addr << 8) | ((unsigned char *)nwent->n_addr)[i];
	i = (32 - nwent->n_length) % 8;
	if (i != 0)
		addr &= ~((1 << (i + 1)) - 1);
	pvt->netent.n_net = addr;
	return (&pvt->netent);
}

#endif /*__BIND_NOSTATIC*/

/*! \file */
