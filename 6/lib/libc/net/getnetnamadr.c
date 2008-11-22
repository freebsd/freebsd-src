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
#include "un-namespace.h"
#include "netdb_private.h"

extern int _ht_getnetbyname(void *, void *, va_list);
extern int _dns_getnetbyname(void *, void *, va_list);
extern int _nis_getnetbyname(void *, void *, va_list);
extern int _ht_getnetbyaddr(void *, void *, va_list);
extern int _dns_getnetbyaddr(void *, void *, va_list);
extern int _nis_getnetbyaddr(void *, void *, va_list);

/* Network lookup order if nsswitch.conf is broken or nonexistant */
static const ns_src default_src[] = {
	{ NSSRC_FILES, NS_SUCCESS },
	{ NSSRC_DNS, NS_SUCCESS },
	{ 0 }
};

NETDB_THREAD_ALLOC(netent_data)
NETDB_THREAD_ALLOC(netdata)

static void
netent_data_free(void *ptr)
{
	struct netent_data *ned = ptr;

	if (ned == NULL)
		return;
	ned->stayopen = 0;
	_endnethtent(ned);
	free(ned);
}

static void
netdata_free(void *ptr)
{
	free(ptr);
}

int
__copy_netent(struct netent *ne, struct netent *nptr, char *buf, size_t buflen)
{
	char *cp;
	int i, n;
	int numptr, len;

	/* Find out the amount of space required to store the answer. */
	numptr = 1; /* NULL ptr */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; ne->n_aliases[i]; i++, numptr++) {
		len += strlen(ne->n_aliases[i]) + 1;
	}
	len += strlen(ne->n_name) + 1;
	len += numptr * sizeof(char*);

	if (len > (int)buflen) {
		errno = ERANGE;
		return (-1);
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

	return (0);
}

int
getnetbyname_r(const char *name, struct netent *ne, char *buffer,
    size_t buflen, struct netent **result, int *h_errorp)
{
	int rval, ret_errno;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyname, NULL)
		{ NSSRC_DNS, _dns_getnetbyname, NULL },
		NS_NIS_CB(_nis_getnetbyname, NULL) /* force -DHESIOD */
		{ 0 }
	};

	rval = _nsdispatch((void *)result, dtab, NSDB_NETWORKS,
	    "getnetbyname_r", default_src, name, ne, buffer, buflen,
	    &ret_errno, h_errorp);

	return ((rval == NS_SUCCESS) ? 0 : -1);
}

int
getnetbyaddr_r(uint32_t addr, int af, struct netent *ne, char *buffer,
    size_t buflen, struct netent **result, int *h_errorp)
{
	int rval, ret_errno;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyaddr, NULL)
		{ NSSRC_DNS, _dns_getnetbyaddr, NULL },
		NS_NIS_CB(_nis_getnetbyaddr, NULL) /* force -DHESIOD */
		{ 0 }
	};

	rval = _nsdispatch((void *)result, dtab, NSDB_NETWORKS,
	    "getnetbyaddr_r", default_src, addr, af, ne, buffer, buflen,
	    &ret_errno, h_errorp);

	return ((rval == NS_SUCCESS) ? 0 : -1);
}

struct netent *
getnetbyname(const char *name)
{
	struct netdata *nd;
	struct netent *rval;
	int ret_h_errno;

	if ((nd = __netdata_init()) == NULL)
		return (NULL);
	if (getnetbyname_r(name, &nd->net, nd->data, sizeof(nd->data), &rval,
	    &ret_h_errno) != 0)
		return (NULL);
	return (rval);
}

struct netent *
getnetbyaddr(uint32_t addr, int af)
{
	struct netdata *nd;
	struct netent *rval;
	int ret_h_errno;

	if ((nd = __netdata_init()) == NULL)
		return (NULL);
	if (getnetbyaddr_r(addr, af, &nd->net, nd->data, sizeof(nd->data),
	    &rval, &ret_h_errno) != 0)
		return (NULL);
	return (rval);
}

void
setnetent(int stayopen)
{
	struct netent_data *ned;

	if ((ned = __netent_data_init()) == NULL)
		return;
	_setnethtent(stayopen, ned);
	_setnetdnsent(stayopen);
}

void
endnetent(void)
{
	struct netent_data *ned;

	if ((ned = __netent_data_init()) == NULL)
		return;
	_endnethtent(ned);
	_endnetdnsent();
}
