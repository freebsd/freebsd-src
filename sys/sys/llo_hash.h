/*-
 * Copyright (c) 2014 Randall Stewart <rrs@FreeBSD.org>
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
#ifndef __LL_HASH_H__
#define __LL_HASH_H__
#include <sys/types.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/counter.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/garbage_collector.h>

/* 
 * Lock Less often hash table - This hash table is *not*
 * lock free, but instead you only lock entries when you
 * put them in and take them out. It is completely safe
 * to read from this hash table with *no* locks.
 */
typedef u_long (*llo_hashfunc) (void *);
typedef int (*llo_comparefunc) (void *, void *); /* return 0 if two keys the same */
typedef int (*llo_freefunc) (void *);

#define LLO_MTX_INIT(l, i) mtx_init(&(l[i]), "llo_mtx", "llo_mutex", MTX_DEF|MTX_DUPOK)
#define LLO_MTX_DESTROY(l, i) mtx_destroy(&(l[i]))
#define LLO_MTX_LOCK(l, i) mtx_lock(&(l[i]))
#define LLO_MTX_UNLOCK(l, i) mtx_unlock(&(l[i]))

#define LLO_MMTX_INIT(l) mtx_init(&(l)->llo_mmtx, "llo_mmtx", "llo_mmutex", MTX_DEF|MTX_DUPOK)
#define LLO_MMTX_DESTROY(l) mtx_destroy(&(l)->llo_mmtx)
#define LLO_MMTX_LOCK(l) mtx_lock(&(l)->llo_mmtx)
#define LLO_MMTX_UNLOCK(l) mtx_unlock(&(l)->llo_mmtx)

struct llo_hash;

struct llo_hash_entry {
	LIST_ENTRY(llo_hash_entry) next;
	LIST_ENTRY(llo_hash_entry) cnext;
	struct llo_hash *parent;
	void *entry;
	void *key;
        counter_u64_t delete_epoch;
	struct garbage gar;
};

LIST_HEAD(llo_hash_head, llo_hash_entry);

#define LLO_IFLAG_MMTX     0x01
#define LLO_IFLAG_NOWAIT   0x02
#define LLO_IFLAG_PURGING  0x04
#define LLO_IFLAG_MINU64   0x10
#define LLO_IFLAG_CALLUP   0x20

/* Timer values used in callout() functions */
#define LLO_CALLOUT_SEC 2
#define LLO_CALLOUT_USEC 0

struct llo_hash {
	struct mtx llo_mmtx;
	struct mtx *llo_hmtx;
	counter_u64_t llo_epoch_start;
	counter_u64_t llo_epoch_end;
	llo_hashfunc llo_hf;
	llo_comparefunc llo_cf;
        llo_freefunc llo_ff;
	struct llo_hash_head *llo_ht;
	struct llo_hash_head lazy_clist;
	struct malloc_type *llo_typ;
	u_long llo_hashmod;
	struct callout lazy_clist_tmr;
        uint32_t entries;
        uint32_t being_deleted;
        uint8_t table_flags;
};


#define LLO_FLAGS_NOWAIT    0x0001	/* Call mallocs with M_NOWAIT (else M_WAITOK). */
#define LLO_FLAGS_MULTI_MTX 0x0002	/* Use multiple mutexs (one per hash bucket). */
#define LLO_FLAGS_MIN_U64   0x0004	/* 
					 * Defer allocation of counter_u64_t until removal
					 * from the table instead of at entry allocation.
					 */
/*
 * Initialize a lock less often hash table. It uses
 * an epoch style method to be able to *not* get any locks
 * on reading. The user must provide a removal function that
 * will be called to free the entry when it is safe to 
 * free the memory.
 */
struct llo_hash *
llo_hash_init(int nelements, 
	      struct malloc_type *typ, 
	      llo_hashfunc hf,
	      llo_comparefunc cf,
	      llo_freefunc ff,
	      size_t keysz,
	      int llo_flags);

/*
 * Destroy a previously built hash table. This will
 * fail with a non-zero return if any entries are in
 * the table, though entries can be being deleted.
 */
int llo_hash_destroy(struct llo_hash *);

/*
 * Add an entry to the hash table. The function will malloc
 * memory with your the malloc flags the same as that at
 * table creation.
 *
 * Note that the epoch is *not* moved forward during an add of
 * a new element. Failure can occur for memory reasons or if
 * the table destroy function has been called and you try to
 * add an entry (failure is a non-zero return).
 */
int llo_add_to_hash(struct llo_hash *llohash, void *entry, void *key);

/*
 * Lookup an entry via the key. This requires no lock, but it will
 * move forward the start epoch. A call to llo_release() is required
 * when you are finished looking at an entry.
 */
void *llo_hash_lookup(struct llo_hash *llohash, void *key);

/*
 * Release a entry moving the end epoch forward. It
 * is possible that the entry is *not* in the table, since
 * it may have been removed. (?Hmm should I not require the
 * arguments since we really don't use them other than
 * for verification and its ok to fail that verification?)
 */
void llo_release(struct llo_hash *llohash, void **entry, void *key);

/*
 * Delete an entry from the table. This does *not* advance either
 * epoch but does remove the entry from the table and then sets
 * up the delete_epoch coping in the values of the current
 * start epoch. This then is used in combination with the
 * garbage collector to validate all references are past
 * this entry, when that occurs the counters and bookkeeping
 * information is freed and then a call is made to the llo_freefunc
 * so that the user can engage the correct free routine on the memory
 * that was in the hash table. Note that the memory should remain
 * valid until this call since it is possible until that time that
 * some thread could be looking at the memory (usually only the
 * bookkeeping but you never know). This function may fail if
 * the entry is *not* in the table (failure is a non-zero return).
 */
int llo_del_from_hash(struct llo_hash *llohash, void *entry, void *key);
		     
#endif
