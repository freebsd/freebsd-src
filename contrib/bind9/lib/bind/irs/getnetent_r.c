/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-1999 by Internet Software Consortium.
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

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: getnetent_r.c,v 1.4.18.2 2005-09-03 12:45:14 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS)
	static int getnetent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <port_after.h>

#ifdef NET_R_RETURN

static NET_R_RETURN 
copy_netent(struct netent *, struct netent *, NET_R_COPY_ARGS);

NET_R_RETURN
getnetbyname_r(const char *name,  struct netent *nptr, NET_R_ARGS) {
	struct netent *ne = getnetbyname(name);
#ifdef NET_R_SETANSWER
	int n = 0;

	if (ne == NULL || (n = copy_netent(ne, nptr, NET_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = ne;
	if (ne == NULL)
		*h_errnop = h_errno;
	return (n);
#else
	if (ne == NULL)
		return (NET_R_BAD);

	return (copy_netent(ne, nptr, NET_R_COPY));
#endif
}

#ifndef GETNETBYADDR_ADDR_T
#define GETNETBYADDR_ADDR_T long
#endif
NET_R_RETURN
getnetbyaddr_r(GETNETBYADDR_ADDR_T addr, int type, struct netent *nptr, NET_R_ARGS) {
	struct netent *ne = getnetbyaddr(addr, type);
#ifdef NET_R_SETANSWER
	int n = 0;

	if (ne == NULL || (n = copy_netent(ne, nptr, NET_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = ne;
	if (ne == NULL)
		*h_errnop = h_errno;
	return (n);
#else

	if (ne == NULL)
		return (NET_R_BAD);

	return (copy_netent(ne, nptr, NET_R_COPY));
#endif
}

/*%
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

NET_R_RETURN
getnetent_r(struct netent *nptr, NET_R_ARGS) {
	struct netent *ne = getnetent();
#ifdef NET_R_SETANSWER
	int n = 0;

	if (ne == NULL || (n = copy_netent(ne, nptr, NET_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = ne;
	if (ne == NULL)
		*h_errnop = h_errno;
	return (n);
#else

	if (ne == NULL)
		return (NET_R_BAD);

	return (copy_netent(ne, nptr, NET_R_COPY));
#endif
}

NET_R_SET_RETURN
#ifdef NET_R_ENT_ARGS
setnetent_r(int stay_open, NET_R_ENT_ARGS)
#else
setnetent_r(int stay_open)
#endif
{
#ifdef NET_R_ENT_ARGS
	UNUSED(ndptr);
#endif
	setnetent(stay_open);
#ifdef NET_R_SET_RESULT
	return (NET_R_SET_RESULT);
#endif
}

NET_R_END_RETURN
#ifdef NET_R_ENT_ARGS
endnetent_r(NET_R_ENT_ARGS)
#else
endnetent_r()
#endif
{
#ifdef NET_R_ENT_ARGS
	UNUSED(ndptr);
#endif
	endnetent();
	NET_R_END_RESULT(NET_R_OK);
}

/* Private */

#ifndef NETENT_DATA
static NET_R_RETURN
copy_netent(struct netent *ne, struct netent *nptr, NET_R_COPY_ARGS) {
	char *cp;
	int i, n;
	int numptr, len;

	/* Find out the amount of space required to store the answer. */
	numptr = 1; /*%< NULL ptr */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; ne->n_aliases[i]; i++, numptr++) {
		len += strlen(ne->n_aliases[i]) + 1;
	}
	len += strlen(ne->n_name) + 1;
	len += numptr * sizeof(char*);
	
	if (len > (int)buflen) {
		errno = ERANGE;
		return (NET_R_BAD);
	}

	/* copy net value and type */
	nptr->n_addrtype = ne->n_addrtype;
	nptr->n_net = ne->n_net;

	cp = (char *)ALIGN(buf) + numptr * sizeof(char *);

	/* copy official name */
	n = strlen(ne->n_name) + 1;
	strcpy(cp, ne->n_name);
	nptr->n_name = cp;
	cp += n;

	/* copy aliases */
	nptr->n_aliases = (char **)ALIGN(buf);
	for (i = 0 ; ne->n_aliases[i]; i++) {
		n = strlen(ne->n_aliases[i]) + 1;
		strcpy(cp, ne->n_aliases[i]);
		nptr->n_aliases[i] = cp;
		cp += n;
	}
	nptr->n_aliases[i] = NULL;

	return (NET_R_OK);
}
#else /* !NETENT_DATA */
static int
copy_netent(struct netent *ne, struct netent *nptr, NET_R_COPY_ARGS) {
	char *cp, *eob;
	int i, n;

	/* copy net value and type */
	nptr->n_addrtype = ne->n_addrtype;
	nptr->n_net = ne->n_net;

	/* copy official name */
	cp = ndptr->line;
	eob = ndptr->line + sizeof(ndptr->line);
	if ((n = strlen(ne->n_name) + 1) < (eob - cp)) {
		strcpy(cp, ne->n_name);
		nptr->n_name = cp;
		cp += n;
	} else {
		return (-1);
	}

	/* copy aliases */
	i = 0;
	nptr->n_aliases = ndptr->net_aliases;
	while (ne->n_aliases[i] && i < (_MAXALIASES-1)) {
		if ((n = strlen(ne->n_aliases[i]) + 1) < (eob - cp)) {
			strcpy(cp, ne->n_aliases[i]);
			nptr->n_aliases[i] = cp;
			cp += n;
		} else {
			break;
		}
		i++;
	}
	nptr->n_aliases[i] = NULL;

	return (NET_R_OK);
}
#endif /* !NETENT_DATA */
#else /* NET_R_RETURN */
	static int getnetent_r_unknown_system = 0;
#endif /* NET_R_RETURN */
#endif /* !defined(_REENTRANT) || !defined(DO_PTHREADS) */
/*! \file */
