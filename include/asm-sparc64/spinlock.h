/* spinlock.h: 64-bit Sparc spinlock support.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC64_SPINLOCK_H
#define __SPARC64_SPINLOCK_H

#include <linux/config.h>

#ifndef __ASSEMBLY__

/* To get debugging spinlocks which detect and catch
 * deadlock situations, set CONFIG_DEBUG_SPINLOCK
 * and rebuild your kernel.
 */

/* All of these locking primitives are expected to work properly
 * even in an RMO memory model, which currently is what the kernel
 * runs in.
 *
 * There is another issue.  Because we play games to save cycles
 * in the non-contention case, we need to be extra careful about
 * branch targets into the "spinning" code.  They live in their
 * own section, but the newer V9 branches have a shorter range
 * than the traditional 32-bit sparc branch variants.  The rule
 * is that the branches that go into and out of the spinner sections
 * must be pre-V9 branches.
 */

#ifndef CONFIG_DEBUG_SPINLOCK

typedef unsigned char spinlock_t;
#define SPIN_LOCK_UNLOCKED	0

#define spin_lock_init(lock)	(*((unsigned char *)(lock)) = 0)
#define spin_is_locked(lock)	(*((volatile unsigned char *)(lock)) != 0)

#define spin_unlock_wait(lock)	\
do {	membar("#LoadLoad");	\
} while(*((volatile unsigned char *)lock))

extern __inline__ void spin_lock(spinlock_t *lock)
{
	__asm__ __volatile__(
"1:	ldstub		[%0], %%g7\n"
"	brnz,pn		%%g7, 2f\n"
"	 membar		#StoreLoad | #StoreStore\n"
"	.subsection	2\n"
"2:	ldub		[%0], %%g7\n"
"	brnz,pt		%%g7, 2b\n"
"	 membar		#LoadLoad\n"
"	b,a,pt		%%xcc, 1b\n"
"	.previous\n"
	: /* no outputs */
	: "r" (lock)
	: "g7", "memory");
}

extern __inline__ int spin_trylock(spinlock_t *lock)
{
	unsigned int result;
	__asm__ __volatile__("ldstub [%1], %0\n\t"
			     "membar #StoreLoad | #StoreStore"
			     : "=r" (result)
			     : "r" (lock)
			     : "memory");
	return (result == 0);
}

extern __inline__ void spin_unlock(spinlock_t *lock)
{
	__asm__ __volatile__("membar	#StoreStore | #LoadStore\n\t"
			     "stb	%%g0, [%0]"
			     : /* No outputs */
			     : "r" (lock)
			     : "memory");
}

#else /* !(CONFIG_DEBUG_SPINLOCK) */

typedef struct {
	unsigned char lock;
	unsigned int owner_pc, owner_cpu;
} spinlock_t;
#define SPIN_LOCK_UNLOCKED (spinlock_t) { 0, 0, 0xff }
#define spin_lock_init(__lock)	\
do {	(__lock)->lock = 0; \
	(__lock)->owner_pc = 0; \
	(__lock)->owner_cpu = 0xff; \
} while(0)
#define spin_is_locked(__lock)	(*((volatile unsigned char *)(&((__lock)->lock))) != 0)
#define spin_unlock_wait(__lock)	\
do { \
	membar("#LoadLoad"); \
} while(*((volatile unsigned char *)(&((__lock)->lock))))

extern void _do_spin_lock (spinlock_t *lock, char *str);
extern void _do_spin_unlock (spinlock_t *lock);
extern int _spin_trylock (spinlock_t *lock);

#define spin_trylock(lp)	_spin_trylock(lp)
#define spin_lock(lock)		_do_spin_lock(lock, "spin_lock")
#define spin_unlock(lock)	_do_spin_unlock(lock)

#endif /* CONFIG_DEBUG_SPINLOCK */

/* Multi-reader locks, these are much saner than the 32-bit Sparc ones... */

#ifndef CONFIG_DEBUG_SPINLOCK

typedef unsigned int rwlock_t;
#define RW_LOCK_UNLOCKED	0
#define rwlock_init(lp) do { *(lp) = RW_LOCK_UNLOCKED; } while(0)

extern void __read_lock(rwlock_t *);
extern void __read_unlock(rwlock_t *);
extern void __write_lock(rwlock_t *);
extern void __write_unlock(rwlock_t *);

#define read_lock(p)	__read_lock(p)
#define read_unlock(p)	__read_unlock(p)
#define write_lock(p)	__write_lock(p)
#define write_unlock(p)	__write_unlock(p)

#else /* !(CONFIG_DEBUG_SPINLOCK) */

typedef struct {
	unsigned long lock;
	unsigned int writer_pc, writer_cpu;
	unsigned int reader_pc[4];
} rwlock_t;
#define RW_LOCK_UNLOCKED	(rwlock_t) { 0, 0, 0xff, { 0, 0, 0, 0 } }
#define rwlock_init(lp) do { *(lp) = RW_LOCK_UNLOCKED; } while(0)

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

#endif /* CONFIG_DEBUG_SPINLOCK */

#endif /* !(__ASSEMBLY__) */

#endif /* !(__SPARC64_SPINLOCK_H) */
