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
static char rcsid[] = "$Id: getnetent.c,v 1.10 1997/12/04 04:57:53 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
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

static struct irs_nw *	init(void);
static struct netent *	nw_to_net(struct nwent *);
static void		freepvt(void);
static struct netent *	fakeaddr(const char *, int af);

/* Portability */

#ifndef INADDR_NONE
# define INADDR_NONE 0xffffffff
#endif

/* Public */

struct netent *
getnetent() {
	struct irs_nw *nw = init();
	
	if (!nw)
		return (NULL);
	net_data.nw_last = nw_to_net((*nw->next)(nw));
	return (net_data.nw_last);
}

struct netent *
getnetbyname(const char *name) {
	struct irs_nw *nw = init();
	struct netent *np;
	char **nap;
	
	if (!nw)
		return (NULL);
	if (net_data.nw_stayopen && net_data.nw_last) {
		if (!strcmp(net_data.nw_last->n_name, name))
			return (net_data.nw_last);
		for (nap = net_data.nw_last->n_aliases; nap && *nap; nap++)
			if (!strcmp(name, *nap))
				return (net_data.nw_last);
	}
	if ((np = fakeaddr(name, AF_INET)) != NULL)
		return (np);
	net_data.nw_last = nw_to_net((*nw->byname)(nw, name, AF_INET));
	if (!net_data.nw_stayopen)
		endnetent();
	return (net_data.nw_last);
}

struct netent *
getnetbyaddr(unsigned long net, int type) {
	struct irs_nw *nw = init();
	u_char addr[4];
	int bits;
	
	if (!nw)
		return (NULL);
	if (net_data.nw_stayopen && net_data.nw_last)
		if (type == net_data.nw_last->n_addrtype &&
		    net == net_data.nw_last->n_net)
			return (net_data.nw_last);
	
	addr[3] = (0xFF000000 & net) >> 24;
	addr[2] = (0x00FF0000 & net) >> 16;
	addr[1] = (0x0000FF00 & net) >> 8;
	addr[0] = (0x000000FF & net);
	
	/* Use the old class rules to figure out the network bits. */
	if (addr[3] >= 240)
		bits = 32;
	else if (addr[3] >= 224)
		bits = 4;
	else if (addr[3] >= 192)
		bits = 24;
	else if (addr[3] >= 128)
		bits = 16;
	else
		bits = 8;
	
	net_data.nw_last = nw_to_net((*nw->byaddr)(nw, addr, bits, AF_INET));
	if (!net_data.nw_stayopen)
		endnetent();
	return (net_data.nw_last);
}

void
setnetent(int stayopen) {
	struct irs_nw *nw = init();
	
	if (!nw)
		return;
	freepvt();
	(*nw->rewind)(nw);
	net_data.nw_stayopen = (stayopen != 0);
}

void
endnetent() {
	struct irs_nw *nw = init();

	if (nw != NULL)
		(*nw->minimize)(nw);
}

/* Private */

static struct irs_nw *
init() {
	if (!net_data_init())
		goto error;
	if (!net_data.nw)
		net_data.nw = (*net_data.irs->nw_map)(net_data.irs);
	if (!net_data.nw) {
 error:		
		errno = EIO;
		return (NULL);
	}
	return (net_data.nw);
}

static void
freepvt() {
	if (net_data.nw_data) {
		free(net_data.nw_data);
		net_data.nw_data = NULL;
	}
}

static struct netent *
fakeaddr(const char *name, int af) {
	struct pvt *pvt;
	const char *cp;
	u_long tmp;

	if (af != AF_INET) {
		/* XXX should support IPv6 some day */
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	if (!isascii(name[0]) || !isdigit(name[0]))
		return (NULL);
	for (cp = name; *cp; ++cp)
		if (!isascii(*cp) || (!isdigit(*cp) && *cp != '.'))
			return (NULL);
	if (*--cp == '.')
		return (NULL);

	/* All-numeric, no dot at the end. */

	tmp = inet_network(name);
	if (tmp == INADDR_NONE) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}

	/* Valid network number specified.
	 * Fake up a netent as if we'd actually
	 * done a lookup.
	 */
	freepvt();
	net_data.nw_data = malloc(sizeof(struct pvt));
	if (!net_data.nw_data) {
		errno = ENOMEM;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt = net_data.nw_data;

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
nw_to_net(struct nwent *nwent) {
	struct pvt *pvt;
	u_long addr = 0;
	int i; 
	int msbyte;

	if (!nwent || nwent->n_addrtype != AF_INET)
		return (NULL);
	freepvt();
	net_data.nw_data = malloc(sizeof(struct pvt));
	if (!net_data.nw_data) {
		errno = ENOMEM;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt = net_data.nw_data;
	pvt->netent.n_name = nwent->n_name;
	pvt->netent.n_aliases = nwent->n_aliases;
	pvt->netent.n_addrtype = nwent->n_addrtype;
	
/*
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


