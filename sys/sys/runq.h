/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Jake Burkholder <jake@FreeBSD.org>
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
 */

#ifndef	_RUNQ_H_
#define	_RUNQ_H_

#include <sys/_param.h>
#include <sys/queue.h>

#ifdef _KERNEL
#include <sys/libkern.h>
#else
#include <strings.h>
#endif

struct thread;

/*
 * Run queue parameters.
 */

#define	RQ_MAX_PRIO	(255)	/* Maximum priority (minimum is 0). */
#define	RQ_PPQ		(4)	/* Priorities per queue. */

/*
 * Deduced from the above parameters and machine ones.
 */
typedef	unsigned long	rqb_word_t;	/* runq's status words type. */

#define	RQ_NQS	(howmany(RQ_MAX_PRIO + 1, RQ_PPQ)) /* Number of run queues. */
#define	RQB_BPW	(sizeof(rqb_word_t) * NBBY) /* Bits per runq word. */
#define	RQB_LEN	(howmany(RQ_NQS, RQB_BPW)) /* Words to cover RQ_NQS queues. */
#define	RQB_WORD(idx)	((idx) / RQB_BPW)
#define	RQB_BIT(idx)	(1ul << ((idx) % RQB_BPW))
#define	RQB_FFS(word)	(ffsl((long)(word)) - 1) /* Assumes two-complement. */

/*
 * Head of run queues.
 */
TAILQ_HEAD(rqhead, thread);

/*
 * Bit array which maintains the status of a run queue.  When a queue is
 * non-empty the bit corresponding to the queue number will be set.
 */
struct rqbits {
	rqb_word_t rqb_bits[RQB_LEN];
};

/*
 * Run queue structure.  Contains an array of run queues on which processes
 * are placed, and a structure to maintain the status of each queue.
 */
struct runq {
	struct	rqbits rq_status;
	struct	rqhead rq_queues[RQ_NQS];
};

void	runq_add(struct runq *, struct thread *, int);
void	runq_add_pri(struct runq *, struct thread *, u_char, int);
int	runq_check(struct runq *);
struct	thread *runq_choose(struct runq *);
struct	thread *runq_choose_from(struct runq *, u_char);
struct	thread *runq_choose_fuzz(struct runq *, int);
void	runq_init(struct runq *);
void	runq_remove(struct runq *, struct thread *);
void	runq_remove_idx(struct runq *, struct thread *, u_char *);

#endif
