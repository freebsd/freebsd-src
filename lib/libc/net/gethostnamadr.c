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

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <nsswitch.h>
#include <arpa/nameser.h>		/* XXX hack for _res */
#include <resolv.h>			/* XXX hack for _res */

extern int _ht_gethostbyname(void *, void *, va_list);
extern int _dns_gethostbyname(void *, void *, va_list);
extern int _nis_gethostbyname(void *, void *, va_list);
extern int _ht_gethostbyaddr(void *, void *, va_list);
extern int _dns_gethostbyaddr(void *, void *, va_list);
extern int _nis_gethostbyaddr(void *, void *, va_list);

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
		return (NULL);
	}
	if (_res.options & RES_USE_INET6) {		/* XXX */
		hp = gethostbyname2(name, AF_INET6);	/* XXX */
		if (hp)					/* XXX */
			return (hp);			/* XXX */
	}						/* XXX */
	return (gethostbyname2(name, AF_INET));
}

struct hostent *
gethostbyname2(const char *name, int type)
{
	struct hostent *hp = 0;
	int rval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_gethostbyname, NULL)
		{ NSSRC_DNS, _dns_gethostbyname, NULL },
		NS_NIS_CB(_nis_gethostbyname, NULL) /* force -DHESIOD */
		{ 0 }
	};       
	
	rval = nsdispatch((void *)&hp, dtab, NSDB_HOSTS, "gethostbyname",
			  default_src, name, type);

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

	rval = nsdispatch((void *)&hp, dtab, NSDB_HOSTS, "gethostbyaddr",
			  default_src, addr, len, type);

	if (rval != NS_SUCCESS)
		return NULL;
	else
		return hp;
}

struct hostent_data;

/*
 * Temporary function (not thread safe)
 */
int gethostbyaddr_r(const char *addr, int len, int type,
	struct hostent *result, struct hostent_data *buffer)
{
	struct hostent *hp;
	int ret;
	if ((hp = gethostbyaddr(addr, len, type)) == NULL) {
		ret = -1;
	} else {
		memcpy(result, hp, sizeof(struct hostent));
		ret = 0;
	}
	return(ret);
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
