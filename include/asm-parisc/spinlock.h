#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/spinlock_t.h>		/* get spinlock primitives */
#include <asm/psw.h>			/* local_* primitives need PSW_I */
#include <asm/system_irqsave.h>		/* get local_* primitives */

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 */
typedef struct {
	spinlock_t lock;
	volatile int counter;
} rwlock_t;

#define RW_LOCK_UNLOCKED (rwlock_t) { SPIN_LOCK_UNLOCKED, 0 }

#define rwlock_init(lp)	do { *(lp) = RW_LOCK_UNLOCKED; } while (0)

/* read_lock, read_unlock are pretty straightforward.  Of course it somehow
 * sucks we end up saving/restoring flags twice for read_lock_irqsave aso. */

static inline void read_lock(rwlock_t *rw)
{
	unsigned long flags;
	spin_lock_irqsave(&rw->lock, flags);

	rw->counter++;

	spin_unlock_irqrestore(&rw->lock, flags);
}

static inline void read_unlock(rwlock_t *rw)
{
	unsigned long flags;
	spin_lock_irqsave(&rw->lock, flags);

	rw->counter--;

	spin_unlock_irqrestore(&rw->lock, flags);
}

/* write_lock is less trivial.  We optimistically grab the lock and check
 * if we surprised any readers.  If so we release the lock and wait till
 * they're all gone before trying again
 *
 * Also note that we don't use the _irqsave / _irqrestore suffixes here.
 * If we're called with interrupts enabled and we've got readers (or other
 * writers) in interrupt handlers someone fucked up and we'd dead-lock
 * sooner or later anyway.   prumpf */

static inline void write_lock(rwlock_t *rw)
{
retry:
	spin_lock(&rw->lock);

	if(rw->counter != 0) {
		/* this basically never happens */
		spin_unlock(&rw->lock);

		while(rw->counter != 0);

		goto retry;
	}

	/* got it.  now leave without unlocking */
}

/* write_unlock is absolutely trivial - we don't have to wait for anything */

static inline void write_unlock(rwlock_t *rw)
{
	spin_unlock(&rw->lock);
}

#endif /* __ASM_SPINLOCK_H */
