/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: gethost.c,v 1.29.206.1 2004/03/06 08:15:30 marka Exp $ */

#include <config.h>

#include <errno.h>
#include <string.h>

#include <lwres/net.h>
#include <lwres/netdb.h>

#include "assert_p.h"

#define LWRES_ALIGNBYTES (sizeof(char *) - 1)
#define LWRES_ALIGN(p) \
	(((unsigned long)(p) + LWRES_ALIGNBYTES) &~ LWRES_ALIGNBYTES)

static struct hostent *he = NULL;
static int copytobuf(struct hostent *, struct hostent *, char *, int);

struct hostent *
lwres_gethostbyname(const char *name) {

	if (he != NULL)
		lwres_freehostent(he);

	he = lwres_getipnodebyname(name, AF_INET, 0, &lwres_h_errno);
	return (he);
}

struct hostent *
lwres_gethostbyname2(const char *name, int af) {
	if (he != NULL)
		lwres_freehostent(he);

	he = lwres_getipnodebyname(name, af, 0, &lwres_h_errno);
	return (he);
}

struct hostent *
lwres_gethostbyaddr(const char *addr, int len, int type) {

	if (he != NULL)
		lwres_freehostent(he);

	he = lwres_getipnodebyaddr(addr, len, type, &lwres_h_errno);
	return (he);
}

struct hostent *
lwres_gethostent(void) {
	if (he != NULL)
		lwres_freehostent(he);

	return (NULL);
}

void
lwres_sethostent(int stayopen) {
	/*
	 * Empty.
	 */
	UNUSED(stayopen);
}

void
lwres_endhostent(void) {
	/*
	 * Empty.
	 */
}

struct hostent *
lwres_gethostbyname_r(const char *name, struct hostent *resbuf,
		char *buf, int buflen, int *error)
{
	struct hostent *he;
	int res;

	he = lwres_getipnodebyname(name, AF_INET, 0, error);
	if (he == NULL)
		return (NULL);
	res = copytobuf(he, resbuf, buf, buflen);
	lwres_freehostent(he);
	if (res != 0) {
		errno = ERANGE;
		return (NULL);
	}
	return (resbuf);
}

struct hostent  *
lwres_gethostbyaddr_r(const char *addr, int len, int type,
		      struct hostent *resbuf, char *buf, int buflen,
		      int *error)
{
	struct hostent *he;
	int res;

	he = lwres_getipnodebyaddr(addr, len, type, error);
	if (he == NULL)
		return (NULL);
	res = copytobuf(he, resbuf, buf, buflen);
	lwres_freehostent(he);
	if (res != 0) {
		errno = ERANGE;
		return (NULL);
	}
	return (resbuf);
}

struct hostent  *
lwres_gethostent_r(struct hostent *resbuf, char *buf, int buflen, int *error) {
	UNUSED(resbuf);
	UNUSED(buf);
	UNUSED(buflen);
	*error = 0;
	return (NULL);
}

void
lwres_sethostent_r(int stayopen) {
	/*
	 * Empty.
	 */
	UNUSED(stayopen);
}

void
lwres_endhostent_r(void) {
	/*
	 * Empty.
	 */
}

static int
copytobuf(struct hostent *he, struct hostent *hptr, char *buf, int buflen) {
        char *cp;
        char **ptr;
        int i, n;
        int nptr, len;

        /*
	 * Find out the amount of space required to store the answer.
	 */
        nptr = 2; /* NULL ptrs */
        len = (char *)LWRES_ALIGN(buf) - buf;
        for (i = 0; he->h_addr_list[i]; i++, nptr++) {
                len += he->h_length;
        }
        for (i = 0; he->h_aliases[i]; i++, nptr++) {
                len += strlen(he->h_aliases[i]) + 1;
        }
        len += strlen(he->h_name) + 1;
        len += nptr * sizeof(char*);

        if (len > buflen) {
                return (-1);
        }

        /*
	 * Copy address size and type.
	 */
        hptr->h_addrtype = he->h_addrtype;
        n = hptr->h_length = he->h_length;

        ptr = (char **)LWRES_ALIGN(buf);
        cp = (char *)LWRES_ALIGN(buf) + nptr * sizeof(char *);

        /*
	 * Copy address list.
	 */
        hptr->h_addr_list = ptr;
        for (i = 0; he->h_addr_list[i]; i++, ptr++) {
                memcpy(cp, he->h_addr_list[i], n);
                hptr->h_addr_list[i] = cp;
                cp += n;
        }
        hptr->h_addr_list[i] = NULL;
        ptr++;

        /*
	 * Copy official name.
	 */
        n = strlen(he->h_name) + 1;
        strcpy(cp, he->h_name);
        hptr->h_name = cp;
        cp += n;

        /*
	 * Copy aliases.
	 */
        hptr->h_aliases = ptr;
        for (i = 0; he->h_aliases[i]; i++) {
                n = strlen(he->h_aliases[i]) + 1;
                strcpy(cp, he->h_aliases[i]);
                hptr->h_aliases[i] = cp;
                cp += n;
        }
        hptr->h_aliases[i] = NULL;

        return (0);
}
