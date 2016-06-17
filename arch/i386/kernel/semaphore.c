/*
 * i386 semaphore implementation.
 *
 * (C) Copyright 1999 Linus Torvalds
 *
 * Portions Copyright 1999 Red Hat, Inc.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * rw semaphores implemented November 1999 by Benjamin LaHaise <bcrl@redhat.com>
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <asm/semaphore.h>

/*
 * Semaphores are implemented using a two-way counter:
 * The "count" variable is decremented for each process
 * that tries to acquire the semaphore, while the "sleeping"
 * variable is a count of such acquires.
 *
 * Notably, the inline "up()" and "down()" functions can
 * efficiently test if they need to do any extra work (up
 * needs to do something only if count was negative before
 * the increment operation.
 *
 * "sleeping" and the contention routine ordering is
 * protected by the semaphore spinlock.
 *
 * Note that these functions are only called when there is
 * contention on the lock, and as such all this is the
 * "non-critical" part of the whole semaphore business. The
 * critical part is the inline stuff in <asm/semaphore.h>
 * where we want to avoid any extra jumps and calls.
 */

/*
 * Logic:
 *  - only on a boundary condition do we need to care. When we go
 *    from a negative count to a non-negative, we wake people up.
 *  - when we go from a non-negative count to a negative do we
 *    (a) synchronize with the "sleeper" count and (b) make sure
 *    that we're on the wakeup list before we synchronize so that
 *    we cannot lose wakeup events.
 */

void __up(struct semaphore *sem)
{
	wake_up(&sem->wait);
}

static spinlock_t semaphore_lock = SPIN_LOCK_UNLOCKED;

void __down(struct semaphore * sem)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	tsk->state = TASK_UNINTERRUPTIBLE;
	add_wait_queue_exclusive(&sem->wait, &wait);

	spin_lock_irq(&semaphore_lock);
	sem->sleepers++;
	for (;;) {
		int sleepers = sem->sleepers;

		/*
		 * Add "everybody else" into it. They aren't
		 * playing, because we own the spinlock.
		 */
		if (!atomic_add_negative(sleepers - 1, &sem->count)) {
			sem->sleepers = 0;
			break;
		}
		sem->sleepers = 1;	/* us - see -1 above */
		spin_unlock_irq(&semaphore_lock);

		schedule();
		tsk->state = TASK_UNINTERRUPTIBLE;
		spin_lock_irq(&semaphore_lock);
	}
	spin_unlock_irq(&semaphore_lock);
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

	spin_lock_irq(&semaphore_lock);
	sem->sleepers ++;
	for (;;) {
		int sleepers = sem->sleepers;

		/*
		 * With signals pending, this turns into
		 * the trylock failure case - we won't be
		 * sleeping, and we* can't get the lock as
		 * it has contention. Just correct the count
		 * and exit.
		 */
		if (signal_pending(current)) {
			retval = -EINTR;
			sem->sleepers = 0;
			atomic_add(sleepers, &sem->count);
			break;
		}

		/*
		 * Add "everybody else" into it. They aren't
		 * playing, because we own the spinlock. The
		 * "-1" is because we're still hoping to get
		 * the lock.
		 */
		if (!atomic_add_negative(sleepers - 1, &sem->count)) {
			sem->sleepers = 0;
			break;
		}
		sem->sleepers = 1;	/* us - see -1 above */
		spin_unlock_irq(&semaphore_lock);

		schedule();
		tsk->state = TASK_INTERRUPTIBLE;
		spin_lock_irq(&semaphore_lock);
	}
	spin_unlock_irq(&semaphore_lock);
	tsk->state = TASK_RUNNING;
	remove_wait_queue(&sem->wait, &wait);
	wake_up(&sem->wait);
	return retval;
}

/*
 * Trylock failed - make sure we correct for
 * having decremented the count.
 *
 * We could have done the trylock with a
 * single "cmpxchg" without failure cases,
 * but then it wouldn't work on a 386.
 */
