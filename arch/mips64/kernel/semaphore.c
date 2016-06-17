/*
 * Copyright (C) 1999, 2001, 02, 03 Ralf Baechle
 *
 * Heavily inspired by the Alpha implementation
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>

#ifdef CONFIG_CPU_HAS_LLDSCD
/*
 * On machines without lld/scd we need a spinlock to make the manipulation of
 * sem->count and sem->waking atomic.  Scalability isn't an issue because
 * this lock is used on UP only so it's just an empty variable.
 */
spinlock_t semaphore_lock = SPIN_LOCK_UNLOCKED;

EXPORT_SYMBOL(semaphore_lock);
#endif

/*
 * Semaphores are implemented using a two-way counter: The "count" variable is
 * decremented for each process that tries to sleep, while the "waking" variable
 * is incremented when the "up()" code goes to wake up waiting processes.
 *
 * Notably, the inline "up()" and "down()" functions can efficiently test if
 * they need to do any extra work (up needs to do something only if count was
 * negative before the increment operation.
 *
 * waking_non_zero() must execute atomically.
 *
 * When __up() is called, the count was negative before incrementing it, and we
 * need to wake up somebody.
 *
 * This routine adds one to the count of processes that need to wake up and
 * exit.  ALL waiting processes actually wake up but only the one that gets to
 * the "waking" field first will gate through and acquire the semaphore.  The
 * others will go back to sleep.
 *
 * Note that these functions are only called when there is contention on the
 * lock, and as such all this is the "non-critical" part of the whole semaphore
 * business. The critical part is the inline stuff in <asm/semaphore.h> where
 * we want to avoid any extra jumps and calls.
 */
void __up_wakeup(struct semaphore *sem)
{
	wake_up(&sem->wait);
}

EXPORT_SYMBOL(__up_wakeup);

#ifdef CONFIG_CPU_HAS_LLSC

static inline int waking_non_zero(struct semaphore *sem)
{
	int ret, tmp;

	__asm__ __volatile__(
	"1:	ll	%1, %2			# waking_non_zero	\n"
	"	blez	%1, 2f						\n"
	"	subu	%0, %1, 1					\n"
	"	sc	%0, %2						\n"
	"	beqz	%0, 1b						\n"
	"2:								\n"
	: "=r" (ret), "=r" (tmp), "+m" (sem->waking)
	: "0" (0));

	return ret;
}

#else /* !CONFIG_CPU_HAS_LLSC */

static inline int waking_non_zero(struct semaphore *sem)
{
	unsigned long flags;
	int waking, ret = 0;

	spin_lock_irqsave(&semaphore_lock, flags);
	waking = atomic_read(&sem->waking);
	if (waking > 0) {
		atomic_set(&sem->waking, waking - 1);
		ret = 1;
	}
	spin_unlock_irqrestore(&semaphore_lock, flags);

	return ret;
}

#endif /* !CONFIG_CPU_HAS_LLSC */

/*
 * Perform the "down" function.  Return zero for semaphore acquired, return
 * negative for signalled out of the function.
 *
 * If called from down, the return is ignored and the wait loop is not
 * interruptible.  This means that a task waiting on a semaphore using "down()"
 * cannot be killed until someone does an "up()" on the semaphore.
 *
 * If called from down_interruptible, the return value gets checked upon return.
 * If the return value is negative then the task continues with the negative
 * value in the return register (it can be tested by the caller).
 *
 * Either form may be used in conjunction with "up()".
 */

