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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)$Id: getnetnamadr.c,v 1.5 1996/07/12 18:54:40 jkh Exp $";
static char rcsid[] = "$Id: getnetnamadr.c,v 1.5 1996/07/12 18:54:40 jkh Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#ifndef _PATH_NETCONF
#define _PATH_NETCONF	"/etc/host.conf"
#endif

enum service_type {
  SERVICE_NONE = 0,
  SERVICE_BIND,
  SERVICE_TABLE,
  SERVICE_NIS };
#define SERVICE_MAX	SERVICE_NIS

static struct {
  const char *name;
  enum service_type type;
} service_names[] = {
  { "hosts", SERVICE_TABLE },
  { "/etc/hosts", SERVICE_TABLE },
  { "hosttable", SERVICE_TABLE },
  { "htable", SERVICE_TABLE },
  { "bind", SERVICE_BIND },
  { "dns", SERVICE_BIND },
  { "domain", SERVICE_BIND },
  { "yp", SERVICE_NIS },
  { "yellowpages", SERVICE_NIS },
  { "nis", SERVICE_NIS },
  { 0, SERVICE_NONE }
};

static enum service_type service_order[SERVICE_MAX + 1];
static int service_done = 0;

static enum service_type
get_service_name(const char *name) {
	int i;
	for(i = 0; service_names[i].type != SERVICE_NONE; i++) {
		if(!strcasecmp(name, service_names[i].name)) {
			return service_names[i].type;
		}
	}
	return SERVICE_NONE;
}

static void
init_services()
{
	char *cp, *p, buf[BUFSIZ];
	register int cc = 0;
	FILE *fd;

	if ((fd = (FILE *)fopen(_PATH_NETCONF, "r")) == NULL) {
				/* make some assumptions */
		service_order[0] = SERVICE_TABLE;
		service_order[1] = SERVICE_NONE;
	} else {
		while (fgets(buf, BUFSIZ, fd) != NULL && cc < SERVICE_MAX) {
			if(buf[0] == '#')
				continue;

			p = buf;
			while ((cp = strsep(&p, "\n \t,:;")) != NULL && *cp == '\0')
				;
			if (cp == NULL)
				continue;
			do {
				if (isalpha(cp[0])) {
					service_order[cc] = get_service_name(cp);
					if(service_order[cc] != SERVICE_NONE)
						cc++;
				}
				while ((cp = strsep(&p, "\n \t,:;")) != NULL && *cp == '\0')
					;
			} while(cp != NULL && cc < SERVICE_MAX);
		}
		service_order[cc] = SERVICE_NONE;
		fclose(fd);
	}
	service_done = 1;
}

struct netent *
getnetbyname(const char *name)
{
	struct netent *hp = 0;
	int nserv = 0;

	if (!service_done)
		init_services();

	while (!hp) {
		switch (service_order[nserv]) {
		      case SERVICE_NONE:
			return NULL;
		      case SERVICE_TABLE:
			hp = _getnetbyhtname(name);
			break;
		      case SERVICE_BIND:
			hp = _getnetbydnsname(name);
			break;
		      case SERVICE_NIS:
			hp = _getnetbynisname(name);
			break;
		}
		nserv++;
	}
	return hp;
}

struct netent *
getnetbyaddr(addr, af)
	long addr;
	int af;
{
	struct netent *hp = 0;
	int nserv = 0;

	if (!service_done)
		init_services();

	while (!hp) {
		switch (service_order[nserv]) {
		      case SERVICE_NONE:
			return 0;
		      case SERVICE_TABLE:
			hp = _getnetbyhtaddr(addr, af);
			break;
		      case SERVICE_BIND:
			hp = _getnetbydnsaddr(addr, af);
			break;
		      case SERVICE_NIS:
			hp = _getnetbynisaddr(addr, af);
			break;
		}
		nserv++;
	}
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
