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
static char sccsid[] = "@(#)$FreeBSD$";
static char rcsid[] = "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <arpa/nameser.h>		/* XXX hack for _res */
#include <resolv.h>			/* XXX hack for _res */

#define _PATH_HOSTCONF	"/etc/host.conf"

enum service_type {
  SERVICE_NONE = 0,
  SERVICE_BIND,
  SERVICE_HOSTS,
  SERVICE_NIS };
#define SERVICE_MAX	SERVICE_NIS

static struct {
  const char *name;
  enum service_type type;
} service_names[] = {
  { "hosts", SERVICE_HOSTS },
  { "/etc/hosts", SERVICE_HOSTS },
  { "hosttable", SERVICE_HOSTS },
  { "htable", SERVICE_HOSTS },
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

	if ((fd = (FILE *)fopen(_PATH_HOSTCONF, "r")) == NULL) {
				/* make some assumptions */
		service_order[0] = SERVICE_BIND;
		service_order[1] = SERVICE_HOSTS;
		service_order[2] = SERVICE_NONE;
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
				if (isalpha((unsigned char)cp[0])) {
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
	int nserv = 0;

	if (!service_done)
		init_services();

	while (!hp) {
		switch (service_order[nserv]) {
		      case SERVICE_NONE:
			return NULL;
		      case SERVICE_HOSTS:
			hp = _gethostbyhtname(name, type);
			break;
		      case SERVICE_BIND:
			hp = _gethostbydnsname(name, type);
			break;
		      case SERVICE_NIS:
			hp = _gethostbynisname(name, type);
			break;
		}
		nserv++;
	}
	return hp;
}

struct hostent *
gethostbyaddr(const char *addr, int len, int type)
{
	struct hostent *hp = 0;
	int nserv = 0;

	if (!service_done)
		init_services();

	while (!hp) {
		switch (service_order[nserv]) {
		      case SERVICE_NONE:
			return 0;
		      case SERVICE_HOSTS:
			hp = _gethostbyhtaddr(addr, len, type);
			break;
		      case SERVICE_BIND:
			hp = _gethostbydnsaddr(addr, len, type);
			break;
		      case SERVICE_NIS:
			hp = _gethostbynisaddr(addr, len, type);
			break;
		}
		nserv++;
	}
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
