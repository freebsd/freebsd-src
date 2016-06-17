/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996  Linus Torvalds
 * Copyright (C) 1998, 99, 2000, 01  Ralf Baechle
 * Copyright (C) 1999, 2000, 01  Silicon Graphics, Inc.
 * Copyright (C) 2000, 01 MIPS Technologies, Inc.
 */
#ifndef _ASM_SEMAPHORE_H
#define _ASM_SEMAPHORE_H

#include <linux/compiler.h>
#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/rwsem.h>
#include <asm/atomic.h>

struct semaphore {
#ifdef __MIPSEB__
	atomic_t count;
	atomic_t waking;
#else
	atomic_t waking;
	atomic_t count;
#endif
	wait_queue_head_t wait;
#if WAITQUEUE_DEBUG
	long __magic;
#endif
} __attribute__((aligned(8)));

#if WAITQUEUE_DEBUG
# define __SEM_DEBUG_INIT(name) , .__magic = (long)&(name).__magic
#else
# define __SEM_DEBUG_INIT(name)
#endif

#define __SEMAPHORE_INITIALIZER(name,_count) {				\
	.count	= ATOMIC_INIT(_count),					\
	.waking	= ATOMIC_INIT(0),					\
	.wait	= __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)		\
	__SEM_DEBUG_INIT(name)						\
}

#define __MUTEX_INITIALIZER(name) __SEMAPHORE_INITIALIZER(name, 1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name, 1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init (struct semaphore *sem, int val)
{
	atomic_set(&sem->count, val);
	atomic_set(&sem->waking, 0);
	init_waitqueue_head(&sem->wait);
#if WAITQUEUE_DEBUG
	sem->__magic = (long)&sem->__magic;
#endif
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

#ifndef CONFIG_CPU_HAS_LLDSCD
/*
 * On machines without lld/scd we need a spinlock to make the manipulation of
 * sem->count and sem->waking atomic.
 */
extern spinlock_t semaphore_lock;
#endif

extern void __down_failed(struct semaphore * sem);
extern int  __down_failed_interruptible(struct semaphore * sem);
extern void __up_wakeup(struct semaphore * sem);

static inline void down(struct semaphore * sem)
{
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	count = atomic_dec_return(&sem->count);
	if (unlikely(count < 0))
		__down_failed(sem);
}

/*
 * Interruptible try to acquire a semaphore.  If we obtained
 * it, return zero.  If we were interrupted, returns -EINTR
 */
static inline int down_interruptible(struct semaphore * sem)
{
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	count = atomic_dec_return(&sem->count);
	if (unlikely(count < 0))
		return __down_failed_interruptible(sem);

	return 0;
}

#ifdef CONFIG_CPU_HAS_LLDSCD

/*
 * down_trylock returns 0 on success, 1 if we failed to get the lock.
 *
 * We must manipulate count and waking simultaneously and atomically.
 * Here, we do this by using lld/scd on the pair of 32-bit words.
 *
 * Pseudocode:
 *
 *   Decrement(sem->count)
 *   If(sem->count >=0) {
 *	Return(SUCCESS)			// resource is free
 *   } else {
 *	If(sem->waking <= 0) {		// if no wakeup pending
 *	   Increment(sem->count)	// undo decrement
 *	   Return(FAILURE)
 *      } else {
 *	   Decrement(sem->waking)	// otherwise "steal" wakeup
 *	   Return(SUCCESS)
 *	}
 *   }
 */
static inline int down_trylock(struct semaphore * sem)
{
	long ret, tmp, tmp2, sub;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__asm__ __volatile__(
	"	.set	mips3			# down_trylock		\n"
	"0:	lld	%1, %4						\n"
	"	dli	%3, 0x0000000100000000	# count -= 1		\n"
	"	dsubu	%1, %3						\n"
	"	li	%0, 0			# ret = 0		\n"
	"	bgez	%1, 2f			# if count >= 0		\n"
	"	sll	%2, %1, 0		# extract waking	\n"
	"	blez	%2, 1f			# if waking < 0 -> 1f	\n"
	"	daddiu	%1, %1, -1		# waking -= 1		\n"
	"	b	2f						\n"
	"1:	daddu	%1, %1, %3		# count += 1		\n"
	"	li	%0, 1			# ret = 1		\n"
	"2:	scd	%1, %4						\n"
	"	beqz	%1, 0b						\n"
	"	sync							\n"
	"	.set	mips0						\n"
	: "=&r"(ret), "=&r"(tmp), "=&r"(tmp2), "=&r"(sub)
	: "m"(*sem)
	: "memory");

	return ret;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 */
static inline void up(struct semaphore * sem)
{
	unsigned long tmp, tmp2;
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	/*
	 * We must manipulate count and waking simultaneously and atomically.
	 * Otherwise we have races between up and __down_failed_interruptible
	 * waking up on a signal.
	 */

	__asm__ __volatile__(
	"	.set	mips3					\n"
	"	sync			# up			\n"
	"1:	lld	%1, %3					\n"
	"	dsra32	%0, %1, 0	# extract count to %0	\n"
	"	daddiu	%0, 1		# count += 1		\n"
	"	slti	%2, %0, 1	# %3 = (%0 <= 0)	\n"
	"	daddu	%1, %2		# waking += %3		\n"
	"	dsll32 %1, %1, 0	# zero-extend %1	\n"
	"	dsrl32 %1, %1, 0				\n"
	"	dsll32	%2, %0, 0	# Reassemble union	\n"
	"	or	%1, %2		# from count and waking	\n"
	"	scd	%1, %3					\n"
	"	beqz	%1, 1b					\n"
	"	.set	mips0					\n"
	: "=&r"(count), "=&r"(tmp), "=&r"(tmp2), "+m"(*sem)
	:
	: "memory");

	if (unlikely(count <= 0))
		__up_wakeup(sem);
}

#else

/*
 * Non-blockingly attempt to down() a semaphore.
 * Returns zero if we acquired it
 */
static inline int down_trylock(struct semaphore * sem)
{
	unsigned long flags;
	int count, waking;
	int ret = 0;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	spin_lock_irqsave(&semaphore_lock, flags);
	count = atomic_read(&sem->count) - 1;
	atomic_set(&sem->count, count);
	if (unlikely(count < 0)) {
		waking = atomic_read(&sem->waking);
		if (waking <= 0) {
			atomic_set(&sem->count, count + 1);
			ret = 1;
		} else {
			atomic_set(&sem->waking, waking - 1);
			ret = 0;
		}
	}
	spin_unlock_irqrestore(&semaphore_lock, flags);

	return ret;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 */
static inline void up(struct semaphore * sem)
{
	unsigned long flags;
	int count, waking;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif
	/*
	 * We must manipulate count and waking simultaneously and atomically.
	 * Otherwise we have races between up and __down_failed_interruptible
	 * waking up on a signal.
	 */

	spin_lock_irqsave(&semaphore_lock, flags);
	count = atomic_read(&sem->count) + 1;
	waking = atomic_read(&sem->waking);
	if (count <= 0)
		waking++;
	atomic_set(&sem->count, count);
	atomic_set(&sem->waking, waking);
	spin_unlock_irqrestore(&semaphore_lock, flags);

	if (unlikely(count <= 0))
		__up_wakeup(sem);
}

#endif /* CONFIG_CPU_HAS_LLDSCD */

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif /* _ASM_SEMAPHORE_H */
