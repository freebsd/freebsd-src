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

static struct netdata netdata;
static thread_key_t netdata_key;
static once_t netdata_init_once = ONCE_INITIALIZER;
static int netdata_thr_keycreated = 0;

static void
netdata_free(void *ptr)
{
	struct netdata *nd = ptr;

	if (nd == NULL)
		return;
	nd->data.stayopen = 0;
	_endnethtent(&nd->data);
	free(nd);
}

static void
netdata_keycreate(void)
{
	netdata_thr_keycreated =
	    (thr_keycreate(&netdata_key, netdata_free) == 0);
}

struct netdata *
__netdata_init(void)
{
	struct netdata *nd;

	if (thr_main() != 0)
		return &netdata;
	if (thr_once(&netdata_init_once, netdata_keycreate) != 0 ||
	    !netdata_thr_keycreated)
		return NULL;
	if ((nd = thr_getspecific(netdata_key)) != NULL)
		return nd;
	if ((nd = calloc(1, sizeof(*nd))) == NULL)
		return NULL;
	if (thr_setspecific(netdata_key, nd) == 0)
		return nd;
	free(nd);
	return NULL;
}

int
getnetbyname_r(const char *name, struct netent *ne, struct netent_data *ned)
{
	int rval;


	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyname, NULL)
		{ NSSRC_DNS, _dns_getnetbyname, NULL },
		NS_NIS_CB(_nis_getnetbyname, NULL) /* force -DHESIOD */
		{ 0 }
	};

	rval = _nsdispatch(NULL, dtab, NSDB_NETWORKS, "getnetbyname",
	    default_src, name, ne, ned);

	return (rval == NS_SUCCESS) ? 0 : -1;
}

int
getnetbyaddr_r(uint32_t addr, int af, struct netent *ne,
    struct netent_data *ned)
{
	int rval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyaddr, NULL)
		{ NSSRC_DNS, _dns_getnetbyaddr, NULL },
		NS_NIS_CB(_nis_getnetbyaddr, NULL) /* force -DHESIOD */
		{ 0 }
	};

	rval = _nsdispatch(NULL, dtab, NSDB_NETWORKS, "getnetbyaddr",
	    default_src, addr, af, ne, ned);

	return (rval == NS_SUCCESS) ? 0 : -1;
}

void
setnetent_r(int stayopen, struct netent_data *ned)
{
	_setnethtent(stayopen, ned);
	_setnetdnsent(stayopen);
}

void
endnetent_r(struct netent_data *ned)
{
	_endnethtent(ned);
	_endnetdnsent();
}

struct netent *
getnetbyname(const char *name)
{
	struct netdata *nd;

	if ((nd = __netdata_init()) == NULL)
		return NULL;
	if (getnetbyname_r(name, &nd->net, &nd->data) != 0)
		return NULL;
	return &nd->net;
}

struct netent *
getnetbyaddr(uint32_t addr, int af)
{
	struct netdata *nd;

	if ((nd = __netdata_init()) == NULL)
		return NULL;
	if (getnetbyaddr_r(addr, af, &nd->net, &nd->data) != 0)
		return NULL;
	return &nd->net;
}

void
setnetent(int stayopen)
{
	struct netdata *nd;

	if ((nd = __netdata_init()) == NULL)
		return;
	setnetent_r(stayopen, &nd->data);
}

void
endnetent(void)
{
	struct netdata *nd;

	if ((nd = __netdata_init()) == NULL)
		return;
	endnetent_r(&nd->data);
}
