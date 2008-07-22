/* $OpenBSD: monitor_mm.h,v 1.4 2006/08/03 03:34:42 deraadt Exp $ */

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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

#ifndef _MM_H_
#define _MM_H_

struct mm_share {
	RB_ENTRY(mm_share) next;
	void *address;
	size_t size;
};

struct mm_master {
	RB_HEAD(mmtree, mm_share) rb_free;
	struct mmtree rb_allocated;
	void *address;
	size_t size;

	struct mm_master *mmalloc;	/* Used to completely share */

	int write;		/* used to writing to other party */
	int read;		/* used for reading from other party */
};

RB_PROTOTYPE(mmtree, mm_share, next, mm_compare)

#define MM_MINSIZE		128

#define MM_ADDRESS_END(x)	(void *)((u_char *)(x)->address + (x)->size)

struct mm_master *mm_create(struct mm_master *, size_t);
void mm_destroy(struct mm_master *);

void mm_share_sync(struct mm_master **, struct mm_master **);

void *mm_malloc(struct mm_master *, size_t);
void *mm_xmalloc(struct mm_master *, size_t);
void mm_free(struct mm_master *, void *);

void mm_memvalid(struct mm_master *, void *, size_t);
#endif /* _MM_H_ */
