/*
 * Copyright (c) 1996,1997 by Internet Software Consortium.
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
static char rcsid[] = "$Id: gethostent.c,v 1.13 1997/12/04 04:57:52 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "irs_data.h"

/* Definitions */

struct pvt {
	char *		aliases[1];
	char *		addrs[2];
	char		addr[NS_IN6ADDRSZ];
	char		name[NS_MAXDNAME + 1];
	struct hostent	host;
};

/* Forward */

static struct irs_ho *	init(void);
static void		freepvt(void);
static struct hostent *	fakeaddr(const char *, int);

/* Public */

struct hostent *
gethostbyname(const char *name) {
	struct hostent *hp;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (_res.options & RES_USE_INET6) {
		hp = gethostbyname2(name, AF_INET6);
		if (hp)
			return (hp);
	}
	return (gethostbyname2(name, AF_INET));
}

struct hostent *
gethostbyname2(const char *name, int af) {
	struct irs_ho *ho = init();
	struct hostent *hp;
	const char *cp;
	char **hap;

	if (!ho)
		return (NULL);
	if (net_data.ho_stayopen && net_data.ho_last) {
		if (!strcasecmp(name, net_data.ho_last->h_name))
			return (net_data.ho_last);
		for (hap = net_data.ho_last->h_aliases; hap && *hap; hap++)
			if (!strcasecmp(name, *hap))
				return (net_data.ho_last);
	}
	if (!strchr(name, '.') && (cp = hostalias(name)))
		name = cp;
	if ((hp = fakeaddr(name, af)) != NULL)
		return (hp);
	net_data.ho_last = (*ho->byname2)(ho, name, af);
	if (!net_data.ho_stayopen)
		endhostent();
	return (net_data.ho_last);
}

struct hostent *
gethostbyaddr(const char *addr, int len, int af) {
	struct irs_ho *ho = init();
	char **hap;

	if (!ho)
		return (NULL);
	if (net_data.ho_stayopen && net_data.ho_last &&
	    net_data.ho_last->h_length == len)
		for (hap = net_data.ho_last->h_addr_list;
		     hap && *hap;
		     hap++)
			if (!memcmp(addr, *hap, len))
				return (net_data.ho_last);
	net_data.ho_last = (*ho->byaddr)(ho, addr, len, af);
	if (!net_data.ho_stayopen)
		endhostent();
	return (net_data.ho_last);
}

struct hostent *
gethostent() {
	struct irs_ho *ho = init();

	if (!ho)
		return (NULL);
	net_data.ho_last = (*ho->next)(ho);
	return (net_data.ho_last);
}

void
sethostent(int stayopen) {
	struct irs_ho *ho = init();

	if (!ho)
		return;
	freepvt();
	(*ho->rewind)(ho);
	net_data.ho_stayopen = (stayopen != 0);
}

void
endhostent() {
	struct irs_ho *ho = init();

	if (ho != NULL)
		(*ho->minimize)(ho);
}

/* Private */

static struct irs_ho *
init() {
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (!net_data_init())
		goto error;
	if (!net_data.ho)
		net_data.ho = (*net_data.irs->ho_map)(net_data.irs);
	if (!net_data.ho) {
 error:		errno = EIO;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	return (net_data.ho);
}

static void
freepvt() {
	if (net_data.ho_data) {
		free(net_data.ho_data);
		net_data.ho_data = NULL;
	}
}

static struct hostent *
fakeaddr(const char *name, int af) {
	struct pvt *pvt;
	const char *cp;

	freepvt();
	net_data.ho_data = malloc(sizeof(struct pvt));
	if (!net_data.ho_data) {
		errno = ENOMEM;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt = net_data.ho_data;
	/*
	 * Unlike its forebear (inet_aton), our friendly inet_pton() is strict
	 * in its interpretation of its input, and it will only return "1" if
	 * the input string is a formally valid (and thus unambiguous with
	 * respect to host names) internet address specification for this AF.
	 *
	 * This means "telnet 0xdeadbeef" and "telnet 127.1" are dead now.
	 */
	if (inet_pton(af, name, pvt->addr) != 1) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	strncpy(pvt->name, name, NS_MAXDNAME);
	pvt->name[NS_MAXDNAME] = '\0';
	pvt->host.h_addrtype = af;
	switch (af) {
	case AF_INET:
		pvt->host.h_length = NS_INADDRSZ;
		break;
	case AF_INET6:
		pvt->host.h_length = NS_IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt->host.h_name = pvt->name;
	pvt->host.h_aliases = pvt->aliases;
	pvt->aliases[0] = NULL;
	pvt->addrs[0] = (char *)pvt->addr;
	pvt->addrs[1] = NULL;
	pvt->host.h_addr_list = pvt->addrs;
	if (af == AF_INET && (_res.options & RES_USE_INET6))
		map_v4v6_address(pvt->addr, pvt->addr);
	h_errno = NETDB_SUCCESS;
	return (&pvt->host);
}

#ifdef grot	/* for future use in gethostbyaddr(), for "SUNSECURITY" */
	struct hostent *rhp;
	char **haddr;
	u_long old_options;
	char hname2[MAXDNAME+1];

	if (af == AF_INET) {
	    /*
	     * turn off search as the name should be absolute,
	     * 'localhost' should be matched by defnames
	     */
	    strncpy(hname2, hp->h_name, MAXDNAME);
	    hname2[MAXDNAME] = '\0';
	    old_options = _res.options;
	    _res.options &= ~RES_DNSRCH;
	    _res.options |= RES_DEFNAMES;
	    if (!(rhp = gethostbyname(hname2))) {
		_res.options = old_options;
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	    _res.options = old_options;
	    for (haddr = rhp->h_addr_list; *haddr; haddr++)
		if (!memcmp(*haddr, addr, INADDRSZ))
			break;
	    if (!*haddr) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	}
#endif /* grot */
