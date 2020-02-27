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
#define	SMR_SEQ_LT(a, b)	((smr_delta_t)((a)-(b)) < 0)
#define	SMR_SEQ_LEQ(a, b)	((smr_delta_t)((a)-(b)) <= 0)
#define	SMR_SEQ_GT(a, b)	((smr_delta_t)((a)-(b)) > 0)
#define	SMR_SEQ_GEQ(a, b)	((smr_delta_t)((a)-(b)) >= 0)
#define	SMR_SEQ_DELTA(a, b)	((smr_delta_t)((a)-(b)))
#define	SMR_SEQ_MIN(a, b)	(SMR_SEQ_LT((a), (b)) ? (a) : (b))
#define	SMR_SEQ_MAX(a, b)	(SMR_SEQ_GT((a), (b)) ? (a) : (b))

#define	SMR_SEQ_INVALID		0

/* Shared SMR state. */
union s_wr {
	struct {
		smr_seq_t	seq;	/* Current write sequence #. */
		int		ticks;	/* tick of last update (LAZY) */
	};
	uint64_t	_pair;
};
struct smr_shared {
	const char	*s_name;	/* Name for debugging/reporting. */
	union s_wr	s_wr;		/* Write sequence */
	smr_seq_t	s_rd_seq;	/* Minimum observed read sequence. */
};
typedef struct smr_shared *smr_shared_t;

/* Per-cpu SMR state. */
struct smr {
	smr_seq_t	c_seq;		/* Current observed sequence. */
	smr_shared_t	c_shared;	/* Shared SMR state. */
	int		c_deferred;	/* Deferred advance counter. */
	int		c_limit;	/* Deferred advance limit. */
	int		c_flags;	/* SMR Configuration */
};

#define	SMR_LAZY	0x0001		/* Higher latency write, fast read. */
#define	SMR_DEFERRED	0x0002		/* Aggregate updates to wr_seq. */

#define	SMR_ENTERED(smr)						\
    (curthread->td_critnest != 0 && zpcpu_get((smr))->c_seq != SMR_SEQ_INVALID)

#define	SMR_ASSERT_ENTERED(smr)						\
    KASSERT(SMR_ENTERED(smr), ("Not in smr section"))

#define	SMR_ASSERT_NOT_ENTERED(smr)					\
    KASSERT(!SMR_ENTERED(smr), ("In smr section."));

#define SMR_ASSERT(ex, fn)						\
    KASSERT((ex), (fn ": Assertion " #ex " failed at %s:%d", __FILE__, __LINE__))

/*
 * SMR Accessors are meant to provide safe access to SMR protected
 * pointers and prevent misuse and accidental access.
 *
 * Accessors are grouped by type:
 * entered	- Use while in a read section (between smr_enter/smr_exit())
 * serialized 	- Use while holding a lock that serializes writers.   Updates
 *		  are synchronized with readers via included barriers.
 * unserialized	- Use after the memory is out of scope and not visible to
 *		  readers.
 *
 * All acceses include a parameter for an assert to verify the required
 * synchronization.  For example, a writer might use:
 *
 * smr_serialized_store(pointer, value, mtx_owned(&writelock));
 *
 * These are only enabled in INVARIANTS kernels.
 */

/* Type restricting pointer access to force smr accessors. */
#define	SMR_TYPE_DECLARE(smrtype, type)					\
typedef struct {							\
	type	__ptr;		/* Do not access directly */		\
} smrtype

/*
 * Read from an SMR protected pointer while in a read section.
 */
#define	smr_entered_load(p, smr) ({					\
	SMR_ASSERT(SMR_ENTERED((smr)), "smr_entered_load");		\
	(__typeof((p)->__ptr))atomic_load_acq_ptr((uintptr_t *)&(p)->__ptr); \
})

/*
 * Read from an SMR protected pointer while serialized by an
 * external mechanism.  'ex' should contain an assert that the
 * external mechanism is held.  i.e. mtx_owned()
 */
#define	smr_serialized_load(p, ex) ({					\
	SMR_ASSERT(ex, "smr_serialized_load");				\
	(__typeof((p)->__ptr))atomic_load_ptr(&(p)->__ptr);		\
})

/*
 * Store 'v' to an SMR protected pointer while serialized by an
 * external mechanism.  'ex' should contain an assert that the
 * external mechanism is held.  i.e. mtx_owned()
 *
 * Writers that are serialized with mutual exclusion or on a single
 * thread should use smr_serialized_store() rather than swap.
 */
#define	smr_serialized_store(p, v, ex) do {				\
	SMR_ASSERT(ex, "smr_serialized_store");				\
	__typeof((p)->__ptr) _v = (v);					\
	atomic_store_rel_ptr((uintptr_t *)&(p)->__ptr, (uintptr_t)_v);	\
} while (0)

