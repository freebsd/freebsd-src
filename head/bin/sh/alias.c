/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)alias.c	8.3 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include "shell.h"
#include "output.h"
#include "error.h"
#include "memalloc.h"
#include "mystring.h"
#include "alias.h"
#include "options.h"	/* XXX for argptr (should remove?) */

#define ATABSIZE 39

static struct alias *atab[ATABSIZE];
static int aliases;

static void setalias(const char *, const char *);
static int unalias(const char *);
static struct alias **hashalias(const char *);

static
void
setalias(const char *name, const char *val)
{
	struct alias *ap, **app;

	app = hashalias(name);
	for (ap = *app; ap; ap = ap->next) {
		if (equal(name, ap->name)) {
			INTOFF;
			ckfree(ap->val);
			ap->val	= savestr(val);
			INTON;
			return;
		}
	}
	/* not found */
	INTOFF;
	ap = ckmalloc(sizeof (struct alias));
	ap->name = savestr(name);
	/*
	 * XXX - HACK: in order that the parser will not finish reading the
	 * alias value off the input before processing the next alias, we
	 * dummy up an extra space at the end of the alias.  This is a crock
	 * and should be re-thought.  The idea (if you feel inclined to help)
	 * is to avoid alias recursions.  The mechanism used is: when
	 * expanding an alias, the value of the alias is pushed back on the
	 * input as a string and a pointer to the alias is stored with the
	 * string.  The alias is marked as being in use.  When the input
	 * routine finishes reading the string, it marks the alias not
	 * in use.  The problem is synchronization with the parser.  Since
	 * it reads ahead, the alias is marked not in use before the
	 * resulting token(s) is next checked for further alias sub.  The
	 * H A C K is that we add a little fluff after the alias value
	 * so that the string will not be exhausted.  This is a good
	 * idea ------- ***NOT***
	 */
#ifdef notyet
	ap->val = savestr(val);
#else /* hack */
	{
	size_t len = strlen(val);
	ap->val = ckmalloc(len + 2);
	memcpy(ap->val, val, len);
	ap->val[len] = ' ';	/* fluff */
	ap->val[len+1] = '\0';
	}
#endif
	ap->flag = 0;
	ap->next = *app;
	*app = ap;
	aliases++;
	INTON;
}

static int
unalias(const char *name)
{
	struct alias *ap, **app;

	app = hashalias(name);

	for (ap = *app; ap; app = &(ap->next), ap = ap->next) {
		if (equal(name, ap->name)) {
			/*
			 * if the alias is currently in use (i.e. its
			 * buffer is being used by the input routine) we
			 * just null out the name instead of freeing it.
			 * We could clear it out later, but this situation
			 * is so rare that it hardly seems worth it.
			 */
			if (ap->flag & ALIASINUSE)
				*ap->name = '\0';
			else {
				INTOFF;
				*app = ap->next;
				ckfree(ap->name);
				ckfree(ap->val);
				ckfree(ap);
				INTON;
			}
			aliases--;
			return (0);
		}
	}

	return (1);
}

static void
rmaliases(void)
{
	struct alias *ap, *tmp;
	int i;

	INTOFF;
	for (i = 0; i < ATABSIZE; i++) {
		ap = atab[i];
		atab[i] = NULL;
		while (ap) {
			ckfree(ap->name);
			ckfree(ap->val);
			tmp = ap;
			ap = ap->next;
			ckfree(tmp);
		}
	}
	aliases = 0;
	INTON;
}

struct alias *
lookupalias(const char *name, int check)
{
	struct alias *ap = *hashalias(name);

	for (; ap; ap = ap->next) {
		if (equal(name, ap->name)) {
			if (check && (ap->flag & ALIASINUSE))
				return (NULL);
			return (ap);
		}
	}

	return (NULL);
}

static int
comparealiases(const void *p1, const void *p2)
{
	const struct alias *const *a1 = p1;
	const struct alias *const *a2 = p2;

	return strcmp((*a1)->name, (*a2)->name);
}

static void
printalias(const struct alias *a)
{
	char *p;

	out1fmt("%s=", a->name);
	/* Don't print the space added above. */
	p = a->val + strlen(a->val) - 1;
	*p = '\0';
	out1qstr(a->val);
	*p = ' ';
	out1c('\n');
}

static void
printaliases(void)
{
	int i, j;
	struct alias **sorted, *ap;

	sorted = ckmalloc(aliases * sizeof(*sorted));
	j = 0;
	for (i = 0; i < ATABSIZE; i++)
		for (ap = atab[i]; ap; ap = ap->next)
			if (*ap->name != '\0')
				sorted[j++] = ap;
	qsort(sorted, aliases, sizeof(*sorted), comparealiases);
	for (i = 0; i < aliases; i++)
		printalias(sorted[i]);
	ckfree(sorted);
}

int
aliascmd(int argc, char **argv)
{
	char *n, *v;
	int ret = 0;
	struct alias *ap;

	if (argc == 1) {
		printaliases();
		return (0);
	}
	while ((n = *++argv) != NULL) {
		if ((v = strchr(n+1, '=')) == NULL) /* n+1: funny ksh stuff */
			if ((ap = lookupalias(n, 0)) == NULL) {
				warning("%s: not found", n);
				ret = 1;
			} else
				printalias(ap);
		else {
			*v++ = '\0';
			setalias(n, v);
		}
	}

	return (ret);
}

int
unaliascmd(int argc __unused, char **argv __unused)
{
	int i;

	while ((i = nextopt("a")) != '\0') {
		if (i == 'a') {
			rmaliases();
			return (0);
		}
	}
	for (i = 0; *argptr; argptr++)
		i |= unalias(*argptr);

	return (i);
}

static struct alias **
hashalias(const char *p)
{
	unsigned int hashval;

	hashval = *p << 4;
	while (*p)
		hashval+= *p++;
	return &atab[hashval % ATABSIZE];
}
