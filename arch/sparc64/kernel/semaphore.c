/* $Id: semaphore.c,v 1.9 2001/11/18 00:12:56 davem Exp $
 * semaphore.c: Sparc64 semaphore implementation.
 *
 * This is basically the PPC semaphore scheme ported to use
 * the sparc64 atomic instructions, so see the PPC code for
 * credits.
 */

#include <linux/sched.h>

/*
 * Atomically update sem->count.
 * This does the equivalent of the following:
 *
 *	old_count = sem->count;
 *	tmp = MAX(old_count, 0) + incr;
 *	sem->count = tmp;
 *	return old_count;
 */
static __inline__ int __sem_update_count(struct semaphore *sem, int incr)
{
	int old_count, tmp;

	__asm__ __volatile__("\n"
"	! __sem_update_count old_count(%0) tmp(%1) incr(%4) &sem->count(%3)\n"
"1:	ldsw	[%3], %0\n"
"	mov	%0, %1\n"
"	cmp	%0, 0\n"
"	movl	%%icc, 0, %1\n"
"	add	%1, %4, %1\n"
"	cas	[%3], %0, %1\n"
"	cmp	%0, %1\n"
"	bne,pn	%%icc, 1b\n"
"	 membar #StoreLoad | #StoreStore\n"
	: "=&r" (old_count), "=&r" (tmp), "=m" (sem->count)
	: "r" (&sem->count), "r" (incr), "m" (sem->count)
	: "cc");

	return old_count;
}

void __up(struct semaphore *sem)
{
	__sem_update_count(sem, 1);
	wake_up(&sem->wait);
}

void __down(struct semaphore * sem)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	tsk->state = TASK_UNINTERRUPTIBLE;
	add_wait_queue_exclusive(&sem->wait, &wait);

	while (__sem_update_count(sem, -1) <= 0) {
		schedule();
		tsk->state = TASK_UNINTERRUPTIBLE;
	}
	remove_wait_queue(&sem->wait, &wait);
	tsk->state = TASK_RUNNING;

	wake_up(&sem->wait);
}

int __down_interruptible(struct semaphore * sem)
{
	int retval = 0;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	tsk->state = TASK_INTERRUPTIBLE;
	add_wait_queue_exclusive(&sem->wait, &wait);

	while (__sem_update_count(sem, -1) <= 0) {
		if (signal_pending(current)) {
			__sem_update_count(sem, 0);
			retval = -EINTR;
			break;
		}
		schedule();
		tsk->state = TASK_INTERRUPTIBLE;
	}
	tsk->state = TASK_RUNNING;
	remove_wait_queue(&sem->wait, &wait);
	wake_up(&sem->wait);
	return retval;
}
