/*-
 * Copyright (c) 2003-2004 Alan L. Cox <alc@cs.rice.edu>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SYS_SF_BUF_H_
#define _SYS_SF_BUF_H_

/*
 * Options to sf_buf_alloc() are specified through its flags argument.  This
 * argument's value should be the result of a bitwise or'ing of one or more
 * of the following values.
 */
#define	SFB_CATCH	1		/* Check signals if the allocation
					   sleeps. */
#define	SFB_CPUPRIVATE	2		/* Create a CPU private mapping. */
#define	SFB_DEFAULT	0
#define	SFB_NOWAIT	4		/* Return NULL if all bufs are used. */

struct vm_page;

struct sfstat {				/* sendfile statistics */
	uint64_t	sf_iocnt;	/* times sendfile had to do disk I/O */
	uint64_t	sf_allocfail;	/* times sfbuf allocation failed */
	uint64_t	sf_allocwait;	/* times sfbuf allocation had to wait */
};

#ifdef _KERNEL
#include <machine/sf_buf.h>
#include <sys/systm.h>
#include <sys/counter.h>
struct mbuf;	/* for sf_buf_mext() */

extern counter_u64_t sfstat[sizeof(struct sfstat) / sizeof(uint64_t)];
#define	SFSTAT_ADD(name, val)	\
    counter_u64_add(sfstat[offsetof(struct sfstat, name) / sizeof(uint64_t)],\
	(val))
#define	SFSTAT_INC(name)	SFSTAT_ADD(name, 1)
#endif /* _KERNEL */

int	sf_buf_mext(struct mbuf *mb, void *addr, void *args);

#endif /* !_SYS_SF_BUF_H_ */
