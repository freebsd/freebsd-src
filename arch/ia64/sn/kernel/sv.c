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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/semaphore.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/current.h>

#include <asm/sn/sv.h>

/* Define this to have sv_test() run some simple tests.
   kernel_thread() must behave as expected when this is called.  */
#undef RUN_SV_TEST

#define DEBUG

/* Set up some macros so sv_wait(), sv_signal(), and sv_broadcast()
   can sanity check interrupt state on architectures where we know
   how. */
#ifdef DEBUG
 #define SV_DEBUG_INTERRUPT_STATE
 #ifdef __mips64
  #define SV_TEST_INTERRUPTS_ENABLED(flags) ((flags & 0x1) != 0)
  #define SV_TEST_INTERRUPTS_DISABLED(flags) ((flags & 0x1) == 0)
  #define SV_INTERRUPT_TEST_WORKERS 31
 #elif defined(__ia64)
  #define SV_TEST_INTERRUPTS_ENABLED(flags) ((flags & 0x4000) != 0)
  #define SV_TEST_INTERRUPTS_DISABLED(flags) ((flags & 0x4000) == 0)
  #define SV_INTERRUPT_TEST_WORKERS 4 /* simulator's slow */
 #else
  #undef  SV_DEBUG_INTERRUPT_STATE
  #define SV_INTERRUPT_TEST_WORKERS 4 /* reasonable? default. */
 #endif /* __mips64 */
#endif /* DEBUG */


/* XXX FIXME hack hack hack.  Our mips64 tree is from before the
   switch to WQ_FLAG_EXCLUSIVE, and our ia64 tree is from after it. */
#ifdef TASK_EXCLUSIVE
  #undef EXCLUSIVE_IN_QUEUE
#else
  #define EXCLUSIVE_IN_QUEUE
  #define TASK_EXCLUSIVE 0 /* for the set_current_state() in sv_wait() */
#endif


static inline void sv_lock(sv_t *sv) {
	spin_lock(&sv->sv_lock);
}

static inline void sv_unlock(sv_t *sv) {
	spin_unlock(&sv->sv_lock);
}

/* up() is "extern inline", so we can't pass its address to sv_wait.
   Use this function's address instead. */
static void up_wrapper(struct semaphore *sem) {
	up(sem);
}

/* spin_unlock() is sometimes a macro. */
static void spin_unlock_wrapper(spinlock_t *s) {
	spin_unlock(s);
}

/* XXX Perhaps sv_wait() should do the switch() each time and avoid
   the extra indirection and the need for the _wrapper functions? */

static inline void sv_set_mon_type(sv_t *sv, int type) {
	switch (type) {
	case SV_MON_SPIN:
		sv->sv_mon_unlock_func =
		  (sv_mon_unlock_func_t)spin_unlock_wrapper;
		break;
	case SV_MON_SEMA:
		sv->sv_mon_unlock_func =
		  (sv_mon_unlock_func_t)up_wrapper;
		if(sv->sv_flags & SV_INTS) {
			printk(KERN_ERR "sv_set_mon_type: The monitor lock "
			       "cannot be shared with interrupts if it is a "
			       "semaphore!\n");
			BUG();
		}
		if(sv->sv_flags & SV_BHS) {
			printk(KERN_ERR "sv_set_mon_type: The monitor lock "
			       "cannot be shared with bottom-halves if it is "
			       "a semaphore!\n");
			BUG();
		}
		break;
#if 0 
	/*
	 * If needed, and will need to think about interrupts.  This
	 * may be needed, for example, if someone wants to use sv's
	 * with something like dev_base; writers need to hold two
	 * locks. 
	 */
	case SV_MON_CUSTOM: 
		{
		struct sv_mon_custom *c = lock;
		sv->sv_mon_unlock_func = c->sv_mon_unlock_func;
		sv->sv_mon_lock        = c->sv_mon_lock;
		break;
		}
#endif
		
	default:
		printk(KERN_ERR "sv_set_mon_type: unknown type %d (0x%x)! "
		       "(flags 0x%x)\n", type, type, sv->sv_flags);
		BUG();
		break;
	}
	sv->sv_flags |= type;
}

static inline void sv_set_ord(sv_t *sv, int ord) {
	if (!ord)
		ord = SV_ORDER_DEFAULT;

	if (ord != SV_ORDER_FIFO && ord != SV_ORDER_LIFO) {
		printk(KERN_EMERG "sv_set_ord: unknown order %d (0x%x)! ",
		       ord, ord);
		BUG();
	}

	sv->sv_flags |= ord;
}

