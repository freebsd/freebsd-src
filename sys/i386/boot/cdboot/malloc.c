/*
 * Copyright © 1997 Pluto Technologies International, Inc.  Boulder CO
 * Copyright © 1997 interface business GmbH, Dresden.
 *	All rights reserved.
 *
 * This code was written by Jörg Wunsch, Dresden.
 * Direct comments to <joerg_wunsch@interface-business.de>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/boot/cdboot/malloc.c,v 1.2 1999/08/28 00:43:18 peter Exp $
 */

/*
 * Simple memory allocator for the bootstrap loader.  Probably suffers
 * a lot from fragmentation.
 */

#include "boot.h"

#include <stddef.h>

/* ``Nobody will ever need more than 640 KB of RAM.'' :-) */
#define MAXBRK (640 * 1024 * 1024)

/* allocation unit */
#define NCHUNKS 2048

struct chunk
{
	struct chunk *next;
	size_t len;
};

static void *brkval;
static struct chunk *freelist;

void *
malloc(size_t len)
{
	struct chunk *p, *q, *oldp;
	size_t nelems;

	nelems = (len + sizeof(struct chunk) - 1) / sizeof(struct chunk) + 1;

	/*
	 * First, see if we can satisfy the request from the freelist.
	 */
	for (p = freelist, oldp = 0;
	     p && p != (struct chunk *)brkval;
	     oldp = p, p = p->next) {
		if (p->len > nelems) {
			/* chunk is larger, shorten, and return the tail */
			p->len -= nelems;
			q = p + p->len;
			q->next = 0;
			q->len = nelems;
			q++;
			return (void *)q;
		}
		if (p->len == nelems) {
			/* exact match, remove from freelist */
			if (oldp == 0)
				freelist = p->next;
			else
				oldp->next = p->next;
			p->next = 0;
			p++;
			return (void *)p;
		}
	}
	/*
	 * Nothing found on freelist, try obtaining more space.
	 */
	if (brkval == 0)
		brkval = &end;
	q = p = (struct chunk *)brkval;
	if ((int)(p + NCHUNKS) > MAXBRK)
		return 0;

	p += NCHUNKS;
	brkval = p;

	q->next = p;
	q->len = NCHUNKS;

	if (oldp == 0)
		freelist = q;
	else {
		if (oldp + oldp->len == q) {
			/* extend last chunk */
			oldp->len += NCHUNKS;
			oldp->next = p;
		} else
			oldp->next = q;
	}

	return malloc(len);
}

void
free(void *ptr)
{
	struct chunk *p, *q, *oldp;

	if (ptr == 0)
		return;

	q = (struct chunk *)ptr;
	q--;
	if (q->next != 0) {
		printf("malloc error: botched ptr to free()\n");
		return;
	}

	/*
	 * Walk the freelist, and insert in the correct sequence.
	 */
	for (p = freelist, oldp = 0;
	     p && p != (struct chunk *)brkval;
	     oldp = p, p = p->next) {
		if ((unsigned)p > (unsigned)q) {
			if (q + q->len == p) {
				/* aggregate with next chunk */
				q->len += p->len;
				q->next = p->next;
				p = p->next;
			}
			if (oldp) {
				if (oldp + oldp->len == q) {
				/* aggregate with previous chunk */
					oldp->len += q->len;
					oldp->next = p;
				} else {
				/* insert into chain */
					q->next = p;
					oldp->next = q;
				}
				return;
			}
			q->next = p;
			freelist = q;
		}
	}
	if (oldp) {
		/* we are topmost */
		if (oldp + oldp->len == q) {
			/* aggregate with previous chunk */
			oldp->len += q->len;
			oldp->next = p;
		} else {
			oldp->next = q;
			q->next = p;
		}
		return;
	}
	/* we are alone on the freelist */
	freelist = q;
}

