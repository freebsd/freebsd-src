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

struct netent *
getnetbyname(const char *name)
{
	struct netent *hp = 0;
	int rval;


	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyname, NULL)
		{ NSSRC_DNS, _dns_getnetbyname, NULL },
		NS_NIS_CB(_nis_getnetbyname, NULL) /* force -DHESIOD */
		{ 0 }
	};       
	
	rval = nsdispatch((void *)&hp, dtab, NSDB_NETWORKS, "getnetbyname",
			  default_src, name);

	if (rval != NS_SUCCESS)
		return NULL;
	else
		return hp;
}

struct netent *
getnetbyaddr(u_long addr, int af)
{
	struct netent *hp = 0;
	int rval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_ht_getnetbyaddr, NULL)
		{ NSSRC_DNS, _dns_getnetbyaddr, NULL },
		NS_NIS_CB(_nis_getnetbyaddr, NULL) /* force -DHESIOD */
		{ 0 }
	};       

	rval = nsdispatch((void *)&hp, dtab, NSDB_NETWORKS, "getnetbyaddr",
			  default_src, addr, af);

	if (rval != NS_SUCCESS)
		return NULL;
	else
		return hp;
}

void
setnetent(stayopen)
	int stayopen;
{
	_setnethtent(stayopen);
	_setnetdnsent(stayopen);
}

void
endnetent()
{
	_endnethtent();
	_endnetdnsent();
}
