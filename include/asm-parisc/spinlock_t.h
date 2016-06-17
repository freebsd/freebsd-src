#ifndef __PARISC_SPINLOCK_T_H
#define __PARISC_SPINLOCK_T_H

/* LDCW, the only atomic read-write operation PA-RISC has. *sigh*.
 *
 * Note that PA-RISC has to use `1' to mean unlocked and `0' to mean locked
 * since it only has load-and-zero.
 */
#ifdef CONFIG_PA20
/* 
> From: "Jim Hull" <jim.hull of hp.com>
> Delivery-date: Wed, 29 Jan 2003 13:57:05 -0500
> I've attached a summary of the change, but basically, for PA 2.0, as
> long as the ",CO" (coherent operation) completer is specified, then the
> 16-byte alignment requirement for ldcw and ldcd is relaxed, and instead
> they only require "natural" alignment (4-byte for ldcw, 8-byte for
> ldcd).
*/

#define __ldcw(a) ({ \
	unsigned __ret; \
	__asm__ __volatile__("ldcw,co 0(%1),%0" : "=r" (__ret) : "r" (a)); \
	__ret; \
})
#else
#define __ldcw(a) ({ \
	unsigned __ret; \
	__asm__ __volatile__("ldcw 0(%1),%0" : "=r" (__ret) : "r" (a)); \
	__ret; \
})
#endif

/*
 * Your basic SMP spinlocks, allowing only a single CPU anywhere
 */

typedef struct {
#ifdef CONFIG_PA20
	volatile unsigned int lock;
#else
	volatile unsigned int __attribute__((aligned(16))) lock;
#endif
#ifdef CONFIG_DEBUG_SPINLOCK
	volatile unsigned long owner_pc;
	volatile unsigned long owner_cpu;
#endif
} spinlock_t;

#ifndef CONFIG_DEBUG_SPINLOCK
#define SPIN_LOCK_UNLOCKED (spinlock_t) { 1 }

/* Define 6 spinlock primitives that don't depend on anything else. */

#define spin_lock_init(x)       do { (x)->lock = 1; } while(0)
#define spin_is_locked(x)       ((x)->lock == 0)
#define spin_trylock(x)		(__ldcw(&(x)->lock) != 0)
 
/* 
 * PA2.0 is not strongly ordered.  PA1.X is strongly ordered.
 * ldcw enforces ordering and we need to make sure ordering is
 * enforced on the unlock too.
 * "stw,ma" with Zero index is an alias for "stw,o".
 * But PA 1.x can assemble the "stw,ma" while it doesn't know about "stw,o".
 * And PA 2.0 will generate the right insn using either form.
 * Thanks to John David Anglin for this cute trick.
 *
 * Writing this with asm also ensures that the unlock doesn't
 * get reordered
 */
#define spin_unlock(x) \
	__asm__ __volatile__ ("stw,ma  %%sp,0(%0)" : : "r" (&(x)->lock) : "memory" )

#define spin_unlock_wait(x)     do { barrier(); } while(((volatile spinlock_t *)(x))->lock == 0)

#define spin_lock(x) do { \
	while (__ldcw (&(x)->lock) == 0) \
		while ((x)->lock == 0) ; \
} while (0)

#else

#define SPIN_LOCK_UNLOCKED (spinlock_t) { 1, 0, 0 }

/* Define 6 spinlock primitives that don't depend on anything else. */

#define spin_lock_init(x)       do { (x)->lock = 1; (x)->owner_cpu = 0; (x)->owner_pc = 0; } while(0)
#define spin_is_locked(x)       ((x)->lock == 0)
void spin_lock(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
#define spin_unlock_wait(x)     do { barrier(); } while(((volatile spinlock_t *)(x))->lock == 0)

#endif

#endif /* __PARISC_SPINLOCK_T_H */
