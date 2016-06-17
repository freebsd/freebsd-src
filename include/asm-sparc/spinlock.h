/* spinlock.h: 32-bit Sparc spinlock support.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC_SPINLOCK_H
#define __SPARC_SPINLOCK_H

#include <linux/threads.h>	/* For NR_CPUS */

#ifndef __ASSEMBLY__

#include <asm/psr.h>

/*
 * Define this to use the verbose/debugging versions in
 * arch/sparc/lib/debuglocks.c
 *
 * Be sure to make dep whenever changing this option.
 */
#define SPIN_LOCK_DEBUG

#ifdef SPIN_LOCK_DEBUG
struct _spinlock_debug {
	unsigned char lock;
	unsigned long owner_pc;
};
typedef struct _spinlock_debug spinlock_t;

#define SPIN_LOCK_UNLOCKED	(spinlock_t) { 0, 0 }
#define spin_lock_init(lp)	do { *(lp)= SPIN_LOCK_UNLOCKED; } while(0)
#define spin_is_locked(lp)  (*((volatile unsigned char *)(&((lp)->lock))) != 0)
#define spin_unlock_wait(lp)	do { barrier(); } while(*(volatile unsigned char *)(&(lp)->lock))

extern void _do_spin_lock(spinlock_t *lock, char *str);
extern int _spin_trylock(spinlock_t *lock);
extern void _do_spin_unlock(spinlock_t *lock);

#define spin_trylock(lp)	_spin_trylock(lp)
#define spin_lock(lock)		_do_spin_lock(lock, "spin_lock")
#define spin_unlock(lock)	_do_spin_unlock(lock)

struct _rwlock_debug {
	volatile unsigned int lock;
	unsigned long owner_pc;
	unsigned long reader_pc[NR_CPUS];
};
typedef struct _rwlock_debug rwlock_t;

#define RW_LOCK_UNLOCKED (rwlock_t) { 0, 0, {0} }

#define rwlock_init(lp)	do { *(lp)= RW_LOCK_UNLOCKED; } while(0)

extern void _do_read_lock(rwlock_t *rw, char *str);
extern void _do_read_unlock(rwlock_t *rw, char *str);
extern void _do_write_lock(rwlock_t *rw, char *str);
extern void _do_write_unlock(rwlock_t *rw);

#define read_lock(lock)	\
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_do_read_lock(lock, "read_lock"); \
	__restore_flags(flags); \
} while(0)

#define read_unlock(lock) \
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_do_read_unlock(lock, "read_unlock"); \
	__restore_flags(flags); \
} while(0)

#define write_lock(lock) \
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_do_write_lock(lock, "write_lock"); \
	__restore_flags(flags); \
} while(0)

#define write_unlock(lock) \
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_do_write_unlock(lock); \
	__restore_flags(flags); \
} while(0)

#else /* !SPIN_LOCK_DEBUG */

typedef unsigned char spinlock_t;
#define SPIN_LOCK_UNLOCKED	0

#define spin_lock_init(lock)   (*((unsigned char *)(lock)) = 0)
#define spin_is_locked(lock)    (*((volatile unsigned char *)(lock)) != 0)

#define spin_unlock_wait(lock) \
do { \
	barrier(); \
} while(*((volatile unsigned char *)lock))

extern __inline__ void spin_lock(spinlock_t *lock)
{
	__asm__ __volatile__(
	"\n1:\n\t"
	"ldstub	[%0], %%g2\n\t"
	"orcc	%%g2, 0x0, %%g0\n\t"
	"bne,a	2f\n\t"
	" ldub	[%0], %%g2\n\t"
	".subsection	2\n"
	"2:\n\t"
	"orcc	%%g2, 0x0, %%g0\n\t"
	"bne,a	2b\n\t"
	" ldub	[%0], %%g2\n\t"
	"b,a	1b\n\t"
	".previous\n"
	: /* no outputs */
	: "r" (lock)
	: "g2", "memory", "cc");
}

extern __inline__ int spin_trylock(spinlock_t *lock)
{
	unsigned int result;
	__asm__ __volatile__("ldstub [%1], %0"
			     : "=r" (result)
			     : "r" (lock)
			     : "memory");
	return (result == 0);
}

extern __inline__ void spin_unlock(spinlock_t *lock)
{
	__asm__ __volatile__("stb %%g0, [%0]" : : "r" (lock) : "memory");
}

/* Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 *
 * XXX This might create some problems with my dual spinlock
 * XXX scheme, deadlocks etc. -DaveM
 */
typedef struct { volatile unsigned int lock; } rwlock_t;

#define RW_LOCK_UNLOCKED (rwlock_t) { 0 }

#define rwlock_init(lp)	do { *(lp)= RW_LOCK_UNLOCKED; } while(0)


/* Sort of like atomic_t's on Sparc, but even more clever.
 *
 *	------------------------------------
 *	| 24-bit counter           | wlock |  rwlock_t
 *	------------------------------------
 *	 31                       8 7     0
 *
 * wlock signifies the one writer is in or somebody is updating
 * counter. For a writer, if he successfully acquires the wlock,
 * but counter is non-zero, he has to release the lock and wait,
 * till both counter and wlock are zero.
 *
 * Unfortunately this scheme limits us to ~16,000,000 cpus.
 */
extern __inline__ void _read_lock(rwlock_t *rw)
{
	register rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_read_enter\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
}

#define read_lock(lock) \
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_read_lock(lock); \
	__restore_flags(flags); \
} while(0)

extern __inline__ void _read_unlock(rwlock_t *rw)
{
	register rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_read_exit\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
}

#define read_unlock(lock) \
do {	unsigned long flags; \
	__save_and_cli(flags); \
	_read_unlock(lock); \
	__restore_flags(flags); \
} while(0)

extern __inline__ void write_lock(rwlock_t *rw)
{
	register rwlock_t *lp asm("g1");
	lp = rw;
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___rw_write_enter\n\t"
	" ldstub	[%%g1 + 3], %%g2\n"
	: /* no outputs */
	: "r" (lp)
	: "g2", "g4", "memory", "cc");
}

#define write_unlock(rw)	do { (rw)->lock = 0; } while(0)

#endif /* SPIN_LOCK_DEBUG */

#endif /* !(__ASSEMBLY__) */

#endif /* __SPARC_SPINLOCK_H */