void __down_failed(struct semaphore * sem)
{
	struct task_struct *tsk = current;
	wait_queue_t wait;

	init_waitqueue_entry(&wait, tsk);
	__set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue_exclusive(&sem->wait, &wait);

	/*
	 * Ok, we're set up.  sem->count is known to be less than zero
	 * so we must wait.
	 *
	 * We can let go the lock for purposes of waiting.
	 * We re-acquire it after awaking so as to protect
	 * all semaphore operations.
	 *
	 * If "up()" is called before we call waking_non_zero() then
	 * we will catch it right away.  If it is called later then
	 * we will have to go through a wakeup cycle to catch it.
	 *
	 * Multiple waiters contend for the semaphore lock to see
	 * who gets to gate through and who has to wait some more.
	 */
	for (;;) {
		if (waking_non_zero(sem))
			break;
		schedule();
		__set_current_state(TASK_UNINTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&sem->wait, &wait);
}

EXPORT_SYMBOL(__down_failed);

#ifdef CONFIG_CPU_HAS_LLDSCD

/*
 * waking_non_zero_interruptible:
 *	1	got the lock
 *	0	go to sleep
 *	-EINTR	interrupted
 *
 * We must undo the sem->count down_interruptible decrement
 * simultaneously and atomically with the sem->waking adjustment,
 * otherwise we can race with wake_one_more.
 *
 * This is accomplished by doing a 64-bit lld/scd on the 2 32-bit words.
 *
 * This is crazy.  Normally it's strictly forbidden to use 64-bit operations
 * in the 32-bit MIPS kernel.  In this case it's however ok because if an
 * interrupt has destroyed the upper half of registers sc will fail.
 * Note also that this will not work for MIPS32 CPUs!
 *
 * Pseudocode:
 *
 * If(sem->waking > 0) {
 *	Decrement(sem->waking)
 *	Return(SUCCESS)
 * } else If(signal_pending(tsk)) {
 *	Increment(sem->count)
 *	Return(-EINTR)
 * } else {
 *	Return(SLEEP)
 * }
 */

static inline int
waking_non_zero_interruptible(struct semaphore *sem, struct task_struct *tsk)
{
	long ret, tmp;

	__asm__ __volatile__(
	"	.set	push		# waking_non_zero_interruptible	\n"
	"	.set	mips3						\n"
	"	.set	noat						\n"
	"0:	lld	%1, %2						\n"
	"	li	%0, 0						\n"
	"	sll	$1, %1, 0					\n"
	"	blez	$1, 1f						\n"
	"	daddiu	%1, %1, -1					\n"
	"	li	%0, 1						\n"
	"	b	2f						\n"
	"1:	beqz	%3, 2f						\n"
	"	li	%0, %4						\n"
	"	dli	$1, 0x0000000100000000				\n"
	"	daddu	%1, %1, $1					\n"
	"2:	scd	%1, %2						\n"
	"	beqz	%1, 0b						\n"
	"	.set	pop						\n"
	: "=&r" (ret), "=&r" (tmp), "=m" (*sem)
	: "r" (signal_pending(tsk)), "i" (-EINTR));

	return ret;
}

#else /* !CONFIG_CPU_HAS_LLDSCD */

static inline int waking_non_zero_interruptible(struct semaphore *sem,
						struct task_struct *tsk)
{
	int waking, pending, ret = 0;
	unsigned long flags;

	pending = signal_pending(tsk);

	spin_lock_irqsave(&semaphore_lock, flags);
	waking = atomic_read(&sem->waking);
	if (waking > 0) {
		atomic_set(&sem->waking, waking - 1);
		ret = 1;
	} else if (pending) {
		atomic_set(&sem->count, atomic_read(&sem->count) + 1);
		ret = -EINTR;
	}
	spin_unlock_irqrestore(&semaphore_lock, flags);

	return ret;
}

#endif /* !CONFIG_CPU_HAS_LLDSCD */

int __down_failed_interruptible(struct semaphore * sem)
{
	struct task_struct *tsk = current;
	wait_queue_t wait;
	int ret = 0;

	init_waitqueue_entry(&wait, tsk);
	__set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue_exclusive(&sem->wait, &wait);

	/*
	 * Ok, we're set up.  sem->count is known to be less than zero
	 * so we must wait.
	 *
	 * We can let go the lock for purposes of waiting.
	 * We re-acquire it after awaking so as to protect
	 * all semaphore operations.
	 *
	 * If "up()" is called before we call waking_non_zero() then
	 * we will catch it right away.  If it is called later then
	 * we will have to go through a wakeup cycle to catch it.
	 *
	 * Multiple waiters contend for the semaphore lock to see
	 * who gets to gate through and who has to wait some more.
	 */
	for (;;) {
		ret = waking_non_zero_interruptible(sem, tsk);
		if (ret) {
			if (ret == 1)
				/* ret != 0 only if we get interrupted -arca */
				ret = 0;
			break;
		}
		schedule();
		__set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&sem->wait, &wait);

	return ret;
}

EXPORT_SYMBOL(__down_failed_interruptible);
