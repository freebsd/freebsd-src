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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
static int serv_stepping_yp = 0;
extern int _yp_check __P(( char ** ));
#endif

#define	MAXALIASES	35

static FILE *servf = NULL;
static char line[BUFSIZ+1];
static struct servent serv;
static char *serv_aliases[MAXALIASES];
int _serv_stayopen;

#ifdef YP
char *___getservbyname_yp = NULL;
char *___getservbyproto_yp = NULL;
int ___getservbyport_yp = 0;
static char *yp_domain = NULL;

static int
_getservbyport_yp(line)
	char *line;
{
	char *result;
	int resultlen;
	char buf[YPMAXRECORD + 2];
	int rv;

	snprintf(buf, sizeof(buf), "%d/%s", ntohs(___getservbyport_yp),
						___getservbyproto_yp);

	___getservbyport_yp = 0;
	___getservbyproto_yp = NULL;

	if(!yp_domain) {
		if(yp_get_default_domain(&yp_domain))
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
	if ((rv = yp_match(yp_domain, "services.byport", buf, strlen(buf),
						&result, &resultlen))) {
		if (rv == YPERR_MAP) {
			if (yp_match(yp_domain, "services.byname", buf,
					strlen(buf), &result, &resultlen))
			return(0);
		} else
			return(0);
	}
		
	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(line, BUFSIZ, "%.*s\n", resultlen, result);

	free(result);
	return(1);
}

static int
_getservbyname_yp(line)
	char *line;
{
	char *result;
	int resultlen;
	char buf[YPMAXRECORD + 2];

	if(!yp_domain) {
		if(yp_get_default_domain(&yp_domain))
			return (0);
	}

	snprintf(buf, sizeof(buf), "%s/%s", ___getservbyname_yp,
						___getservbyproto_yp);

	___getservbyname_yp = 0;
	___getservbyproto_yp = NULL;

	if (yp_match(yp_domain, "services.byname", buf, strlen(buf),
						&result, &resultlen)) {
		return(0);
	}
		
	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(line, BUFSIZ, "%.*s\n", resultlen, result);

	free(result);
	return(1);
}

static int
_getservent_yp(line)
	char *line;
{
	static char *key = NULL;
	static int keylen;
	char *lastkey, *result;
	int resultlen;
	int rv;

	if(!yp_domain) {
		if(yp_get_default_domain(&yp_domain))
			return (0);
	}

	if (!serv_stepping_yp) {
		if (key)
			free(key);
		if ((rv = yp_first(yp_domain, "services.byname", &key, &keylen,
			     &result, &resultlen))) {
			serv_stepping_yp = 0;
			return(0);
		}
		serv_stepping_yp = 1;
	} else {
		lastkey = key;
		rv = yp_next(yp_domain, "services.byname", key, keylen, &key,
			     &keylen, &result, &resultlen);
		free(lastkey);
		if (rv) {
			serv_stepping_yp = 0;
			return (0);
		}
	}

	/* getservent() expects lines terminated with \n -- make it happy */
	snprintf(line, BUFSIZ, "%.*s\n", resultlen, result);

	free(result);

	return(1);
}
#endif

void
setservent(f)
	int f;
{
	if (servf == NULL)
		servf = fopen(_PATH_SERVICES, "r" );
	else
		rewind(servf);
	_serv_stayopen |= f;
}

void
endservent()
{
	if (servf) {
		fclose(servf);
		servf = NULL;
	}
	_serv_stayopen = 0;
}

struct servent *
getservent()
{
	char *p;
	register char *cp, **q;

#ifdef YP
	if (serv_stepping_yp && _getservent_yp(line)) {
		p = (char *)&line;
		goto unpack;
	}
tryagain:
#endif
	if (servf == NULL && (servf = fopen(_PATH_SERVICES, "r" )) == NULL)
		return (NULL);
again:
	if ((p = fgets(line, BUFSIZ, servf)) == NULL)
		return (NULL);
#ifdef YP
	if (*p == '+' && _yp_check(NULL)) {
		if (___getservbyname_yp != NULL) {
			if (!_getservbyname_yp(line))
				goto tryagain;
		} 
		else if (___getservbyport_yp != 0) {
			if (!_getservbyport_yp(line))
				goto tryagain;
		}
		else if (!_getservent_yp(line))
			goto tryagain;
	}
unpack:
#endif
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
	serv.s_name = p;
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
	serv.s_port = htons((u_short)atoi(p));
	serv.s_proto = cp;
	q = serv.s_aliases = serv_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &serv_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&serv);
}