/*
 * swap 'v' with an SMR protected pointer and return the old value
 * while serialized by an external mechanism.  'ex' should contain
 * an assert that the external mechanism is provided.  i.e. mtx_owned()
 *
 * Swap permits multiple writers to update a pointer concurrently.
 */
#define	smr_serialized_swap(p, v, ex) ({				\
	SMR_ASSERT(ex, "smr_serialized_swap");				\
	__typeof((p)->__ptr) _v = (v);					\
	/* Release barrier guarantees contents are visible to reader */ \
	atomic_thread_fence_rel();					\
	(__typeof((p)->__ptr))atomic_swap_ptr(				\
	    (uintptr_t *)&(p)->__ptr, (uintptr_t)_v);			\
})

/*
 * Read from an SMR protected pointer when no serialization is required
 * such as in the destructor callback or when the caller guarantees other
 * synchronization.
 */
#define	smr_unserialized_load(p, ex) ({					\
	SMR_ASSERT(ex, "smr_unserialized_load");			\
	(__typeof((p)->__ptr))atomic_load_ptr(&(p)->__ptr);		\
})

/*
 * Store to an SMR protected pointer when no serialiation is required
 * such as in the destructor callback or when the caller guarantees other
 * synchronization.
 */
#define	smr_unserialized_store(p, v, ex) do {				\
	SMR_ASSERT(ex, "smr_unserialized_store");			\
	__typeof((p)->__ptr) _v = (v);					\
	atomic_store_ptr((uintptr_t *)&(p)->__ptr, (uintptr_t)_v);	\
} while (0)

/*
 * Return the current write sequence number.  This is not the same as the
 * current goal which may be in the future.
 */
static inline smr_seq_t
smr_shared_current(smr_shared_t s)
{

	return (atomic_load_int(&s->s_wr.seq));
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
	KASSERT((smr->c_flags & SMR_LAZY) == 0,
	    ("smr_enter(%s) lazy smr.", smr->c_shared->s_name));
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
	KASSERT((smr->c_flags & SMR_LAZY) == 0,
	    ("smr_exit(%s) lazy smr.", smr->c_shared->s_name));
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
 * Enter a lazy smr section.  This is used for read-mostly state that
 * can tolerate a high free latency.
 */
static inline void
smr_lazy_enter(smr_t smr)
{

	critical_enter();
	smr = zpcpu_get(smr);
	KASSERT((smr->c_flags & SMR_LAZY) != 0,
	    ("smr_lazy_enter(%s) non-lazy smr.", smr->c_shared->s_name));
	KASSERT(smr->c_seq == 0,
	    ("smr_lazy_enter(%s) does not support recursion.",
	    smr->c_shared->s_name));

	/*
	 * This needs no serialization.  If an interrupt occurs before we
	 * assign sr_seq to c_seq any speculative loads will be discarded.
	 * If we assign a stale wr_seq value due to interrupt we use the
	 * same algorithm that renders smr_enter() safe.
	 */
	atomic_store_int(&smr->c_seq, smr_shared_current(smr->c_shared));
}

/*
 * Exit a lazy smr section.  This is used for read-mostly state that
 * can tolerate a high free latency.
 */
static inline void
smr_lazy_exit(smr_t smr)
{

	smr = zpcpu_get(smr);
	CRITICAL_ASSERT(curthread);
	KASSERT((smr->c_flags & SMR_LAZY) != 0,
	    ("smr_lazy_enter(%s) non-lazy smr.", smr->c_shared->s_name));
	KASSERT(smr->c_seq != SMR_SEQ_INVALID,
	    ("smr_lazy_exit(%s) not in a smr section.", smr->c_shared->s_name));

	/*
	 * All loads/stores must be retired before the sequence becomes
	 * visible.  The fence compiles away on amd64.  Another
	 * alternative would be to omit the fence but store the exit
	 * time and wait 1 tick longer.
	 */
	atomic_thread_fence_rel();
	atomic_store_int(&smr->c_seq, SMR_SEQ_INVALID);
	critical_exit();
}

/*
 * Advances the write sequence number.  Returns the sequence number
 * required to ensure that all modifications are visible to readers.
 */
smr_seq_t smr_advance(smr_t smr);

/*
 * Returns true if a goal sequence has been reached.  If
 * wait is true this will busy loop until success.
 */
bool smr_poll(smr_t smr, smr_seq_t goal, bool wait);

/* Create a new SMR context. */
smr_t smr_create(const char *name, int limit, int flags);

/* Destroy the context. */
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