void sv_init(sv_t *sv, sv_mon_lock_t *lock, int flags) 
{
	int ord = flags & SV_ORDER_MASK;
	int type = flags & SV_MON_MASK;

	/* Copy all non-order, non-type flags */
	sv->sv_flags = (flags & ~(SV_ORDER_MASK | SV_MON_MASK));

	if((sv->sv_flags & (SV_INTS | SV_BHS)) == (SV_INTS | SV_BHS)) {
	  printk(KERN_ERR "sv_init: do not set both SV_INTS and SV_BHS, only SV_INTS.\n");
	  BUG();
	}

	sv_set_ord(sv, ord);
	sv_set_mon_type(sv, type);

	/* If lock is NULL, we'll get it from sv_wait_compat() (and
           ignore it in sv_signal() and sv_broadcast()). */
	sv->sv_mon_lock = lock;

	spin_lock_init(&sv->sv_lock);
	init_waitqueue_head(&sv->sv_waiters);
}

/*
 * The associated lock must be locked on entry.  It is unlocked on return.
 *
 * Return values:
 *
 * n < 0 : interrupted,  -n jiffies remaining on timeout, or -1 if timeout == 0
 * n = 0 : timeout expired
 * n > 0 : sv_signal()'d, n jiffies remaining on timeout, or 1 if timeout == 0
 */
signed long sv_wait(sv_t *sv, int sv_wait_flags, unsigned long timeout) 
{
	DECLARE_WAITQUEUE( wait, current );
	unsigned long flags;
	signed long ret = 0;

#ifdef SV_DEBUG_INTERRUPT_STATE
	{
	unsigned long flags;
	__save_flags(flags);

	if(sv->sv_flags & SV_INTS) {
		if(SV_TEST_INTERRUPTS_ENABLED(flags)) {
			printk(KERN_ERR "sv_wait: SV_INTS and interrupts "
			       "enabled (flags: 0x%lx)\n", flags);
			BUG();
		}
	} else {
		if (SV_TEST_INTERRUPTS_DISABLED(flags)) {
			printk(KERN_WARNING "sv_wait: !SV_INTS and interrupts "
			       "disabled! (flags: 0x%lx)\n", flags);
		}
	}
	}
#endif  /* SV_DEBUG_INTERRUPT_STATE */

	sv_lock(sv);

	sv->sv_mon_unlock_func(sv->sv_mon_lock);

	/* Add ourselves to the wait queue and set the state before
	 * releasing the sv_lock so as to avoid racing with the
	 * wake_up() in sv_signal() and sv_broadcast(). 
	 */

	/* don't need the _irqsave part, but there is no wq_write_lock() */
	wq_write_lock_irqsave(&sv->sv_waiters.lock, flags);

#ifdef EXCLUSIVE_IN_QUEUE
	wait.flags |= WQ_FLAG_EXCLUSIVE;
#endif

	switch(sv->sv_flags & SV_ORDER_MASK) {
	case SV_ORDER_FIFO:
		__add_wait_queue_tail(&sv->sv_waiters, &wait);
		break;
	case SV_ORDER_FILO:
		__add_wait_queue(&sv->sv_waiters, &wait);
		break;
	default:
		printk(KERN_ERR "sv_wait: unknown order!  (sv: 0x%p, flags: 0x%x)\n",
					(void *)sv, sv->sv_flags);
		BUG();
	}
	wq_write_unlock_irqrestore(&sv->sv_waiters.lock, flags);

	if(sv_wait_flags & SV_WAIT_SIG)
		set_current_state(TASK_EXCLUSIVE | TASK_INTERRUPTIBLE  );
	else
		set_current_state(TASK_EXCLUSIVE | TASK_UNINTERRUPTIBLE);

	spin_unlock(&sv->sv_lock);

	if(sv->sv_flags & SV_INTS)
		local_irq_enable();
	else if(sv->sv_flags & SV_BHS)
		local_bh_enable();

	if (timeout)
		ret = schedule_timeout(timeout);
	else
		schedule();

	if(current->state != TASK_RUNNING) /* XXX Is this possible? */ {
		printk(KERN_ERR "sv_wait: state not TASK_RUNNING after "
		       "schedule().\n");
		set_current_state(TASK_RUNNING);
	}

	remove_wait_queue(&sv->sv_waiters, &wait);

	/* Return cases:
	   - woken by a sv_signal/sv_broadcast
	   - woken by a signal
	   - woken by timeout expiring
	*/

	/* XXX This isn't really accurate; we may have been woken
           before the signal anyway.... */
	if(signal_pending(current))
		return timeout ? -ret : -1;
	return timeout ? ret : 1;
}


