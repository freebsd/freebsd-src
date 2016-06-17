#ifndef _ASM_PARISC_SEMAPHORE_H
#define _ASM_PARISC_SEMAPHORE_H

/*
 * SMP- and interrupt-safe semaphores.
 *
 * (C) Copyright 1996 Linus Torvalds
 *
 * PA-RISC version by Matthew Wilcox
 *
 */

#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/rwsem.h>

#include <asm/system.h>

/*
 * The `count' is initialised to the number of people who are allowed to
 * take the lock.  (Normally we want a mutex, so this is `1').  if
 * `count' is positive, the lock can be taken.  if it's 0, no-one is
 * waiting on it.  if it's -1, at least one task is waiting.
 */
struct semaphore {
	spinlock_t	sentry;
	int		count;
	wait_queue_head_t wait;
#if WAITQUEUE_DEBUG
	long __magic;
#endif
};

#if WAITQUEUE_DEBUG
# define __SEM_DEBUG_INIT(name) \
		, (long)&(name).__magic
#else
# define __SEM_DEBUG_INIT(name)
#endif

#define __SEMAPHORE_INITIALIZER(name,count) \
{ SPIN_LOCK_UNLOCKED, count, __WAIT_QUEUE_HEAD_INITIALIZER((name).wait) \
	__SEM_DEBUG_INIT(name) }

#define __MUTEX_INITIALIZER(name) \
	__SEMAPHORE_INITIALIZER(name,1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

extern inline void sema_init (struct semaphore *sem, int val)
{
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER((*sem),val);
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

static inline int sem_getcount(struct semaphore *sem)
{
	return sem->count;
}

asmlinkage void __down(struct semaphore * sem);
asmlinkage int  __down_interruptible(struct semaphore * sem);
asmlinkage void __up(struct semaphore * sem);

/* Semaphores can be `tried' from irq context.  So we have to disable
 * interrupts while we're messing with the semaphore.  Sorry.
 */

extern __inline__ void down(struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	spin_lock_irq(&sem->sentry);
	if (sem->count > 0) {
		sem->count--;
	} else {
		__down(sem);
	}
	spin_unlock_irq(&sem->sentry);
}

extern __inline__ int down_interruptible(struct semaphore * sem)
{
	int ret = 0;
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	spin_lock_irq(&sem->sentry);
	if (sem->count > 0) {
		sem->count--;
	} else {
		ret = __down_interruptible(sem);
	}
	spin_unlock_irq(&sem->sentry);
	return ret;
}

/*
 * down_trylock returns 0 on success, 1 if we failed to get the lock.
 * May not sleep, but must preserve irq state
 */
extern __inline__ int down_trylock(struct semaphore * sem)
{
	int flags, count;
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	spin_lock_irqsave(&sem->sentry, flags);
	count = sem->count - 1;
	if (count >= 0)
		sem->count = count;
	spin_unlock_irqrestore(&sem->sentry, flags);
	return (count < 0);
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 */
extern __inline__ void up(struct semaphore * sem)
{
	int flags;
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	spin_lock_irqsave(&sem->sentry, flags);
	if (sem->count < 0) {
		__up(sem);
	} else {
		sem->count++;
	}
	spin_unlock_irqrestore(&sem->sentry, flags);
}

#endif /* _ASM_PARISC_SEMAPHORE_H */
