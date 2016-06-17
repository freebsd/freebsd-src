/*
 * Alpha semaphore implementation.
 *
 * (C) Copyright 1996 Linus Torvalds
 * (C) Copyright 1999, 2000 Richard Henderson
 */

#include <linux/sched.h>


/*
 * Semaphores are implemented using a two-way counter:
 * 
 * The "count" variable is decremented for each process that tries to sleep,
 * while the "waking" variable is incremented when the "up()" code goes to
 * wake up waiting processes.
 *
 * Notably, the inline "up()" and "down()" functions can efficiently test
 * if they need to do any extra work (up needs to do something only if count
 * was negative before the increment operation.
 *
 * waking_non_zero() (from asm/semaphore.h) must execute atomically.
 *
 * When __up() is called, the count was negative before incrementing it,
 * and we need to wake up somebody.
 *
 * This routine adds one to the count of processes that need to wake up and
 * exit.  ALL waiting processes actually wake up but only the one that gets
 * to the "waking" field first will gate through and acquire the semaphore.
 * The others will go back to sleep.
 *
 * Note that these functions are only called when there is contention on the
 * lock, and as such all this is the "non-critical" part of the whole
 * semaphore business. The critical part is the inline stuff in
 * <asm/semaphore.h> where we want to avoid any extra jumps and calls.
 */

/*
 * Perform the "down" function.  Return zero for semaphore acquired,
 * return negative for signalled out of the function.
 *
 * If called from down, the return is ignored and the wait loop is
 * not interruptible.  This means that a task waiting on a semaphore
 * using "down()" cannot be killed until someone does an "up()" on
 * the semaphore.
 *
 * If called from down_interruptible, the return value gets checked
 * upon return.  If the return value is negative then the task continues
 * with the negative value in the return register (it can be tested by
 * the caller).
 *
 * Either form may be used in conjunction with "up()".
 */

void
__down_failed(struct semaphore *sem)
{
	DECLARE_WAITQUEUE(wait, current);

#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down failed(%p)\n",
	       current->comm, current->pid, sem);
#endif

	current->state = TASK_UNINTERRUPTIBLE;
	wmb();
	add_wait_queue_exclusive(&sem->wait, &wait);

	/* At this point we know that sem->count is negative.  In order
	   to avoid racing with __up, we must check for wakeup before
	   going to sleep the first time.  */

	while (1) {
		long ret, tmp;

		/* An atomic conditional decrement of sem->waking.  */
		__asm__ __volatile__(
			"1:	ldl_l	%1,%2\n"
			"	blt	%1,2f\n"
			"	subl	%1,1,%0\n"
			"	stl_c	%0,%2\n"
			"	beq	%0,3f\n"
			"2:\n"
			".subsection 2\n"
			"3:	br	1b\n"
			".previous"
			: "=r"(ret), "=&r"(tmp), "=m"(sem->waking)
			: "0"(0));

		if (ret)
			break;

		schedule();
		set_task_state(current, TASK_UNINTERRUPTIBLE);
	}

	remove_wait_queue(&sem->wait, &wait);
	current->state = TASK_RUNNING;

#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down acquired(%p)\n",
	       current->comm, current->pid, sem);
#endif
}

int
__down_failed_interruptible(struct semaphore *sem)
{
	DECLARE_WAITQUEUE(wait, current);
	long ret;

#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down failed(%p)\n",
	       current->comm, current->pid, sem);
#endif

	current->state = TASK_INTERRUPTIBLE;
	wmb();
	add_wait_queue_exclusive(&sem->wait, &wait);

	while (1) {
		long tmp, tmp2, tmp3;

		/* We must undo the sem->count down_interruptible decrement
		   simultaneously and atomicly with the sem->waking
		   adjustment, otherwise we can race with __up.  This is
		   accomplished by doing a 64-bit ll/sc on two 32-bit words.
		
		   "Equivalent" C.  Note that we have to do this all without
		   (taken) branches in order to be a valid ll/sc sequence.

		   do {
		       tmp = ldq_l;
		       ret = 0;
		       if (tmp >= 0) {			// waking >= 0
		           tmp += 0xffffffff00000000;	// waking -= 1
		           ret = 1;
		       }
		       else if (pending) {
			   // count += 1, but since -1 + 1 carries into the
			   // high word, we have to be more careful here.
			   tmp = (tmp & 0xffffffff00000000)
				 | ((tmp + 1) & 0x00000000ffffffff);
		           ret = -EINTR;
		       }
		       tmp = stq_c = tmp;
		   } while (tmp == 0);
		*/

		__asm__ __volatile__(
			"1:	ldq_l	%1,%4\n"
			"	lda	%0,0\n"
			"	cmovne	%5,%6,%0\n"
			"	addq	%1,1,%2\n"
			"	and	%1,%7,%3\n"
			"	andnot	%2,%7,%2\n"
			"	cmovge	%1,1,%0\n"
			"	or	%3,%2,%2\n"
			"	addq	%1,%7,%3\n"
			"	cmovne	%5,%2,%1\n"
			"	cmovge	%2,%3,%1\n"
			"	stq_c	%1,%4\n"
			"	beq	%1,3f\n"
			"2:\n"
			".subsection 2\n"
			"3:	br	1b\n"
			".previous"
			: "=&r"(ret), "=&r"(tmp), "=&r"(tmp2),
			  "=&r"(tmp3), "=m"(*sem)
			: "r"(signal_pending(current)), "r"(-EINTR),
			  "r"(0xffffffff00000000));

		/* At this point we have ret
		  	1	got the lock
		  	0	go to sleep
		  	-EINTR	interrupted  */
		if (ret != 0)
			break;

		schedule();
		set_task_state(current, TASK_INTERRUPTIBLE);
	}

	remove_wait_queue(&sem->wait, &wait);
	current->state = TASK_RUNNING;
	wake_up(&sem->wait);

#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down %s(%p)\n",
	       current->comm, current->pid,
	       (ret < 0 ? "interrupted" : "acquired"), sem);
#endif

	/* Convert "got the lock" to 0==success.  */
	return (ret < 0 ? ret : 0);
}

void
__up_wakeup(struct semaphore *sem)
{
	wake_up(&sem->wait);
}

void
down(struct semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down(%p) <count=%d> from %p\n",
	       current->comm, current->pid, sem,
	       atomic_read(&sem->count), __builtin_return_address(0));
#endif
	__down(sem);
}

int
down_interruptible(struct semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down(%p) <count=%d> from %p\n",
	       current->comm, current->pid, sem,
	       atomic_read(&sem->count), __builtin_return_address(0));
#endif
	return __down_interruptible(sem);
}

int
down_trylock(struct semaphore *sem)
{
	int ret;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	ret = __down_trylock(sem);

#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): down_trylock %s from %p\n",
	       current->comm, current->pid,
	       ret ? "failed" : "acquired",
	       __builtin_return_address(0));
#endif

	return ret;
}

void
up(struct semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
#ifdef CONFIG_DEBUG_SEMAPHORE
	printk("%s(%d): up(%p) <count=%d> from %p\n",
	       current->comm, current->pid, sem,
	       atomic_read(&sem->count), __builtin_return_address(0));
#endif
	__up(sem);
}
