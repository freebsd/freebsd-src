/*	$NetBSD: mem.c,v 1.2 2002/01/21 19:49:51 tv Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: mem.c,v 1.2 2002/01/21 19:49:51 tv Exp $");
#endif

#include <stdlib.h>
#include <string.h>

#include "lint.h"

void *
xmalloc(size_t s)
{
	void	*p;

	if ((p = malloc(s)) == NULL)
		nomem();
	return (p);
}

void *
xcalloc(size_t n, size_t s)
{
	void	*p;

	if ((p = calloc(n, s)) == NULL)
		nomem();
	return (p);
}

void *
xrealloc(void *p, size_t s)
{

	if ((p = realloc(p, s)) == NULL)
		nomem();
	return (p);
}

char *
xstrdup(const char *s)
{
	char	*s2;

	if ((s2 = strdup(s)) == NULL)
		nomem();
	return (s2);
}

void
nomem(void)
{

	errx(1, "virtual memory exhausted");
}
