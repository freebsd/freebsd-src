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
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
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

static struct hostent *gethostbyname_internal(const char *, int);

/* Host lookup order if nsswitch.conf is broken or nonexistant */
static const ns_src default_src[] = {
	{ NSSRC_FILES, NS_SUCCESS },
	{ NSSRC_DNS, NS_SUCCESS },
	{ 0 }
};

struct hostent *
gethostbyname(const char *name)
{
	struct hostent *hp;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	if (_res.options & RES_USE_INET6) {
		hp = gethostbyname_internal(name, AF_INET6);
		if (hp)
			return hp;
	}
	return gethostbyname_internal(name, AF_INET);
}

struct hostent *
gethostbyname2(const char *name, int af)
{
	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	return gethostbyname_internal(name, af);
}

static struct hostent *
gethostbyname_internal(const char *name, int af)
{
	const char *cp;
	char *bp, *ep;
	struct hostent *hp = 0;
	int size, rval;
	char abuf[MAXDNAME];

	static struct hostent host;
	static char *h_addr_ptrs[2];
	static char *host_aliases[1];
	static char hostbuf[MAXDNAME + IN6ADDRSZ + sizeof(uint32_t)];
	static uint32_t host_addr[4];	/* IPv4 or IPv6 */
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
		return NULL;
	}

	host.h_addrtype = af;
	host.h_length = size;

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
				if (inet_pton(af, name, host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return NULL;
				}
				strncpy(hostbuf, name, MAXDNAME);
				hostbuf[MAXDNAME] = '\0';
				bp = hostbuf + MAXDNAME;
				ep = hostbuf + sizeof hostbuf;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				if (_res.options & RES_USE_INET6)
					_map_v4v6_hostent(&host, &bp, &ep);
				h_errno = NETDB_SUCCESS;
				return &host;
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
				if (inet_pton(af, name, host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return NULL;
				}
				strncpy(hostbuf, name, MAXDNAME);
				hostbuf[MAXDNAME] = '\0';
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				h_errno = NETDB_SUCCESS;
				return &host;
			}
			if (!isxdigit((u_char)*cp) && *cp != ':' && *cp != '.')
				break;
		}

	rval = _nsdispatch((void *)&hp, dtab, NSDB_HOSTS, "gethostbyname",
	    default_src, name, af);

	if (rval != NS_SUCCESS)
		return NULL;
	else
		return hp;
}

struct hostent *
gethostbyaddr(const char *addr, int len, int type)
{
	struct hostent *hp = 0;
	int rval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_gethostbyaddr, NULL)
		{ NSSRC_DNS, _dns_gethostbyaddr, NULL },
		NS_NIS_CB(_nis_gethostbyaddr, NULL) /* force -DHESIOD */
		{ 0 }
	};       

	rval = _nsdispatch((void *)&hp, dtab, NSDB_HOSTS, "gethostbyaddr",
			  default_src, addr, len, type);

	if (rval != NS_SUCCESS)
		return NULL;
	else
		return hp;
}

void
sethostent(stayopen)
	int stayopen;
{
	_sethosthtent(stayopen);
	_sethostdnsent(stayopen);
}

void
endhostent()
{
	_endhosthtent();
	_endhostdnsent();
}
