/*
 * SMP- and interrupt-safe semaphores helper functions. Generic versions, no
 * optimizations whatsoever...
 *
 */

#ifndef _ASM_SEMAPHORE_HELPER_H
#define _ASM_SEMAPHORE_HELPER_H

#include <asm/atomic.h>

#define read(a) ((a)->counter)
#define inc(a) (((a)->counter)++)
#define dec(a) (((a)->counter)--)

#define count_inc(a) ((*(a))++)

/*
 * These two _must_ execute atomically wrt each other.
 */
extern inline void wake_one_more(struct semaphore * sem)
{
	atomic_inc(&sem->waking);
}

extern inline int waking_non_zero(struct semaphore *sem)
{
	unsigned long flags;
	int ret = 0;

	save_and_cli(flags);
	if (read(&sem->waking) > 0) {
		dec(&sem->waking);
		ret = 1;
	}
	restore_flags(flags);
	return ret;
}

extern inline int waking_non_zero_interruptible(struct semaphore *sem,
						struct task_struct *tsk)
{
	int ret = 0;
	unsigned long flags;

	save_and_cli(flags);
	if (read(&sem->waking) > 0) {
		dec(&sem->waking);
		ret = 1;
	} else if (signal_pending(tsk)) {
		count_inc(&sem->count);
		ret = -EINTR;
	}
	restore_flags(flags);
	return ret;
}

extern inline int waking_non_zero_trylock(struct semaphore *sem)
{
        int ret = 1;
	unsigned long flags;

	save_and_cli(flags);
	if (read(&sem->waking) <= 0)
		count_inc(&sem->count);
	else {
		dec(&sem->waking);
		ret = 0;
	}
	restore_flags(flags);
	return ret;
}

#endif /* _ASM_SEMAPHORE_HELPER_H */
