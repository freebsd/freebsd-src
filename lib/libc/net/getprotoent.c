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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "namespace.h"
#include "reentrant.h"
#include "un-namespace.h"
#include "netdb_private.h"

static struct protodata protodata;
static thread_key_t protodata_key;
static once_t protodata_init_once = ONCE_INITIALIZER;
static int protodata_thr_keycreated = 0;

static void
protoent_data_clear(struct protoent_data *ped)
{
	if (ped->fp) {
		fclose(ped->fp);
		ped->fp = NULL;
	}
}

static void
protodata_free(void *ptr)
{
	struct protodata *pd = ptr;

	if (pd == NULL)
		return;
	protoent_data_clear(&pd->data);
	free(pd);
}

static void
protodata_keycreate(void)
{
	protodata_thr_keycreated =
	    (thr_keycreate(&protodata_key, protodata_free) == 0);
}

struct protodata *
__protodata_init(void)
{
	struct protodata *pd;

	if (thr_main() != 0)
		return (&protodata);
	if (thr_once(&protodata_init_once, protodata_keycreate) != 0 ||
	    !protodata_thr_keycreated)
		return (NULL);
	if ((pd = thr_getspecific(protodata_key)) != NULL)
		return (pd);
	if ((pd = calloc(1, sizeof(*pd))) == NULL)
		return (NULL);
	if (thr_setspecific(protodata_key, pd) == 0)
		return (pd);
	free(pd);
	return (NULL);
}

void
setprotoent_r(int f, struct protoent_data *ped)
{
	if (ped->fp == NULL)
		ped->fp = fopen(_PATH_PROTOCOLS, "r");
	else
		rewind(ped->fp);
	ped->stayopen |= f;
}

void
endprotoent_r(struct protoent_data *ped)
{
	if (ped->fp) {
		fclose(ped->fp);
		ped->fp = NULL;
	}
	ped->stayopen = 0;
}

int
getprotoent_r(struct protoent *pe, struct protoent_data *ped)
{
	char *p;
	char *cp, **q, *endp;
	long l;

	if (ped->fp == NULL && (ped->fp = fopen(_PATH_PROTOCOLS, "r")) == NULL)
		return (-1);
again:
	if ((p = fgets(ped->line, BUFSIZ, ped->fp)) == NULL)
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
			if (q < &ped->aliases[PROTOENT_MAXALIASES - 1])
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (0);
}

void
setprotoent(int f)
{
	struct protodata *pd;

	if ((pd = __protodata_init()) == NULL)
		return;
	setprotoent_r(f, &pd->data);
}

void
endprotoent(void)
{
	struct protodata *pd;

	if ((pd = __protodata_init()) == NULL)
		return;
	endprotoent_r(&pd->data);
}

struct protoent *
getprotoent(void)
{
	struct protodata *pd;

	if ((pd = __protodata_init()) == NULL)
		return (NULL);
	if (getprotoent_r(&pd->proto, &pd->data) != 0)
		return (NULL);
	return (&pd->proto);
}
