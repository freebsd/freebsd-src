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
static char sccsid[] = "@(#)getprotoent.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "namespace.h"
#include "reentrant.h"
#include "un-namespace.h"
#include "netdb_private.h"

NETDB_THREAD_ALLOC(protoent_data)
NETDB_THREAD_ALLOC(protodata)

static void
protoent_data_clear(struct protoent_data *ped)
{
	if (ped->fp) {
		fclose(ped->fp);
		ped->fp = NULL;
	}
}

static void
protoent_data_free(void *ptr)
{
	struct protoent_data *ped = ptr;

	protoent_data_clear(ped);
	free(ped);
}

static void
protodata_free(void *ptr)
{
	free(ptr);
}

int
__copy_protoent(struct protoent *pe, struct protoent *pptr, char *buf,
    size_t buflen)
{
	char *cp;
	int i, n;
	int numptr, len;

	/* Find out the amount of space required to store the answer. */
	numptr = 1; /* NULL ptr */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; pe->p_aliases[i]; i++, numptr++) {
		len += strlen(pe->p_aliases[i]) + 1;
	}
	len += strlen(pe->p_name) + 1;
	len += numptr * sizeof(char*);

	if (len > (int)buflen) {
		errno = ERANGE;
		return (-1);
	}

	/* copy protocol value*/
	pptr->p_proto = pe->p_proto;

	cp = (char *)ALIGN(buf) + numptr * sizeof(char *);

	/* copy official name */
	n = strlen(pe->p_name) + 1;
	strcpy(cp, pe->p_name);
	pptr->p_name = cp;
	cp += n;

	/* copy aliases */
	pptr->p_aliases = (char **)ALIGN(buf);
	for (i = 0 ; pe->p_aliases[i]; i++) {
		n = strlen(pe->p_aliases[i]) + 1;
		strcpy(cp, pe->p_aliases[i]);
		pptr->p_aliases[i] = cp;
		cp += n;
	}
	pptr->p_aliases[i] = NULL;

	return (0);
}

void
__setprotoent_p(int f, struct protoent_data *ped)
{
	if (ped->fp == NULL)
		ped->fp = fopen(_PATH_PROTOCOLS, "r");
	else
		rewind(ped->fp);
	ped->stayopen |= f;
}

void
__endprotoent_p(struct protoent_data *ped)
{
	if (ped->fp) {
		fclose(ped->fp);
		ped->fp = NULL;
	}
	ped->stayopen = 0;
}

int
__getprotoent_p(struct protoent *pe, struct protoent_data *ped)
{
	char *p;
	char *cp, **q, *endp;
	long l;

	if (ped->fp == NULL && (ped->fp = fopen(_PATH_PROTOCOLS, "r")) == NULL)
		return (-1);
again:
	if ((p = fgets(ped->line, sizeof ped->line, ped->fp)) == NULL)
		return (-1);
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp != NULL)
		*cp = '\0';
	pe->p_name = p;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = strpbrk(cp, " \t");
	if (p != NULL)
		*p++ = '\0';
	l = strtol(cp, &endp, 10);
	if (endp == cp || *endp != '\0' || l < 0 || l > USHRT_MAX)
		goto again;
	pe->p_proto = l;
	q = pe->p_aliases = ped->aliases;
	if (p != NULL) {
		cp = p;
		while (cp && *cp) {
			if (*cp == ' ' || *cp == '\t') {
				cp++;
				continue;
			}
			if (q < &ped->aliases[_MAXALIASES - 1])
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (0);
}

int
getprotoent_r(struct protoent *pptr, char *buffer, size_t buflen,
    struct protoent **result)
{
	struct protoent pe;
	struct protoent_data *ped;

	if ((ped = __protoent_data_init()) == NULL)
		return (-1);

	if (__getprotoent_p(&pe, ped) != 0)
		return (-1);
	if (__copy_protoent(&pe, pptr, buffer, buflen) != 0)
		return (-1);
	*result = pptr;
	return (0);
}

void
setprotoent(int f)
{
	struct protoent_data *ped;

	if ((ped = __protoent_data_init()) == NULL)
		return;
	__setprotoent_p(f, ped);
}

void
endprotoent(void)
{
	struct protoent_data *ped;

	if ((ped = __protoent_data_init()) == NULL)
		return;
	__endprotoent_p(ped);
}

struct protoent *
getprotoent(void)
{
	struct protodata *pd;
	struct protoent *rval;

	if ((pd = __protodata_init()) == NULL)
		return (NULL);
	if (getprotoent_r(&pd->proto, pd->data, sizeof(pd->data), &rval) != 0)
		return (NULL);
	return (rval);
}
