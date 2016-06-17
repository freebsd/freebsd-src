/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This implemenation of synchronization variables is heavily based on
 * one done by Steve Lord <lord@sgi.com>
 *
 * Paul Cassella <pwc@sgi.com>
 */

#ifndef _ASM_IA64_SN_SV_H
#define _ASM_IA64_SN_SV_H

#include <linux/spinlock.h>
#include <asm/semaphore.h>

#ifndef ASSERT

#define ASSERT(x) do {							  \
                    if(!(x)) {						  \
		      printk(KERN_ERR "%s\n", "Assertion failed: " # x);  \
		      BUG();						  \
		    }							  \
                  } while(0)
#define _SV_ASSERT
#endif

typedef void sv_mon_lock_t;
typedef void (*sv_mon_unlock_func_t)(sv_mon_lock_t *lock);

/* sv_flags values: */

#define SV_ORDER_FIFO        0x001
#define SV_ORDER_FILO        0x002
#define SV_ORDER_LIFO        SV_ORDER_FILO

/* If at some point one order becomes preferable to others, we can
   switch to it if the caller of sv_init doesn't specify. */
#define SV_ORDER_DEFAULT     SV_ORDER_FIFO

#define SV_ORDER_MASK        0x00f


#define SV_MON_SEMA          0x010
#define SV_MON_SPIN          0x020

#define SV_MON_MASK          0x0f0


/*
   If the monitor lock can be aquired from interrupts.  Note that this
   is a superset of the cases in which the sv can be touched from
   interrupts.

   This is currently only valid when the monitor lock is a spinlock.

   If this is used, sv_wait, sv_signal, and sv_broadcast must all be
   called with interrupts disabled, which has to happen anyway to have
   acquired the monitor spinlock. 
 */
#define SV_INTS              0x100

/* ditto for bottom halves */
#define SV_BHS               0x200



/* sv_wait_flag values: */
#define SV_WAIT_SIG          0x001 /* Allow sv_wait to be interrupted by a signal */

typedef struct sv_s {
	wait_queue_head_t sv_waiters;
	sv_mon_lock_t *sv_mon_lock;   /* Lock held for exclusive access to monitor. */
	sv_mon_unlock_func_t sv_mon_unlock_func;
	spinlock_t sv_lock;  /* Spinlock protecting the sv itself. */
	int sv_flags;
} sv_t;

#define DECLARE_SYNC_VARIABLE(sv, l, f) sv_t sv = sv_init(&sv, l, f)

/* 
 * @sv the sync variable to initialize
 * @monitor_lock the lock enforcing exclusive running in the monitor
 * @flags one of
 *   SV_MON_SEMA monitor_lock is a semaphore
 *   SV_MON_SPIN monitor_lock is a spinlock
 * and a bitwise or of some subset of
 *   SV_INTS - the monitor lock can be acquired from interrupts (and
 *             hence, whenever we hold it, interrupts are disabled or
 *             we're in an interrupt.)  This is only valid when
 *             SV_MON_SPIN is set.
 */
void sv_init(sv_t *sv, sv_mon_lock_t *monitor_lock, int flags);

/*
 * Set SV_WAIT_SIG in sv_wait_flags to let the sv_wait be interrupted by signals.
 *
 * timeout is how long to wait before giving up, or 0 to wait
 * indefinitely.  It is given in jiffies, and is relative.
 *
 * The associated lock must be locked on entry.  It is unlocked on return.
 *
 * Return values:
 *
 * n < 0 : interrupted,  -n jiffies remaining on timeout, or -1 if timeout == 0
 * n = 0 : timeout expired
 * n > 0 : sv_signal()'d, n jiffies remaining on timeout, or 1 if timeout == 0
 */
extern signed long sv_wait(sv_t *sv, int sv_wait_flags,
			   unsigned long timeout /* relative jiffies */);

static inline int sv_wait_compat(sv_t *sv, sv_mon_lock_t *lock, int sv_wait_flags,
				 unsigned long timeout, int sv_mon_type)
{
	ASSERT(sv_mon_type == (sv->sv_flags & SV_MON_MASK));
	if(sv->sv_mon_lock)
		ASSERT(lock == sv->sv_mon_lock);
	else
		sv->sv_mon_lock = lock;

	return sv_wait(sv, sv_wait_flags, timeout);
}


/* These work like Irix's sv_wait() and sv_wait_sig(), except the
   caller must call the one correpsonding to the type of the monitor
   lock. */
#define sv_spin_wait(sv, lock)                              \
        sv_wait_compat(sv, lock, 0, 0, SV_MON_SPIN)
#define sv_spin_wait_sig(sv, lock)                          \
        sv_wait_compat(sv, lock, SV_WAIT_SIG, 0, SV_MON_SPIN)

#define sv_sema_wait(sv, lock)                              \
        sv_wait_compat(sv, lock, 0, 0, SV_MON_SEMA)
#define sv_sema_wait_sig(sv, lock)                          \
        sv_wait_compat(sv, lock, SV_WAIT_SIG, 0, SV_MON_SEMA)

/* These work as in Irix. */
void sv_signal(sv_t *sv);
void sv_broadcast(sv_t *sv);

/* This works as in Irix. */
void sv_destroy(sv_t *sv);

#ifdef _SV_ASSERT
#undef ASSERT
#undef _SV_ASSERT
#endif

#endif /* _ASM_IA64_SN_SV_H */
