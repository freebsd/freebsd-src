/*
 * linux/include/asm-arm/semaphore.h
 */
#ifndef __ASM_ARM_SEMAPHORE_H
#define __ASM_ARM_SEMAPHORE_H

#include <linux/linkage.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/rwsem.h>

#include <asm/atomic.h>
#include <asm/proc/locks.h>

struct semaphore {
	atomic_t count;
	int sleepers;
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

#define __SEMAPHORE_INIT(name,count)			\
	{ ATOMIC_INIT(count), 0,			\
	  __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
	  __SEM_DEBUG_INIT(name) }

#define __MUTEX_INITIALIZER(name) \
	__SEMAPHORE_INIT(name,1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count)	\
	struct semaphore name = __SEMAPHORE_INIT(name,count)

#define DECLARE_MUTEX(name)		__DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name)	__DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init(struct semaphore *sem, int val)
{
	atomic_set(&sem->count, val);
	sem->sleepers = 0;
	init_waitqueue_head(&sem->wait);
#if WAITQUEUE_DEBUG
	sem->__magic = (long)&sem->__magic;
#endif
}

static inline void init_MUTEX(struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED(struct semaphore *sem)
{
	sema_init(sem, 0);
}

/*
 * special register calling convention
 */
asmlinkage void __down_failed(void);
asmlinkage int  __down_interruptible_failed(void);
asmlinkage int  __down_trylock_failed(void);
asmlinkage void __up_wakeup(void);

extern void __down(struct semaphore * sem);
extern int  __down_interruptible(struct semaphore * sem);
extern int  __down_trylock(struct semaphore * sem);
extern void __up(struct semaphore * sem);

/*
 * This is ugly, but we want the default case to fall through.
 * "__down" is the actual routine that waits...
 */
static inline void down(struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__down_op(sem, __down_failed);
}

/*
 * This is ugly, but we want the default case to fall through.
 * "__down_interruptible" is the actual routine that waits...
 */
static inline int down_interruptible (struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	return __down_op_ret(sem, __down_interruptible_failed);
}

static inline int down_trylock(struct semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	return __down_op_ret(sem, __down_trylock_failed);
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
static inline void up(struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__up_op(sem, __up_wakeup);
}

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif
