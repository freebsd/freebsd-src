/*	$NetBSD: fsutil.c,v 1.15 2006/06/05 16:52:05 christos Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: fsutil.c,v 1.15 2006/06/05 16:52:05 christos Exp $");
#endif /* not lint */
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <fstab.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsutil.h"

static const char *dev = NULL;
static int preen = 0;

static void vmsg(int, const char *, va_list) __printflike(2, 0);

/*
 * The getfsopt() function checks whether an option is present in
 * an fstab(5) fs_mntops entry. There are six possible cases:
 *
 * fs_mntops  getfsopt  result
 *  rw,foo       foo    true
 *  rw,nofoo    nofoo   true
 *  rw,nofoo     foo    false
 *  rw,foo      nofoo   false
 *  rw           foo    false
 *  rw          nofoo   false
 *
 * This function should be part of and documented in getfsent(3).
 */
int
getfsopt(struct fstab *fs, const char *option)
{
	int negative, found;
	char *opt, *optbuf;

	if (option[0] == 'n' && option[1] == 'o') {
		negative = 1;
		option += 2;
	} else
		negative = 0;
	optbuf = strdup(fs->fs_mntops);
	found = 0;
	for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		if (opt[0] == 'n' && opt[1] == 'o') {
			if (!strcasecmp(opt + 2, option))
				found = negative;
		} else if (!strcasecmp(opt, option))
			found = !negative;
	}
	free(optbuf);
	return (found);
}

void
setcdevname(const char *cd, int pr)
{
	dev = cd;
	preen = pr;
}

const char *
cdevname(void)
{
	return dev;
}

static void
vmsg(int fatal, const char *fmt, va_list ap)
{
	if (!fatal && preen)
		(void) printf("%s: ", dev);

	(void) vprintf(fmt, ap);

	if (fatal && preen)
		(void) printf("\n");

	if (fatal && preen) {
		(void) printf(
		    "%s: UNEXPECTED INCONSISTENCY; RUN %s MANUALLY.\n",
		    dev, getprogname());
		exit(8);
	}
}

/*VARARGS*/
void
pfatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1, fmt, ap);
	va_end(ap);
}

/*VARARGS*/
void
pwarn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(0, fmt, ap);
	va_end(ap);
}

void
perr(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1, fmt, ap);
	va_end(ap);
}

void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vmsg(1, fmt, ap);
	va_end(ap);
	exit(8);
}

const char *
devcheck(const char *origname)
{
	struct stat stslash, stchar;

	if (stat("/", &stslash) < 0) {
		perr("Can't stat `/'");
		return (origname);
	}
	if (stat(origname, &stchar) < 0) {
		perr("Can't stat %s\n", origname);
		return (origname);
	}
	if (!S_ISCHR(stchar.st_mode)) {
		perr("%s is not a char device\n", origname);
	}
	return (origname);
}

void *
emalloc(size_t s)
{
	void *p;

	p = malloc(s);
	if (p == NULL)
		err(1, "malloc failed");
	return (p);
}


void *
erealloc(void *p, size_t s)
{
	void *q;

	q = realloc(p, s);
	if (q == NULL)
		err(1, "realloc failed");
	return (q);
}


char *
estrdup(const char *s)
{
	char *p;

	p = strdup(s);
	if (p == NULL)
		err(1, "strdup failed");
	return (p);
}
