/*
 * bitops.c: atomic operations which got too long to be inlined all over
 *      the place.
 * 
 * Copyright 1999 Philipp Rumpf (prumpf@tux.org)
 * Copyright 2000 Grant Grundler (grundler@cup.hp.com)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <asm/system.h>
#include <asm/atomic.h>

#ifdef CONFIG_SMP
spinlock_t __atomic_hash[ATOMIC_HASH_SIZE] = {
	[0 ... (ATOMIC_HASH_SIZE-1)]  = SPIN_LOCK_UNLOCKED
};
#endif

spinlock_t __atomic_lock = SPIN_LOCK_UNLOCKED;

#ifdef __LP64__
unsigned long __xchg64(unsigned long x, unsigned long *ptr)
{
	unsigned long temp, flags;

	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(ptr), flags);
	temp = *ptr;
	*ptr = x;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(ptr), flags);
	return temp;
}
#endif

unsigned long __xchg32(int x, int *ptr)
{
	unsigned long flags;
	unsigned long temp;

	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(ptr), flags);
	(long) temp = (long) *ptr;	/* XXX - sign extension wanted? */
	*ptr = x;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(ptr), flags);
	return temp;
}


unsigned long __xchg8(char x, char *ptr)
{
	unsigned long flags;
	unsigned long temp;

	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(ptr), flags);
	(long) temp = (long) *ptr;	/* XXX - sign extension wanted? */
	*ptr = x;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(ptr), flags);
	return temp;
}


#ifdef __LP64__
unsigned long __cmpxchg_u64(volatile unsigned long *ptr, unsigned long old, unsigned long new)
{
	unsigned long flags;
	unsigned long prev;

	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(ptr), flags);
	if ((prev = *ptr) == old)
		*ptr = new;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(ptr), flags);
	return prev;
}
#endif

unsigned long __cmpxchg_u32(volatile unsigned int *ptr, unsigned int old, unsigned int new)
{
	unsigned long flags;
	unsigned int prev;

	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(ptr), flags);
	if ((prev = *ptr) == old)
		*ptr = new;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(ptr), flags);
	return (unsigned long)prev;
}
