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
static const char rcsid[] = "$Id: gethostent_r.c,v 1.5.18.4 2005-09-03 12:45:14 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS)
	static int gethostent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <port_after.h>

#ifdef HOST_R_RETURN

static HOST_R_RETURN 
copy_hostent(struct hostent *, struct hostent *, HOST_R_COPY_ARGS);

HOST_R_RETURN
gethostbyname_r(const char *name,  struct hostent *hptr, HOST_R_ARGS) {
	struct hostent *he = gethostbyname(name);
#ifdef HOST_R_SETANSWER
	int n = 0;
#endif

#ifdef HOST_R_ERRNO
	HOST_R_ERRNO;
#endif

#ifdef HOST_R_SETANSWER
	if (he == NULL || (n = copy_hostent(he, hptr, HOST_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = hptr;
	
	return (n);
#else
	if (he == NULL)
		return (HOST_R_BAD);

	return (copy_hostent(he, hptr, HOST_R_COPY));
#endif
}

HOST_R_RETURN
gethostbyaddr_r(const char *addr, int len, int type,
		struct hostent *hptr, HOST_R_ARGS) {
	struct hostent *he = gethostbyaddr(addr, len, type);
#ifdef HOST_R_SETANSWER
	int n = 0;
#endif

#ifdef HOST_R_ERRNO
	HOST_R_ERRNO;
#endif

#ifdef HOST_R_SETANSWER
	if (he == NULL || (n = copy_hostent(he, hptr, HOST_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = hptr;
	
	return (n);
#else
	if (he == NULL)
		return (HOST_R_BAD);

	return (copy_hostent(he, hptr, HOST_R_COPY));
#endif
}

/*%
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

HOST_R_RETURN
gethostent_r(struct hostent *hptr, HOST_R_ARGS) {
	struct hostent *he = gethostent();
#ifdef HOST_R_SETANSWER
	int n = 0;
#endif

#ifdef HOST_R_ERRNO
	HOST_R_ERRNO;
#endif

#ifdef HOST_R_SETANSWER
	if (he == NULL || (n = copy_hostent(he, hptr, HOST_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = hptr;
	
	return (n);
#else
	if (he == NULL)
		return (HOST_R_BAD);

	return (copy_hostent(he, hptr, HOST_R_COPY));
#endif
}

HOST_R_SET_RETURN
#ifdef HOST_R_ENT_ARGS
sethostent_r(int stay_open, HOST_R_ENT_ARGS)
#else
sethostent_r(int stay_open)
#endif
{
#ifdef HOST_R_ENT_ARGS
	UNUSED(hdptr);
#endif
	sethostent(stay_open);
#ifdef	HOST_R_SET_RESULT
	return (HOST_R_SET_RESULT);
#endif
}

HOST_R_END_RETURN
#ifdef HOST_R_ENT_ARGS
endhostent_r(HOST_R_ENT_ARGS)
#else
endhostent_r(void)
#endif
{
#ifdef HOST_R_ENT_ARGS
	UNUSED(hdptr);
#endif
	endhostent();
	HOST_R_END_RESULT(HOST_R_OK);
}

/* Private */

#ifndef HOSTENT_DATA
static HOST_R_RETURN
copy_hostent(struct hostent *he, struct hostent *hptr, HOST_R_COPY_ARGS) {
	char *cp;
	char **ptr;
	int i, n;
	int nptr, len;

	/* Find out the amount of space required to store the answer. */
	nptr = 2; /*%< NULL ptrs */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; he->h_addr_list[i]; i++, nptr++) {
		len += he->h_length;
	}
	for (i = 0; he->h_aliases[i]; i++, nptr++) {
		len += strlen(he->h_aliases[i]) + 1;
	}
	len += strlen(he->h_name) + 1;
	len += nptr * sizeof(char*);
	
	if (len > buflen) {
		errno = ERANGE;
		return (HOST_R_BAD);
	}

	/* copy address size and type */
	hptr->h_addrtype = he->h_addrtype;
	n = hptr->h_length = he->h_length;

	ptr = (char **)ALIGN(buf);
	cp = (char *)ALIGN(buf) + nptr * sizeof(char *);

	/* copy address list */
	hptr->h_addr_list = ptr;
	for (i = 0; he->h_addr_list[i]; i++ , ptr++) {
		memcpy(cp, he->h_addr_list[i], n);
		hptr->h_addr_list[i] = cp;
		cp += n;
	}
	hptr->h_addr_list[i] = NULL;
	ptr++;

	/* copy official name */
	n = strlen(he->h_name) + 1;
	strcpy(cp, he->h_name);
	hptr->h_name = cp;
	cp += n;

	/* copy aliases */
	hptr->h_aliases = ptr;
	for (i = 0 ; he->h_aliases[i]; i++) {
		n = strlen(he->h_aliases[i]) + 1;
		strcpy(cp, he->h_aliases[i]);
		hptr->h_aliases[i] = cp;
		cp += n;
	}
	hptr->h_aliases[i] = NULL;

	return (HOST_R_OK);
}
#else /* !HOSTENT_DATA */
static int
copy_hostent(struct hostent *he, struct hostent *hptr, HOST_R_COPY_ARGS) {
	char *cp, *eob;
	int i, n;

	/* copy address size and type */
	hptr->h_addrtype = he->h_addrtype;
	n = hptr->h_length = he->h_length;

	/* copy up to first 35 addresses */
	i = 0;
	cp = hdptr->hostbuf;
	eob = hdptr->hostbuf + sizeof(hdptr->hostbuf);
	hptr->h_addr_list = hdptr->h_addr_ptrs;
	while (he->h_addr_list[i] && i < (_MAXADDRS)) {
		if (n < (eob - cp)) {
			memcpy(cp, he->h_addr_list[i], n);
			hptr->h_addr_list[i] = cp;
			cp += n;
		} else {
			break;
		}
		i++;
	}
	hptr->h_addr_list[i] = NULL;

	/* copy official name */
	if ((n = strlen(he->h_name) + 1) < (eob - cp)) {
		strcpy(cp, he->h_name);
		hptr->h_name = cp;
		cp += n;
	} else {
		return (-1);
	}

	/* copy aliases */
	i = 0;
	hptr->h_aliases = hdptr->host_aliases;
	while (he->h_aliases[i] && i < (_MAXALIASES-1)) {
		if ((n = strlen(he->h_aliases[i]) + 1) < (eob - cp)) {
			strcpy(cp, he->h_aliases[i]);
			hptr->h_aliases[i] = cp;
			cp += n;
		} else {
			break;
		}
		i++;
	}
	hptr->h_aliases[i] = NULL;

	return (HOST_R_OK);
}
#endif /* !HOSTENT_DATA */
#else /* HOST_R_RETURN */
	static int gethostent_r_unknown_system = 0;
#endif /* HOST_R_RETURN */
#endif /* !defined(_REENTRANT) || !defined(DO_PTHREADS) */
/*! \file */
