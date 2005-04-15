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
static char sccsid[] = "@(#)getservbyname.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netdb.h>
#include <string.h>
#include "netdb_private.h"

int
getservbyname_r(const char *name, const char *proto, struct servent *se,
    struct servent_data *sed)
{
	char **cp;
	int error;

#ifdef YP
	sed->getservbyname_yp = (char *)name;
	sed->getservbyproto_yp = (char *)proto;
#endif

	setservent_r(sed->stayopen, sed);
	while ((error = getservent_r(se, sed)) == 0) {
		if (strcmp(name, se->s_name) == 0)
			goto gotname;
		for (cp = se->s_aliases; *cp; cp++)
			if (strcmp(name, *cp) == 0)
				goto gotname;
		continue;
gotname:
		if (proto == 0 || strcmp(se->s_proto, proto) == 0)
			break;
	}
	if (!sed->stayopen)
		endservent_r(sed);

#ifdef YP
	sed->getservbyname_yp = NULL;
	sed->getservbyproto_yp = NULL;
#endif

	return (error);
}

struct servent *
getservbyname(const char *name, const char *proto)
{
	struct servdata *sd;

	if ((sd = _servdata_init()) == NULL)
		return (NULL);
	if (getservbyname_r(name, proto, &sd->serv, &sd->data) != 0)
		return (NULL);
	return (&sd->serv);
}
