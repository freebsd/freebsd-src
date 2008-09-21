/*-
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
 *
 *	@(#)clist.h	8.1 (Berkeley) 6/4/93
 * $FreeBSD$
 */

#ifndef _SYS_CLIST_H_
#define _SYS_CLIST_H_

#include <sys/param.h>

/*
 * Clists are character lists, which is a variable length linked list
 * of cblocks, with a count of the number of characters in the list.
 */
struct clist {
	int	c_cc;		/* Number of characters in the clist. */
	int	c_cbcount;	/* Number of cblocks. */
	int	c_cbmax;	/* Max # cblocks allowed for this clist. */
	int	c_cbreserved;	/* # cblocks reserved for this clist. */
	char	*c_cf;		/* Pointer to the first cblock. */
	char	*c_cl;		/* Pointer to the last cblock. */
};

struct cblock {
	struct cblock *c_next;			/* next cblock in queue */
	unsigned char c_info[CBSIZE];		/* characters */
};

#ifdef _KERNEL
extern	int cfreecount;

int	 b_to_q(char *cp, int cc, struct clist *q);
void	 clist_alloc_cblocks(struct clist *q, int ccmax, int ccres);
void	 clist_free_cblocks(struct clist *q);
int	 getc(struct clist *q);
void	 ndflush(struct clist *q, int cc);
int	 putc(char c, struct clist *q);
int	 q_to_b(struct clist *q, char *cp, int cc);
int	 unputc(struct clist *q);
#endif

#endif
