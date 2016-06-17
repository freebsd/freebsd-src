#ifndef _ASM_IA64_SEMAPHORE_H
#define _ASM_IA64_SEMAPHORE_H

/*
 * Copyright (C) 1998-2000 Hewlett-Packard Co
 * Copyright (C) 1998-2000 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/wait.h>
#include <linux/rwsem.h>

#include <asm/atomic.h>

struct semaphore {
	atomic_t count;
	int sleepers;
	wait_queue_head_t wait;
#if WAITQUEUE_DEBUG
	long __magic;		/* initialized by __SEM_DEBUG_INIT() */
#endif
};

#if WAITQUEUE_DEBUG
# define __SEM_DEBUG_INIT(name)		, (long) &(name).__magic
#else
# define __SEM_DEBUG_INIT(name)
#endif

#define __SEMAPHORE_INITIALIZER(name,count)					\
{										\
	ATOMIC_INIT(count), 0, __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
	__SEM_DEBUG_INIT(name)							\
}

#define __MUTEX_INITIALIZER(name)	__SEMAPHORE_INITIALIZER(name,1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count)					\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, count)

#define DECLARE_MUTEX(name)		__DECLARE_SEMAPHORE_GENERIC(name, 1)
#define DECLARE_MUTEX_LOCKED(name)	__DECLARE_SEMAPHORE_GENERIC(name, 0)

static inline void
sema_init (struct semaphore *sem, int val)
{
	*sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
}

static inline void
init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void
init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

extern void __down (struct semaphore * sem);
extern int  __down_interruptible (struct semaphore * sem);
extern int  __down_trylock (struct semaphore * sem);
extern void __up (struct semaphore * sem);

/*
 * Atomically decrement the semaphore's count.  If it goes negative,
 * block the calling thread in the TASK_UNINTERRUPTIBLE state.
 */
static inline void
down (struct semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	if (atomic_dec_return(&sem->count) < 0)
		__down(sem);
}

/*
 * Atomically decrement the semaphore's count.  If it goes negative,
 * block the calling thread in the TASK_INTERRUPTIBLE state.
 */
static inline int
down_interruptible (struct semaphore * sem)
{
	int ret = 0;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_interruptible(sem);
	return ret;
}

static inline int
down_trylock (struct semaphore *sem)
{
	int ret = 0;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_trylock(sem);
	return ret;
}

static inline void
up (struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	if (atomic_inc_return(&sem->count) <= 0)
		__up(sem);
}

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif /* _ASM_IA64_SEMAPHORE_H */