void sv_signal(sv_t *sv) 
{
	/* If interrupts can acquire this lock, they can also acquire the
	   sv_mon_lock, which we must already have to have called this, so
	   interrupts must be disabled already.  If interrupts cannot
	   contend for this lock, we don't have to worry about it. */

#ifdef SV_DEBUG_INTERRUPT_STATE
	if(sv->sv_flags & SV_INTS) {
		unsigned long flags;
		__save_flags(flags);
		if(SV_TEST_INTERRUPTS_ENABLED(flags))
			printk(KERN_ERR "sv_signal: SV_INTS and "
			"interrupts enabled! (flags: 0x%lx)\n", flags);
	}
#endif /* SV_DEBUG_INTERRUPT_STATE */

	sv_lock(sv);
	wake_up(&sv->sv_waiters);
	sv_unlock(sv);
}

void sv_broadcast(sv_t *sv) 
{
#ifdef SV_DEBUG_INTERRUPT_STATE
	if(sv->sv_flags & SV_INTS) {
		unsigned long flags;
		__save_flags(flags);
		if(SV_TEST_INTERRUPTS_ENABLED(flags))
			printk(KERN_ERR "sv_broadcast: SV_INTS and "
			       "interrupts enabled! (flags: 0x%lx)\n", flags);
	}
#endif /* SV_DEBUG_INTERRUPT_STATE */

	sv_lock(sv);
	wake_up_all(&sv->sv_waiters);
	sv_unlock(sv);
}

void sv_destroy(sv_t *sv) 
{
	if(!spin_trylock(&sv->sv_lock)) {
		printk(KERN_ERR "sv_destroy: someone else has sv 0x%p locked!\n", (void *)sv);
		BUG();
	}

	/* XXX Check that the waitqueue is empty? 
	       Mark the sv destroyed?
	*/
}


#ifdef RUN_SV_TEST

static DECLARE_MUTEX_LOCKED(talkback);
static DECLARE_MUTEX_LOCKED(sem);
sv_t sv;
sv_t sv_filo;

static int sv_test_1_w(void *arg) 
{
	printk("sv_test_1_w: acquiring spinlock 0x%p...\n", arg);

	spin_lock((spinlock_t*)arg);
	printk("sv_test_1_w: spinlock acquired, waking sv_test_1_s.\n");

	up(&sem);

	printk("sv_test_1_w: sv_spin_wait()'ing.\n");

	sv_spin_wait(&sv, arg);

	printk("sv_test_1_w: talkback.\n");
	up(&talkback);

	printk("sv_test_1_w: exiting.\n");
	return 0;
}

static int sv_test_1_s(void *arg) 
{
	printk("sv_test_1_s: waiting for semaphore.\n");
	down(&sem);
	printk("sv_test_1_s: semaphore acquired.  Acquiring spinlock.\n");
	spin_lock((spinlock_t*)arg);
	printk("sv_test_1_s: spinlock acquired.  sv_signaling.\n");
	sv_signal(&sv);
	printk("sv_test_1_s: talkback.\n");
	up(&talkback);
	printk("sv_test_1_s: exiting.\n");
	return 0;

}

static int count;
static DECLARE_MUTEX(monitor);

static int sv_test_2_w(void *arg) 
{
	int dummy = count++;
	sv_t *sv = (sv_t *)arg;

	down(&monitor);
	up(&talkback);
	printk("sv_test_2_w: thread %d started, sv_waiting.\n", dummy);
	sv_sema_wait(sv, &monitor);
	printk("sv_test_2_w: thread %d woken, exiting.\n", dummy);
	up(&sem);
	return 0;
}

static int sv_test_2_s_1(void *arg) 
{
	int i;
	sv_t *sv = (sv_t *)arg;

	down(&monitor);
	for(i = 0; i < 3; i++) {
		printk("sv_test_2_s_1: waking one thread.\n");
		sv_signal(sv);
		down(&sem);
	}

	printk("sv_test_2_s_1: signaling and broadcasting again.  Nothing should happen.\n");
	sv_signal(sv);
	sv_broadcast(sv);
	sv_signal(sv);
	sv_broadcast(sv);

	printk("sv_test_2_s_1: talkbacking.\n");
	up(&talkback);
	up(&monitor);
	return 0;
}

