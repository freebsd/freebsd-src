#ifndef _SPARC64_SEMAPHORE_H
#define _SPARC64_SEMAPHORE_H

/* These are actually reasonable on the V9.
 *
 * See asm-ppc/semaphore.h for implementation commentary,
 * only sparc64 specific issues are commented here.
 */
#ifdef __KERNEL__

#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/wait.h>
#include <linux/rwsem.h>

struct semaphore {
	atomic_t count;
	wait_queue_head_t wait;
};

#define __SEMAPHORE_INITIALIZER(name, count) \
	{ ATOMIC_INIT(count), \
	  __WAIT_QUEUE_HEAD_INITIALIZER((name).wait) }

#define __MUTEX_INITIALIZER(name) \
	__SEMAPHORE_INITIALIZER(name, 1)

#define __DECLARE_SEMAPHORE_GENERIC(name, count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name)		__DECLARE_SEMAPHORE_GENERIC(name, 1)
#define DECLARE_MUTEX_LOCKED(name)	__DECLARE_SEMAPHORE_GENERIC(name, 0)

static inline void sema_init (struct semaphore *sem, int val)
{
	atomic_set(&sem->count, val);
	init_waitqueue_head(&sem->wait);
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

extern void __down(struct semaphore * sem);
extern int  __down_interruptible(struct semaphore * sem);
extern void __up(struct semaphore * sem);

static __inline__ void down(struct semaphore * sem)
{
	/* This atomically does:
	 * 	old_val = sem->count;
	 *	new_val = sem->count - 1;
	 *	sem->count = new_val;
	 *	if (old_val < 1)
	 *		__down(sem);
	 *
	 * The (old_val < 1) test is equivalent to
	 * the more straightforward (new_val < 0),
	 * but it is easier to test the former because
	 * of how the CAS instruction works.
	 */

	__asm__ __volatile__("\n"
"	! down sem(%0)\n"
"1:	lduw	[%0], %%g5\n"
"	sub	%%g5, 1, %%g7\n"
"	cas	[%0], %%g5, %%g7\n"
"	cmp	%%g5, %%g7\n"
"	bne,pn	%%icc, 1b\n"
"	 cmp	%%g7, 1\n"
"	bl,pn	%%icc, 3f\n"
"	 membar	#StoreLoad | #StoreStore\n"
"2:\n"
"	.subsection 2\n"
"3:	mov	%0, %%g5\n"
"	save	%%sp, -160, %%sp\n"
"	mov	%%g1, %%l1\n"
"	mov	%%g2, %%l2\n"
"	mov	%%g3, %%l3\n"
"	call	%1\n"
"	 mov	%%g5, %%o0\n"
"	mov	%%l1, %%g1\n"
"	mov	%%l2, %%g2\n"
"	ba,pt	%%xcc, 2b\n"
"	 restore %%l3, %%g0, %%g3\n"
"	.previous\n"
	: : "r" (sem), "i" (__down)
	: "g5", "g7", "memory", "cc");
}

static __inline__ int down_interruptible(struct semaphore *sem)
{
	int ret = 0;
	
	/* This atomically does:
	 * 	old_val = sem->count;
	 *	new_val = sem->count - 1;
	 *	sem->count = new_val;
	 *	if (old_val < 1)
	 *		ret = __down_interruptible(sem);
	 *
	 * The (old_val < 1) test is equivalent to
	 * the more straightforward (new_val < 0),
	 * but it is easier to test the former because
	 * of how the CAS instruction works.
	 */

	__asm__ __volatile__("\n"
"	! down_interruptible sem(%2) ret(%0)\n"
"1:	lduw	[%2], %%g5\n"
"	sub	%%g5, 1, %%g7\n"
"	cas	[%2], %%g5, %%g7\n"
"	cmp	%%g5, %%g7\n"
"	bne,pn	%%icc, 1b\n"
"	 cmp	%%g7, 1\n"
"	bl,pn	%%icc, 3f\n"
"	 membar	#StoreLoad | #StoreStore\n"
"2:\n"
"	.subsection 2\n"
"3:	mov	%2, %%g5\n"
"	save	%%sp, -160, %%sp\n"
"	mov	%%g1, %%l1\n"
"	mov	%%g2, %%l2\n"
"	mov	%%g3, %%l3\n"
"	call	%3\n"
"	 mov	%%g5, %%o0\n"
"	mov	%%l1, %%g1\n"
"	mov	%%l2, %%g2\n"
"	mov	%%l3, %%g3\n"
"	ba,pt	%%xcc, 2b\n"
"	 restore %%o0, %%g0, %0\n"
"	.previous\n"
	: "=r" (ret)
	: "0" (ret), "r" (sem), "i" (__down_interruptible)
	: "g5", "g7", "memory", "cc");
	return ret;
}

static __inline__ int down_trylock(struct semaphore *sem)
{
	int ret;

	/* This atomically does:
	 * 	old_val = sem->count;
	 *	new_val = sem->count - 1;
	 *	if (old_val < 1) {
	 *		ret = 1;
	 *	} else {
	 *		sem->count = new_val;
	 *		ret = 0;
	 *	}
	 *
	 * The (old_val < 1) test is equivalent to
	 * the more straightforward (new_val < 0),
	 * but it is easier to test the former because
	 * of how the CAS instruction works.
	 */

	__asm__ __volatile__("\n"
"	! down_trylock sem(%1) ret(%0)\n"
"1:	lduw	[%1], %%g5\n"
"	sub	%%g5, 1, %%g7\n"
"	cmp	%%g5, 1\n"
"	bl,pn	%%icc, 2f\n"
"	 mov	1, %0\n"
"	cas	[%1], %%g5, %%g7\n"
"	cmp	%%g5, %%g7\n"
"	bne,pn	%%icc, 1b\n"
"	 mov	0, %0\n"
"	membar	#StoreLoad | #StoreStore\n"
"2:\n"
	: "=&r" (ret)
	: "r" (sem)
	: "g5", "g7", "memory", "cc");

	return ret;
}

static __inline__ void up(struct semaphore * sem)
{
	/* This atomically does:
	 * 	old_val = sem->count;
	 *	new_val = sem->count + 1;
	 *	sem->count = new_val;
	 *	if (old_val < 0)
	 *		__up(sem);
	 *
	 * The (old_val < 0) test is equivalent to
	 * the more straightforward (new_val <= 0),
	 * but it is easier to test the former because
	 * of how the CAS instruction works.
	 */

	__asm__ __volatile__("\n"
"	! up sem(%0)\n"
"	membar	#StoreLoad | #LoadLoad\n"
"1:	lduw	[%0], %%g5\n"
"	add	%%g5, 1, %%g7\n"
"	cas	[%0], %%g5, %%g7\n"
"	cmp	%%g5, %%g7\n"
"	bne,pn	%%icc, 1b\n"
"	 addcc	%%g7, 1, %%g0\n"
"	ble,pn	%%icc, 3f\n"
"	 membar	#StoreLoad | #StoreStore\n"
"2:\n"
"	.subsection 2\n"
"3:	mov	%0, %%g5\n"
"	save	%%sp, -160, %%sp\n"
"	mov	%%g1, %%l1\n"
"	mov	%%g2, %%l2\n"
"	mov	%%g3, %%l3\n"
"	call	%1\n"
"	 mov	%%g5, %%o0\n"
"	mov	%%l1, %%g1\n"
"	mov	%%l2, %%g2\n"
"	ba,pt	%%xcc, 2b\n"
"	 restore %%l3, %%g0, %%g3\n"
"	.previous\n"
	: : "r" (sem), "i" (__up)
	: "g5", "g7", "memory", "cc");
}

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif /* __KERNEL__ */

#endif /* !(_SPARC64_SEMAPHORE_H) */
