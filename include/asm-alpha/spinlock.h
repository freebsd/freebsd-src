#ifndef _ALPHA_SPINLOCK_H
#define _ALPHA_SPINLOCK_H

#include <linux/config.h>
#include <asm/system.h>
#include <linux/kernel.h>
#include <asm/current.h>


/*
 * Simple spin lock operations.  There are two variants, one clears IRQ's
 * on the local processor, one does not.
 *
 * We make no fairness assumptions. They have a cost.
 */

typedef struct {
	volatile unsigned int lock /*__attribute__((aligned(32))) */;
#if CONFIG_DEBUG_SPINLOCK
	int on_cpu;
	int line_no;
	void *previous;
	struct task_struct * task;
	const char *base_file;
#endif
} spinlock_t;

#if CONFIG_DEBUG_SPINLOCK
#define SPIN_LOCK_UNLOCKED (spinlock_t) {0, -1, 0, 0, 0, 0}
#define spin_lock_init(x)						\
	((x)->lock = 0, (x)->on_cpu = -1, (x)->previous = 0, (x)->task = 0)
#else
#define SPIN_LOCK_UNLOCKED	(spinlock_t) { 0 }
#define spin_lock_init(x)	((x)->lock = 0)
#endif

#define spin_is_locked(x)	((x)->lock != 0)
#define spin_unlock_wait(x)	({ do { barrier(); } while ((x)->lock); })

#if CONFIG_DEBUG_SPINLOCK
extern void spin_unlock(spinlock_t * lock);
extern void debug_spin_lock(spinlock_t * lock, const char *, int);
extern int debug_spin_trylock(spinlock_t * lock, const char *, int);

#define spin_lock(LOCK) debug_spin_lock(LOCK, __BASE_FILE__, __LINE__)
#define spin_trylock(LOCK) debug_spin_trylock(LOCK, __BASE_FILE__, __LINE__)

#define spin_lock_own(LOCK, LOCATION)					\
do {									\
	if (!((LOCK)->lock && (LOCK)->on_cpu == smp_processor_id()))	\
		printk("%s: called on %d from %p but lock %s on %d\n",	\
		       LOCATION, smp_processor_id(),			\
		       __builtin_return_address(0),			\
		       (LOCK)->lock ? "taken" : "freed", (LOCK)->on_cpu); \
} while (0)
#else
static inline void spin_unlock(spinlock_t * lock)
{
	mb();
	lock->lock = 0;
}

static inline void spin_lock(spinlock_t * lock)
{
	long tmp;

	/* Use sub-sections to put the actual loop at the end
	   of this object file's text section so as to perfect
	   branch prediction.  */
	__asm__ __volatile__(
	"1:	ldl_l	%0,%1\n"
	"	blbs	%0,2f\n"
	"	or	%0,1,%0\n"
	"	stl_c	%0,%1\n"
	"	beq	%0,2f\n"
	"	mb\n"
	".subsection 2\n"
	"2:	ldl	%0,%1\n"
	"	blbs	%0,2b\n"
	"	br	1b\n"
	".previous"
	: "=&r" (tmp), "=m" (lock->lock)
	: "m"(lock->lock) : "memory");
}

#define spin_trylock(lock) (!test_and_set_bit(0,(lock)))
#define spin_lock_own(LOCK, LOCATION)	((void)0)
#endif /* CONFIG_DEBUG_SPINLOCK */

/***********************************************************/

typedef struct {
	volatile int write_lock:1, read_counter:31;
} /*__attribute__((aligned(32)))*/ rwlock_t;

#define RW_LOCK_UNLOCKED (rwlock_t) { 0, 0 }

#define rwlock_init(x)	do { *(x) = RW_LOCK_UNLOCKED; } while(0)

#if CONFIG_DEBUG_RWLOCK
extern void write_lock(rwlock_t * lock);
extern void read_lock(rwlock_t * lock);
#else
static inline void write_lock(rwlock_t * lock)
{
	long regx;

	__asm__ __volatile__(
	"1:	ldl_l	%1,%0\n"
	"	bne	%1,6f\n"
	"	or	$31,1,%1\n"
	"	stl_c	%1,%0\n"
	"	beq	%1,6f\n"
	"	mb\n"
	".subsection 2\n"
	"6:	ldl	%1,%0\n"
	"	bne	%1,6b\n"
	"	br	1b\n"
	".previous"
	: "=m" (*(volatile int *)lock), "=&r" (regx)
	: "0" (*(volatile int *)lock) : "memory");
}

static inline void read_lock(rwlock_t * lock)
{
	long regx;

	__asm__ __volatile__(
	"1:	ldl_l	%1,%0\n"
	"	blbs	%1,6f\n"
	"	subl	%1,2,%1\n"
	"	stl_c	%1,%0\n"
	"	beq	%1,6f\n"
	"4:	mb\n"
	".subsection 2\n"
	"6:	ldl	%1,%0\n"
	"	blbs	%1,6b\n"
	"	br	1b\n"
	".previous"
	: "=m" (*(volatile int *)lock), "=&r" (regx)
	: "m" (*(volatile int *)lock) : "memory");
}
#endif /* CONFIG_DEBUG_RWLOCK */

static inline void write_unlock(rwlock_t * lock)
{
	mb();
	*(volatile int *)lock = 0;
}

static inline void read_unlock(rwlock_t * lock)
{
	long regx;
	__asm__ __volatile__(
	"	mb\n"
	"1:	ldl_l	%1,%0\n"
	"	addl	%1,2,%1\n"
	"	stl_c	%1,%0\n"
	"	beq	%1,6f\n"
	".subsection 2\n"
	"6:	br	1b\n"
	".previous"
	: "=m" (*(volatile int *)lock), "=&r" (regx)
	: "m" (*(volatile int *)lock) : "memory");
}

#endif /* _ALPHA_SPINLOCK_H */