static int sv_test_2_s(void *arg) 
{
	int i;
	sv_t *sv = (sv_t *)arg;

	down(&monitor);
	for(i = 0; i < 3; i++) {
		printk("sv_test_2_s: waking one thread (should be %d.)\n", i);
		sv_signal(sv);
		down(&sem);
	}

	printk("sv_test_3_s: waking remaining threads with broadcast.\n");
	sv_broadcast(sv);
	for(; i < 10; i++)
		down(&sem);

	printk("sv_test_3_s: sending talkback.\n");
	up(&talkback);

	printk("sv_test_3_s: exiting.\n");
	up(&monitor);
	return 0;
}


static void big_test(sv_t *sv) 
{
	int i;

	count = 0;

	for(i = 0; i < 3; i++) {
		printk("big_test: spawning thread %d.\n", i);
		kernel_thread(sv_test_2_w, sv, 0);
		down(&talkback);
	}

	printk("big_test: spawning first wake-up thread.\n");
	kernel_thread(sv_test_2_s_1, sv, 0);

	down(&talkback);
	printk("big_test: talkback happened.\n");


	for(i = 3; i < 13; i++) {
		printk("big_test: spawning thread %d.\n", i);
		kernel_thread(sv_test_2_w, sv, 0);
		down(&talkback);
	}

	printk("big_test: spawning wake-up thread.\n");
	kernel_thread(sv_test_2_s, sv, 0);

	down(&talkback);
}

sv_t int_test_sv;
spinlock_t int_test_spin = SPIN_LOCK_UNLOCKED;
int int_test_ready;
static int irqtestcount;

static int interrupt_test_worker(void *unused) 
{
	int id = ++irqtestcount;
	int it = 0;
			unsigned long flags, flags2;

	printk("ITW: thread %d started.\n", id);

	while(1) {
		__save_flags(flags2);
		if(jiffies % 3) {
			printk("ITW %2d %5d: irqsaving          (%lx)\n", id, it, flags2);
			spin_lock_irqsave(&int_test_spin, flags);
		} else {
			printk("ITW %2d %5d: spin_lock_irqing   (%lx)\n", id, it, flags2);
			spin_lock_irq(&int_test_spin);
		}

		__save_flags(flags2);
		printk("ITW %2d %5d: locked, sv_waiting (%lx).\n", id, it, flags2);
		sv_wait(&int_test_sv, 0, 0);

		__save_flags(flags2);
		printk("ITW %2d %5d: wait finished      (%lx), pausing\n", id, it, flags2);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(jiffies & 0xf);
		if(current->state != TASK_RUNNING)
		  printk("ITW:  current->state isn't RUNNING after schedule!\n");
		it++;
	}
}

static void interrupt_test(void) 
{
	int i;

	printk("interrupt_test: initing sv.\n");
	sv_init(&int_test_sv, &int_test_spin, SV_MON_SPIN | SV_INTS);

	for(i = 0; i < SV_INTERRUPT_TEST_WORKERS; i++) {
		printk("interrupt_test: starting test thread %d.\n", i);
		kernel_thread(interrupt_test_worker, 0, 0);
	}
	printk("interrupt_test: done with init part.\n");
	int_test_ready = 1;
}

int sv_test(void) 
{
	spinlock_t s = SPIN_LOCK_UNLOCKED;

	sv_init(&sv, &s, SV_MON_SPIN);
	printk("sv_test: starting sv_test_1_w.\n");
	kernel_thread(sv_test_1_w, &s, 0);
	printk("sv_test: starting sv_test_1_s.\n");
	kernel_thread(sv_test_1_s, &s, 0);

	printk("sv_test: waiting for talkback.\n");
	down(&talkback); down(&talkback);
	printk("sv_test: talkback happened, sv_destroying.\n");
	sv_destroy(&sv);

	count = 0;

	printk("sv_test: beginning big_test on sv.\n");

	sv_init(&sv, &monitor, SV_MON_SEMA);
	big_test(&sv);
	sv_destroy(&sv);

	printk("sv_test: beginning big_test on sv_filo.\n");
	sv_init(&sv_filo, &monitor, SV_MON_SEMA | SV_ORDER_FILO);
	big_test(&sv_filo);
	sv_destroy(&sv_filo);

	interrupt_test();

	printk("sv_test: done.\n");
	return 0;
}

__initcall(sv_test);

#endif /* RUN_SV_TEST */
