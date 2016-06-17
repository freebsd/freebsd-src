#ifndef __LINUX_BRLOCK_H
#define __LINUX_BRLOCK_H

/*
 * 'Big Reader' read-write spinlocks.
 *
 * super-fast read/write locks, with write-side penalty. The point
 * is to have a per-CPU read/write lock. Readers lock their CPU-local
 * readlock, writers must lock all locks to get write access. These
 * CPU-read-write locks are semantically identical to normal rwlocks.
 * Memory usage is higher as well. (NR_CPUS*L1_CACHE_BYTES bytes)
 *
 * The most important feature is that these spinlocks do not cause
 * cacheline ping-pong in the 'most readonly data' case.
 *
 * Copyright 2000, Ingo Molnar <mingo@redhat.com>
 *
 * Registry idea and naming [ crutial! :-) ] by:
 *
 *                 David S. Miller <davem@redhat.com>
 *
 * David has an implementation that doesn't use atomic operations in
 * the read branch via memory ordering tricks - i guess we need to
 * split this up into a per-arch thing? The atomicity issue is a
 * secondary item in profiles, at least on x86 platforms.
 *
 * The atomic op version overhead is indeed a big deal on
 * load-locked/store-conditional cpus (ALPHA/MIPS/PPC) and
 * compare-and-swap cpus (Sparc64).  So we control which
 * implementation to use with a __BRLOCK_USE_ATOMICS define. -DaveM
 */

/* Register bigreader lock indices here. */
enum brlock_indices {
	BR_GLOBALIRQ_LOCK,
	BR_NETPROTO_LOCK,

	__BR_END
};

#include <linux/config.h>

#ifdef CONFIG_SMP

#include <linux/cache.h>
#include <linux/spinlock.h>

#if defined(__i386__) || defined(__ia64__) || defined(__x86_64__)
#define __BRLOCK_USE_ATOMICS
#else
#undef __BRLOCK_USE_ATOMICS
#endif

#ifdef __BRLOCK_USE_ATOMICS
typedef rwlock_t	brlock_read_lock_t;
#else
typedef unsigned int	brlock_read_lock_t;
#endif

/*
 * align last allocated index to the next cacheline:
 */
#define __BR_IDX_MAX \
	(((sizeof(brlock_read_lock_t)*__BR_END + SMP_CACHE_BYTES-1) & ~(SMP_CACHE_BYTES-1)) / sizeof(brlock_read_lock_t))

extern brlock_read_lock_t __brlock_array[NR_CPUS][__BR_IDX_MAX];

#ifndef __BRLOCK_USE_ATOMICS
struct br_wrlock {
	spinlock_t lock;
} __attribute__ ((__aligned__(SMP_CACHE_BYTES)));

extern struct br_wrlock __br_write_locks[__BR_IDX_MAX];
#endif

extern void __br_lock_usage_bug (void);

#ifdef __BRLOCK_USE_ATOMICS

static inline void br_read_lock (enum brlock_indices idx)
{
	/*
	 * This causes a link-time bug message if an
	 * invalid index is used:
	 */
	if (idx >= __BR_END)
		__br_lock_usage_bug();

	read_lock(&__brlock_array[smp_processor_id()][idx]);
}

static inline void br_read_unlock (enum brlock_indices idx)
{
	if (idx >= __BR_END)
		__br_lock_usage_bug();

	read_unlock(&__brlock_array[smp_processor_id()][idx]);
}

#else /* ! __BRLOCK_USE_ATOMICS */
static inline void br_read_lock (enum brlock_indices idx)
{
	unsigned int *ctr;
	spinlock_t *lock;

	/*
	 * This causes a link-time bug message if an
	 * invalid index is used:
	 */
	if (idx >= __BR_END)
		__br_lock_usage_bug();

	ctr = &__brlock_array[smp_processor_id()][idx];
	lock = &__br_write_locks[idx].lock;
again:
	(*ctr)++;
	mb();
	if (spin_is_locked(lock)) {
		(*ctr)--;
		wmb(); /*
			* The release of the ctr must become visible
			* to the other cpus eventually thus wmb(),
			* we don't care if spin_is_locked is reordered
			* before the releasing of the ctr.
			* However IMHO this wmb() is superflous even in theory.
			* It would not be superflous only if on the
			* other CPUs doing a ldl_l instead of an ldl
			* would make a difference and I don't think this is
			* the case.
			* I'd like to clarify this issue further
			* but for now this is a slow path so adding the
			* wmb() will keep us on the safe side.
			*/
		while (spin_is_locked(lock))
			barrier();
		goto again;
	}
}

static inline void br_read_unlock (enum brlock_indices idx)
{
	unsigned int *ctr;

	if (idx >= __BR_END)
		__br_lock_usage_bug();

	ctr = &__brlock_array[smp_processor_id()][idx];

	wmb();
	(*ctr)--;
}
#endif /* __BRLOCK_USE_ATOMICS */

/* write path not inlined - it's rare and larger */

extern void FASTCALL(__br_write_lock (enum brlock_indices idx));
extern void FASTCALL(__br_write_unlock (enum brlock_indices idx));

static inline void br_write_lock (enum brlock_indices idx)
{
	if (idx >= __BR_END)
		__br_lock_usage_bug();
	__br_write_lock(idx);
}

static inline void br_write_unlock (enum brlock_indices idx)
{
	if (idx >= __BR_END)
		__br_lock_usage_bug();
	__br_write_unlock(idx);
}

#else
# define br_read_lock(idx)	((void)(idx))
# define br_read_unlock(idx)	((void)(idx))
# define br_write_lock(idx)	((void)(idx))
# define br_write_unlock(idx)	((void)(idx))
#endif

/*
 * Now enumerate all of the possible sw/hw IRQ protected
 * versions of the interfaces.
 */
#define br_read_lock_irqsave(idx, flags) \
	do { local_irq_save(flags); br_read_lock(idx); } while (0)

#define br_read_lock_irq(idx) \
	do { local_irq_disable(); br_read_lock(idx); } while (0)

#define br_read_lock_bh(idx) \
	do { local_bh_disable(); br_read_lock(idx); } while (0)

#define br_write_lock_irqsave(idx, flags) \
	do { local_irq_save(flags); br_write_lock(idx); } while (0)

#define br_write_lock_irq(idx) \
	do { local_irq_disable(); br_write_lock(idx); } while (0)

#define br_write_lock_bh(idx) \
	do { local_bh_disable(); br_write_lock(idx); } while (0)

#define br_read_unlock_irqrestore(idx, flags) \
	do { br_read_unlock(irx); local_irq_restore(flags); } while (0)

#define br_read_unlock_irq(idx) \
	do { br_read_unlock(idx); local_irq_enable(); } while (0)

#define br_read_unlock_bh(idx) \
	do { br_read_unlock(idx); local_bh_enable(); } while (0)

#define br_write_unlock_irqrestore(idx, flags) \
	do { br_write_unlock(irx); local_irq_restore(flags); } while (0)

#define br_write_unlock_irq(idx) \
	do { br_write_unlock(idx); local_irq_enable(); } while (0)

#define br_write_unlock_bh(idx) \
	do { br_write_unlock(idx); local_bh_enable(); } while (0)

#endif /* __LINUX_BRLOCK_H */