int __down_trylock(struct semaphore * sem)
{
	int sleepers;
	unsigned long flags;

	spin_lock_irqsave(&semaphore_lock, flags);
	sleepers = sem->sleepers + 1;
	sem->sleepers = 0;

	/*
	 * Add "everybody else" and us into it. They aren't
	 * playing, because we own the spinlock.
	 */
	if (!atomic_add_negative(sleepers, &sem->count))
		wake_up(&sem->wait);

	spin_unlock_irqrestore(&semaphore_lock, flags);
	return 1;
}


/*
 * The semaphore operations have a special calling sequence that
 * allow us to do a simpler in-line version of them. These routines
 * need to convert that sequence back into the C sequence when
 * there is contention on the semaphore.
 *
 * %ecx contains the semaphore pointer on entry. Save the C-clobbered
 * registers (%eax, %edx and %ecx) except %eax when used as a return
 * value..
 */
asm(
".text\n"
".align 4\n"
".globl __down_failed\n"
"__down_failed:\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"pushl %ebp\n\t"
	"movl  %esp,%ebp\n\t"
#endif
	"pushl %eax\n\t"
	"pushl %edx\n\t"
	"pushl %ecx\n\t"
	"call __down\n\t"
	"popl %ecx\n\t"
	"popl %edx\n\t"
	"popl %eax\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"movl %ebp,%esp\n\t"
	"popl %ebp\n\t"
#endif
	"ret"
);

asm(
".text\n"
".align 4\n"
".globl __down_failed_interruptible\n"
"__down_failed_interruptible:\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"pushl %ebp\n\t"
	"movl  %esp,%ebp\n\t"
#endif
	"pushl %edx\n\t"
	"pushl %ecx\n\t"
	"call __down_interruptible\n\t"
	"popl %ecx\n\t"
	"popl %edx\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"movl %ebp,%esp\n\t"
	"popl %ebp\n\t"
#endif
	"ret"
);

asm(
".text\n"
".align 4\n"
".globl __down_failed_trylock\n"
"__down_failed_trylock:\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"pushl %ebp\n\t"
	"movl  %esp,%ebp\n\t"
#endif
	"pushl %edx\n\t"
	"pushl %ecx\n\t"
	"call __down_trylock\n\t"
	"popl %ecx\n\t"
	"popl %edx\n\t"
#if defined(CONFIG_FRAME_POINTER)
	"movl %ebp,%esp\n\t"
	"popl %ebp\n\t"
#endif
	"ret"
);

asm(
".text\n"
".align 4\n"
".globl __up_wakeup\n"
"__up_wakeup:\n\t"
	"pushl %eax\n\t"
	"pushl %edx\n\t"
	"pushl %ecx\n\t"
	"call __up\n\t"
	"popl %ecx\n\t"
	"popl %edx\n\t"
	"popl %eax\n\t"
	"ret"
);

/*
 * rw spinlock fallbacks
 */
#if defined(CONFIG_SMP)
asm(
".text\n"
".align	4\n"
".globl	__write_lock_failed\n"
"__write_lock_failed:\n\t"
	LOCK "addl	$" RW_LOCK_BIAS_STR ",(%eax)\n"
"1:	rep; nop\n\t"
	"cmpl	$" RW_LOCK_BIAS_STR ",(%eax)\n\t"
	"jne	1b\n\t"
	LOCK "subl	$" RW_LOCK_BIAS_STR ",(%eax)\n\t"
	"jnz	__write_lock_failed\n\t"
	"ret"
);

asm(
".text\n"
".align	4\n"
".globl	__read_lock_failed\n"
"__read_lock_failed:\n\t"
	LOCK "incl	(%eax)\n"
"1:	rep; nop\n\t"
	"cmpl	$1,(%eax)\n\t"
	"js	1b\n\t"
	LOCK "decl	(%eax)\n\t"
	"js	__read_lock_failed\n\t"
	"ret"
);
#endif
