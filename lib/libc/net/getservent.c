/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
static char sccsid[] = "@(#)getservent.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include "namespace.h"
#include "reentrant.h"
#include "un-namespace.h"
#include "netdb_private.h"

static struct servdata servdata;
static thread_key_t servdata_key;
static once_t servdata_init_once = ONCE_INITIALIZER;
static int servdata_thr_keycreated = 0;

static void
servent_data_clear(struct servent_data *sed)
{
	if (sed->fp) {
		fclose(sed->fp);
		sed->fp = NULL;
	}
#ifdef YP
	free(sed->yp_key);
	sed->yp_key = NULL;
#endif
}

static void
servdata_free(void *ptr)
{
	struct servdata *sd = ptr;

	if (sd == NULL)
		return;
	servent_data_clear(&sd->data);
	free(sd);
}

static void
servdata_keycreate(void)
{
	servdata_thr_keycreated =
	    (thr_keycreate(&servdata_key, servdata_free) == 0);
}

struct servdata *
__servdata_init(void)
{
	struct servdata *sd;

	if (thr_main() != 0)
		return (&servdata);
	if (thr_once(&servdata_init_once, servdata_keycreate) != 0 ||
	    !servdata_thr_keycreated)
		return (NULL);
	if ((sd = thr_getspecific(servdata_key)) != NULL)
		return (sd);
	if ((sd = calloc(1, sizeof(*sd))) == NULL)
		return (NULL);
	if (thr_setspecific(servdata_key, sd) == 0)
		return (sd);
	free(sd);
	return (NULL);
}

#ifdef YP
static int
_getservbyport_yp(struct servent_data *sed)
{
	char *result;
	int resultlen;
	char buf[YPMAXRECORD + 2];
	int rv;

	snprintf(buf, sizeof(buf), "%d/%s", ntohs(sed->yp_port),
	    sed->yp_proto);

	sed->yp_port = 0;
	sed->yp_proto = NULL;

	if (!sed->yp_domain) {
		if (yp_get_default_domain(&sed->yp_domain))
			return (0);
	}

	/*
	 * We have to be a little flexible here. Ideally you're supposed
	 * to have both a services.byname and a services.byport map, but
	 * some systems have only services.byname. FreeBSD cheats a little
	 * by putting the services.byport information in the same map as
	 * services.byname so that either case will work. We allow for both
	 * possibilities here: if there is no services.byport map, we try
	 * services.byname instead.
	 */
	if ((rv = yp_match(sed->yp_domain, "services.byport", buf, strlen(buf),
						&result, &resultlen))) {
		if (rv == YPERR_MAP) {
			if (yp_match(sed->yp_domain, "services.byname", buf,
					strlen(buf), &result, &resultlen))
			return(0);
		} else
			return(0);
	}
		
	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(sed->line, sizeof sed->line, "%.*s\n", resultlen, result);

	free(result);
	return(1);
}

static int
_getservbyname_yp(struct servent_data *sed)
{
	char *result;
	int resultlen;
	char buf[YPMAXRECORD + 2];

	if(!sed->yp_domain) {
		if(yp_get_default_domain(&sed->yp_domain))
			return (0);
	}

	snprintf(buf, sizeof(buf), "%s/%s", sed->yp_name, sed->yp_proto);

	sed->yp_name = 0;
	sed->yp_proto = NULL;

	if (yp_match(sed->yp_domain, "services.byname", buf, strlen(buf),
	    &result, &resultlen)) {
		return(0);
	}
		
	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(sed->line, sizeof sed->line, "%.*s\n", resultlen, result);

	free(result);
	return(1);
}

static int
_getservent_yp(struct servent_data *sed)
{
	char *lastkey, *result;
	int resultlen;
	int rv;

	if (!sed->yp_domain) {
		if (yp_get_default_domain(&sed->yp_domain))
			return (0);
	}

	if (!sed->yp_stepping) {
		free(sed->yp_key);
		rv = yp_first(sed->yp_domain, "services.byname", &sed->yp_key,
		    &sed->yp_keylen, &result, &resultlen);
		if (rv) {
			sed->yp_stepping = 0;
			return(0);
		}
		sed->yp_stepping = 1;
	} else {
		lastkey = sed->yp_key;
		rv = yp_next(sed->yp_domain, "services.byname", sed->yp_key,
		    sed->yp_keylen, &sed->yp_key, &sed->yp_keylen, &result,
		    &resultlen);
		free(lastkey);
		if (rv) {
			sed->yp_stepping = 0;
			return (0);
		}
	}

	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(sed->line, sizeof sed->line, "%.*s\n", resultlen, result);

	free(result);

	return(1);
}
#endif

void
setservent_r(int f, struct servent_data *sed)
{
	if (sed->fp == NULL)
		sed->fp = fopen(_PATH_SERVICES, "r");
	else
		rewind(sed->fp);
	sed->stayopen |= f;
}

void
endservent_r(struct servent_data *sed)
{
	servent_data_clear(sed);
	sed->stayopen = 0;
#ifdef YP
	sed->yp_stepping = 0;
	sed->yp_domain = NULL;
#endif
}

int
getservent_r(struct servent *se, struct servent_data *sed)
{
	char *p;
	char *cp, **q, *endp;
	long l;

#ifdef YP
	if (sed->yp_stepping && _getservent_yp(sed)) {
		p = sed->line;
		goto unpack;
	}
tryagain:
#endif
	if (sed->fp == NULL && (sed->fp = fopen(_PATH_SERVICES, "r")) == NULL)
		return (-1);
again:
	if ((p = fgets(sed->line, sizeof sed->line, sed->fp)) == NULL)
		return (-1);
#ifdef YP
	if (*p == '+' && _yp_check(NULL)) {
		if (sed->yp_name != NULL) {
			if (!_getservbyname_yp(sed))
				goto tryagain;
		} 
		else if (sed->yp_port != 0) {
			if (!_getservbyport_yp(sed))
				goto tryagain;
		}
		else if (!_getservent_yp(sed))
			goto tryagain;
	}
unpack:
#endif
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp != NULL)
		*cp = '\0';
	se->s_name = p;
	p = strpbrk(p, " \t");
	if (p == NULL)
		goto again;
	*p++ = '\0';
	while (*p == ' ' || *p == '\t')
		p++;
	cp = strpbrk(p, ",/");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	l = strtol(p, &endp, 10);
	if (endp == p || *endp != '\0' || l < 0 || l > USHRT_MAX)
		goto again;
	se->s_port = htons((in_port_t)l);
	se->s_proto = cp;
	q = se->s_aliases = sed->aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &sed->aliases[_MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (0);
}

void
setservent(int f)
{
	struct servdata *sd;

	if ((sd = __servdata_init()) == NULL)
		return;
	setservent_r(f, &sd->data);
}

void
endservent(void)
{
	struct servdata *sd;

	if ((sd = __servdata_init()) == NULL)
		return;
	endservent_r(&sd->data);
}

struct servent *
getservent(void)
{
	struct servdata *sd;

	if ((sd = __servdata_init()) == NULL)
		return (NULL);
	if (getservent_r(&sd->serv, &sd->data) != 0)
		return (NULL);
	return (&sd->serv);
}
