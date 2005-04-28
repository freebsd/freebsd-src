/*-
 * Copyright (c) 1994, Garrett Wollman
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include "reentrant.h"
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <nsswitch.h>
#include <arpa/nameser.h>		/* XXX hack for _res */
#include <resolv.h>			/* XXX hack for _res */
#include "un-namespace.h"
#include "netdb_private.h"

extern int _ht_gethostbyname(void *, void *, va_list);
extern int _dns_gethostbyname(void *, void *, va_list);
extern int _nis_gethostbyname(void *, void *, va_list);
extern int _ht_gethostbyaddr(void *, void *, va_list);
extern int _dns_gethostbyaddr(void *, void *, va_list);
extern int _nis_gethostbyaddr(void *, void *, va_list);
extern const char *_res_hostalias(const char *, char *, size_t);

static int gethostbyname_internal(const char *, int, struct hostent *,
    struct hostent_data *);

/* Host lookup order if nsswitch.conf is broken or nonexistant */
static const ns_src default_src[] = {
	{ NSSRC_FILES, NS_SUCCESS },
	{ NSSRC_DNS, NS_SUCCESS },
	{ 0 }
};

static struct hostdata hostdata;
static thread_key_t hostdata_key;
static once_t hostdata_init_once = ONCE_INITIALIZER;
static int hostdata_thr_keycreated = 0;

static void
hostdata_free(void *ptr)
{
	struct hostdata *hd = ptr;

	if (hd == NULL)
		return;
	hd->data.stayopen = 0;
	_endhosthtent(&hd->data);
	free(hd);
}

static void
hostdata_keycreate(void)
{
	hostdata_thr_keycreated =
	    (thr_keycreate(&hostdata_key, hostdata_free) == 0);
}

struct hostdata *
__hostdata_init(void)
{
	struct hostdata *hd;

	if (thr_main() != 0)
		return &hostdata;
	if (thr_once(&hostdata_init_once, hostdata_keycreate) != 0 ||
	    !hostdata_thr_keycreated)
		return NULL;
	if ((hd = thr_getspecific(hostdata_key)) != NULL)
		return hd;
	if ((hd = calloc(1, sizeof(*hd))) == NULL)
		return NULL;
	if (thr_setspecific(hostdata_key, hd) == 0)
		return hd;
	free(hd);
	return NULL;
}

int
gethostbyname_r(const char *name, struct hostent *he, struct hostent_data *hed)
{
	int error;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return -1;
	}
	if (_res.options & RES_USE_INET6) {
		error = gethostbyname_internal(name, AF_INET6, he, hed);
		if (error == 0)
			return 0;
	}
	return gethostbyname_internal(name, AF_INET, he, hed);
}

int
gethostbyname2_r(const char *name, int af, struct hostent *he,
    struct hostent_data *hed)
{
	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return -1;
	}
	return gethostbyname_internal(name, af, he, hed);
}

static int
gethostbyname_internal(const char *name, int af, struct hostent *he,
    struct hostent_data *hed)
{
	const char *cp;
	char *bp, *ep;
	int size, rval;
	char abuf[MAXDNAME];

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_gethostbyname, NULL)
		{ NSSRC_DNS, _dns_gethostbyname, NULL },
		NS_NIS_CB(_nis_gethostbyname, NULL) /* force -DHESIOD */
		{ 0 }
	};

	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		break;
	default:
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return -1;
	}

	he->h_addrtype = af;
	he->h_length = size;

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_query() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') &&
	    (cp = _res_hostalias(name, abuf, sizeof abuf)))
		name = cp;

	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit((u_char)name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (inet_pton(af, name, hed->host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return -1;
				}
				strncpy(hed->hostbuf, name, MAXDNAME);
				hed->hostbuf[MAXDNAME] = '\0';
				bp = hed->hostbuf + MAXDNAME + 1;
				ep = hed->hostbuf + sizeof hed->hostbuf;
				he->h_name = hed->hostbuf;
				he->h_aliases = hed->host_aliases;
				hed->host_aliases[0] = NULL;
				hed->h_addr_ptrs[0] = (char *)hed->host_addr;
				hed->h_addr_ptrs[1] = NULL;
				he->h_addr_list = hed->h_addr_ptrs;
				if (_res.options & RES_USE_INET6)
					_map_v4v6_hostent(he, &bp, &ep);
				h_errno = NETDB_SUCCESS;
				return 0;
			}
			if (!isdigit((u_char)*cp) && *cp != '.')
				break;
		}
	if ((isxdigit((u_char)name[0]) && strchr(name, ':') != NULL) ||
	    name[0] == ':')
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-IPv6-legal, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (inet_pton(af, name, hed->host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return -1;
				}
				strncpy(hed->hostbuf, name, MAXDNAME);
				hed->hostbuf[MAXDNAME] = '\0';
				he->h_name = hed->hostbuf;
				he->h_aliases = hed->host_aliases;
				hed->host_aliases[0] = NULL;
				hed->h_addr_ptrs[0] = (char *)hed->host_addr;
				hed->h_addr_ptrs[1] = NULL;
				he->h_addr_list = hed->h_addr_ptrs;
				h_errno = NETDB_SUCCESS;
				return 0;
			}
			if (!isxdigit((u_char)*cp) && *cp != ':' && *cp != '.')
				break;
		}

	rval = _nsdispatch(NULL, dtab, NSDB_HOSTS, "gethostbyname",
	    default_src, name, af, he, hed);

	return (rval == NS_SUCCESS) ? 0 : -1;
}

int
gethostbyaddr_r(const char *addr, int len, int af, struct hostent *he,
    struct hostent_data *hed)
{
	int rval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_gethostbyaddr, NULL)
		{ NSSRC_DNS, _dns_gethostbyaddr, NULL },
		NS_NIS_CB(_nis_gethostbyaddr, NULL) /* force -DHESIOD */
		{ 0 }
	};

	rval = _nsdispatch(NULL, dtab, NSDB_HOSTS, "gethostbyaddr",
	    default_src, addr, len, af, he, hed);

	return (rval == NS_SUCCESS) ? 0 : -1;
}

void
sethostent_r(int stayopen, struct hostent_data *hed)
{
	_sethosthtent(stayopen, hed);
	_sethostdnsent(stayopen);
}

void
endhostent_r(struct hostent_data *hed)
{
	_endhosthtent(hed);
	_endhostdnsent();
}

struct hostent *
gethostbyname(const char *name)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return NULL;
	if (gethostbyname_r(name, &hd->host, &hd->data) != 0)
		return NULL;
	return &hd->host;
}

struct hostent *
gethostbyname2(const char *name, int af)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return NULL;
	if (gethostbyname2_r(name, af, &hd->host, &hd->data) != 0)
		return NULL;
	return &hd->host;
}

struct hostent *
gethostbyaddr(const char *addr, int len, int af)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return NULL;
	if (gethostbyaddr_r(addr, len, af, &hd->host, &hd->data) != 0)
		return NULL;
	return &hd->host;
}

void
sethostent(int stayopen)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return;
	sethostent_r(stayopen, &hd->data);
}

void
endhostent(void)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return;
	endhostent_r(&hd->data);
}
