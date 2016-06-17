#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/system.h>
#include <asm/processor.h>

#if defined(CONFIG_DEBUG_SPINLOCK)
#define SPINLOCK_DEBUG 1
#else
#define SPINLOCK_DEBUG 0
#endif

/*
 * Simple spin lock operations.
 */

typedef struct {
	volatile unsigned long lock;
#if SPINLOCK_DEBUG
	volatile unsigned long owner_pc;
	volatile unsigned long owner_cpu;
#endif
} spinlock_t;

#ifdef __KERNEL__
#if SPINLOCK_DEBUG
#define SPINLOCK_DEBUG_INIT     , 0, 0
#else
#define SPINLOCK_DEBUG_INIT     /* */
#endif

#define SPIN_LOCK_UNLOCKED	(spinlock_t) { 0 SPINLOCK_DEBUG_INIT }

#define spin_lock_init(x) 	do { *(x) = SPIN_LOCK_UNLOCKED; } while(0)
#define spin_is_locked(x)	((x)->lock != 0)
#define spin_unlock_wait(x)	do { barrier(); } while(spin_is_locked(x))

#if SPINLOCK_DEBUG

extern void _spin_lock(spinlock_t *lock);
extern void _spin_unlock(spinlock_t *lock);
extern int spin_trylock(spinlock_t *lock);
extern unsigned long __spin_trylock(volatile unsigned long *lock);

#define spin_lock(lp)			_spin_lock(lp)
#define spin_unlock(lp)			_spin_unlock(lp)

#else /* ! SPINLOCK_DEBUG */

static inline void spin_lock(spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"b	1f		# spin_lock\n\
2:	lwzx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	bne+	2b\n\
1:	lwarx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	bne-	2b\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%2,0,%1\n\
	bne-	2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&lock->lock), "r"(1)
	: "cr0", "memory");
}

static inline void spin_unlock(spinlock_t *lock)
{
	__asm__ __volatile__("eieio		# spin_unlock": : :"memory");
	lock->lock = 0;
}

#define spin_trylock(lock) (!test_and_set_bit(0,(lock)))

#endif

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 */
typedef struct {
	volatile unsigned long lock;
#if SPINLOCK_DEBUG
	volatile unsigned long owner_pc;
#endif
} rwlock_t;

#if SPINLOCK_DEBUG
#define RWLOCK_DEBUG_INIT     , 0
#else
#define RWLOCK_DEBUG_INIT     /* */
#endif

#define RW_LOCK_UNLOCKED (rwlock_t) { 0 RWLOCK_DEBUG_INIT }
#define rwlock_init(lp) do { *(lp) = RW_LOCK_UNLOCKED; } while(0)

#if SPINLOCK_DEBUG

extern void _read_lock(rwlock_t *rw);
extern void _read_unlock(rwlock_t *rw);
extern void _write_lock(rwlock_t *rw);
extern void _write_unlock(rwlock_t *rw);

#define read_lock(rw)		_read_lock(rw)
#define write_lock(rw)		_write_lock(rw)
#define write_unlock(rw)	_write_unlock(rw)
#define read_unlock(rw)		_read_unlock(rw)

#else /* ! SPINLOCK_DEBUG */

static __inline__ void read_lock(rwlock_t *rw)
{
	unsigned int tmp;

	__asm__ __volatile__(
	"b	2f		# read_lock\n\
1:	lwzx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	blt+	1b\n\
2:	lwarx	%0,0,%1\n\
	addic.	%0,%0,1\n\
	ble-	1b\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%0,0,%1\n\
	bne-	2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "memory");
}

static __inline__ void read_unlock(rwlock_t *rw)
{
	unsigned int tmp;

	__asm__ __volatile__(
	"eieio			# read_unlock\n\
1:	lwarx	%0,0,%1\n\
	addic	%0,%0,-1\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%0,0,%1\n\
	bne-	1b"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "memory");
}

static __inline__ void write_lock(rwlock_t *rw)
{
	unsigned int tmp;

	__asm__ __volatile__(
	"b	2f		# write_lock\n\
1:	lwzx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	bne+	1b\n\
2:	lwarx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	bne-	1b\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%2,0,%1\n\
	bne-	2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&rw->lock), "r"(-1)
	: "cr0", "memory");
}

static __inline__ void write_unlock(rwlock_t *rw)
{
	__asm__ __volatile__("eieio		# write_unlock": : :"memory");
	rw->lock = 0;
}

#endif

#endif /* __ASM_SPINLOCK_H */
#endif /* __KERNEL__ */
