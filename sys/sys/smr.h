/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019, 2020 Jeffrey Roberson <jeff@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 *
 */

#ifndef _SYS_SMR_H_
#define	_SYS_SMR_H_

#include <sys/_smr.h>

/*
 * Safe memory reclamation.  See subr_smr.c for a description of the
 * algorithm.
 *
 * Readers synchronize with smr_enter()/exit() and writers may either
 * free directly to a SMR UMA zone or use smr_synchronize or wait.
 */

/*
 * Modular arithmetic for comparing sequence numbers that have
 * potentially wrapped.  Copied from tcp_seq.h.
 */
#define	SMR_SEQ_LT(a, b)	((int32_t)((a)-(b)) < 0)
#define	SMR_SEQ_LEQ(a, b)	((int32_t)((a)-(b)) <= 0)
#define	SMR_SEQ_GT(a, b)	((int32_t)((a)-(b)) > 0)
#define	SMR_SEQ_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)
#define	SMR_SEQ_DELTA(a, b)	((int32_t)((a)-(b)))

#define	SMR_SEQ_INVALID		0

/* Shared SMR state. */
struct smr_shared {
	const char	*s_name;	/* Name for debugging/reporting. */
	smr_seq_t	s_wr_seq;	/* Current write sequence #. */
	smr_seq_t	s_rd_seq;	/* Minimum observed read sequence. */
};
typedef struct smr_shared *smr_shared_t;

/* Per-cpu SMR state. */
struct smr {
	smr_seq_t	c_seq;		/* Current observed sequence. */
	smr_shared_t	c_shared;	/* Shared SMR state. */
	int		c_deferred;	/* Deferred advance counter. */
};

#define	SMR_ENTERED(smr)						\
    (curthread->td_critnest != 0 && zpcpu_get((smr))->c_seq != SMR_SEQ_INVALID)

#define	SMR_ASSERT_ENTERED(smr)						\
    KASSERT(SMR_ENTERED(smr), ("Not in smr section"))

#define	SMR_ASSERT_NOT_ENTERED(smr)					\
    KASSERT(!SMR_ENTERED(smr), ("In smr section."));

/*
 * Return the current write sequence number.
 */
static inline smr_seq_t
smr_shared_current(smr_shared_t s)
{

	return (atomic_load_int(&s->s_wr_seq));
}

static inline smr_seq_t
smr_current(smr_t smr)
{

	return (smr_shared_current(zpcpu_get(smr)->c_shared));
}

/*
 * Enter a read section.
 */
static inline void
smr_enter(smr_t smr)
{

	critical_enter();
	smr = zpcpu_get(smr);
	KASSERT(smr->c_seq == 0,
	    ("smr_enter(%s) does not support recursion.",
	    smr->c_shared->s_name));

	/*
	 * Store the current observed write sequence number in our
	 * per-cpu state so that it can be queried via smr_poll().
	 * Frees that are newer than this stored value will be
	 * deferred until we call smr_exit().
	 *
	 * An acquire barrier is used to synchronize with smr_exit()
	 * and smr_poll().
	 *
	 * It is possible that a long delay between loading the wr_seq
	 * and storing the c_seq could create a situation where the
	 * rd_seq advances beyond our stored c_seq.  In this situation
	 * only the observed wr_seq is stale, the fence still orders
	 * the load.  See smr_poll() for details on how this condition
	 * is detected and handled there.
	 */
	/* This is an add because we do not have atomic_store_acq_int */
	atomic_add_acq_int(&smr->c_seq, smr_shared_current(smr->c_shared));
}

/*
 * Exit a read section.
 */
static inline void
smr_exit(smr_t smr)
{

	smr = zpcpu_get(smr);
	CRITICAL_ASSERT(curthread);
	KASSERT(smr->c_seq != SMR_SEQ_INVALID,
	    ("smr_exit(%s) not in a smr section.", smr->c_shared->s_name));

	/*
	 * Clear the recorded sequence number.  This allows poll() to
	 * detect CPUs not in read sections.
	 *
	 * Use release semantics to retire any stores before the sequence
	 * number is cleared.
	 */
	atomic_store_rel_int(&smr->c_seq, SMR_SEQ_INVALID);
	critical_exit();
}

/*
 * Advances the write sequence number.  Returns the sequence number
 * required to ensure that all modifications are visible to readers.
 */
smr_seq_t smr_advance(smr_t smr);

/*
 * Advances the write sequence number only after N calls.  Returns
 * the correct goal for a wr_seq that has not yet occurred.  Used to
 * minimize shared cacheline invalidations for frequent writers.
 */
smr_seq_t smr_advance_deferred(smr_t smr, int limit);

/*
 * Returns true if a goal sequence has been reached.  If
 * wait is true this will busy loop until success.
 */
bool smr_poll(smr_t smr, smr_seq_t goal, bool wait);

/* Create a new SMR context. */
smr_t smr_create(const char *name);
void smr_destroy(smr_t smr);

/*
 * Blocking wait for all readers to observe 'goal'.
 */
static inline bool
smr_wait(smr_t smr, smr_seq_t goal)
{

	return (smr_poll(smr, goal, true));
}

/*
 * Synchronize advances the write sequence and returns when all
 * readers have observed it. 
 *
 * If your application can cache a sequence number returned from
 * smr_advance() and poll or wait at a later time there will
 * be less chance of busy looping while waiting for readers.
 */
static inline void
smr_synchronize(smr_t smr)
{

        smr_wait(smr, smr_advance(smr));
}

/* Only at startup. */
void smr_init(void);

#endif	/* _SYS_SMR_H_ */
