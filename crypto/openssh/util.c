/*	$OpenBSD: util.c,v 1.6 2000/10/27 07:32:19 markus Exp $	*/

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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

#include "includes.h"
RCSID("$OpenBSD: util.c,v 1.6 2000/10/27 07:32:19 markus Exp $");

#include "ssh.h"

char *
chop(char *s)
{
	char *t = s;
	while (*t) {
		if(*t == '\n' || *t == '\r') {
			*t = '\0';
			return s;
		}
		t++;
	}
	return s;

}

void
set_nonblock(int fd)
{
	int val;
	val = fcntl(fd, F_GETFL, 0);
	if (val < 0) {
		error("fcntl(%d, F_GETFL, 0): %s", fd, strerror(errno));
		return;
	}
	if (val & O_NONBLOCK) {
		debug("fd %d IS O_NONBLOCK", fd);
		return;
	}
	debug("fd %d setting O_NONBLOCK", fd);
	val |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, val) == -1)
		if (errno != ENODEV)
			error("fcntl(%d, F_SETFL, O_NONBLOCK): %s",
			    fd, strerror(errno));
}

/* Characters considered whitespace in strsep calls. */
#define WHITESPACE " \t\r\n"

char *
strdelim(char **s)
{
	char *old;
	int wspace = 0;

	if (*s == NULL)
		return NULL;

	old = *s;

	*s = strpbrk(*s, WHITESPACE "=");
	if (*s == NULL)
		return (old);

	/* Allow only one '=' to be skipped */
	if (*s[0] == '=')
		wspace = 1;
	*s[0] = '\0';

	*s += strspn(*s + 1, WHITESPACE) + 1;
	if (*s[0] == '=' && !wspace)
		*s += strspn(*s + 1, WHITESPACE) + 1;

	return (old);
}
