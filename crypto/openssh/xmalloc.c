/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 * 
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: xmalloc.c,v 1.8 2000/09/07 20:27:55 deraadt Exp $");

#include "ssh.h"

void *
xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL)
		fatal("xmalloc: out of memory (allocating %d bytes)", (int) size);
	return ptr;
}

void *
xrealloc(void *ptr, size_t new_size)
{
	void *new_ptr;

	if (ptr == NULL)
		fatal("xrealloc: NULL pointer given as argument");
	new_ptr = realloc(ptr, new_size);
	if (new_ptr == NULL)
		fatal("xrealloc: out of memory (new_size %d bytes)", (int) new_size);
	return new_ptr;
}

void
xfree(void *ptr)
{
	if (ptr == NULL)
		fatal("xfree: NULL pointer given as argument");
	free(ptr);
}

char *
xstrdup(const char *str)
{
	int len = strlen(str) + 1;

	char *cp = xmalloc(len);
	strlcpy(cp, str, len);
	return cp;
}
