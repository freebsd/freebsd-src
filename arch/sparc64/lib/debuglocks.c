/* $Id: debuglocks.c,v 1.9 2001/11/17 00:10:48 davem Exp $
 * debuglocks.c: Debugging versions of SMP locking primitives.
 *
 * Copyright (C) 1998 David S. Miller (davem@redhat.com)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/system.h>

#if defined(CONFIG_SMP) && defined(CONFIG_DEBUG_SPINLOCK)

#define GET_CALLER(PC) __asm__ __volatile__("mov %%i7, %0" : "=r" (PC))

static inline void show (char *str, spinlock_t *lock, unsigned long caller)
{
	int cpu = smp_processor_id();

	printk("%s(%p) CPU#%d stuck at %08x, owner PC(%08x):CPU(%x)\n",
	       str, lock, cpu, (unsigned int) caller,
	       lock->owner_pc, lock->owner_cpu);
}

static inline void show_read (char *str, rwlock_t *lock, unsigned long caller)
{
	int cpu = smp_processor_id();

	printk("%s(%p) CPU#%d stuck at %08x, writer PC(%08x):CPU(%x)\n",
	       str, lock, cpu, (unsigned int) caller,
	       lock->writer_pc, lock->writer_cpu);
}

static inline void show_write (char *str, rwlock_t *lock, unsigned long caller)
{
	int cpu = smp_processor_id();

	printk("%s(%p) CPU#%d stuck at %08x\n",
	       str, lock, cpu, (unsigned int) caller);
	printk("Writer: PC(%08x):CPU(%x)\n",
	       lock->writer_pc, lock->writer_cpu);
	printk("Readers: 0[%08x] 1[%08x] 2[%08x] 4[%08x]\n",
	       lock->reader_pc[0], lock->reader_pc[1],
	       lock->reader_pc[2], lock->reader_pc[3]);
}

#undef INIT_STUCK
#define INIT_STUCK 100000000

void _do_spin_lock(spinlock_t *lock, char *str)
{
	unsigned long caller, val;
	int stuck = INIT_STUCK;
	int cpu = smp_processor_id();
	int shown = 0;

	GET_CALLER(caller);
again:
	__asm__ __volatile__("ldstub [%1], %0"
			     : "=r" (val)
			     : "r" (&(lock->lock))
			     : "memory");
	membar("#StoreLoad | #StoreStore");
	if (val) {
		while (lock->lock) {
			if (!--stuck) {
				if (shown++ <= 2)
					show(str, lock, caller);
				stuck = INIT_STUCK;
			}
			membar("#LoadLoad");
		}
		goto again;
	}
	lock->owner_pc = ((unsigned int)caller);
	lock->owner_cpu = cpu;
	current->thread.smp_lock_count++;
	current->thread.smp_lock_pc = ((unsigned int)caller);
}

int _spin_trylock(spinlock_t *lock)
{
	unsigned long val, caller;
	int cpu = smp_processor_id();

	GET_CALLER(caller);
	__asm__ __volatile__("ldstub [%1], %0"
			     : "=r" (val)
			     : "r" (&(lock->lock))
			     : "memory");
	membar("#StoreLoad | #StoreStore");
	if (!val) {
		lock->owner_pc = ((unsigned int)caller);
		lock->owner_cpu = cpu;
		current->thread.smp_lock_count++;
		current->thread.smp_lock_pc = ((unsigned int)caller);
	}
	return val == 0;
}

void _do_spin_unlock(spinlock_t *lock)
{
	lock->owner_pc = 0;
	lock->owner_cpu = NO_PROC_ID;
	membar("#StoreStore | #LoadStore");
	lock->lock = 0;
	current->thread.smp_lock_count--;
}

/* Keep INIT_STUCK the same... */

void _do_read_lock (rwlock_t *rw, char *str)
{
	unsigned long caller, val;
	int stuck = INIT_STUCK;
	int cpu = smp_processor_id();
	int shown = 0;

	GET_CALLER(caller);
wlock_again:
	/* Wait for any writer to go away.  */
	while (((long)(rw->lock)) < 0) {
		if (!--stuck) {
			if (shown++ <= 2)
				show_read(str, rw, caller);
			stuck = INIT_STUCK;
		}
		membar("#LoadLoad");
	}
	/* Try once to increment the counter.  */
	__asm__ __volatile__(
"	ldx		[%0], %%g5\n"
"	brlz,a,pn	%%g5, 2f\n"
"	 mov		1, %0\n"
"	add		%%g5, 1, %%g7\n"
"	casx		[%0], %%g5, %%g7\n"
"	sub		%%g5, %%g7, %0\n"
"2:"	: "=r" (val)
	: "0" (&(rw->lock))
	: "g5", "g7", "memory");
	membar("#StoreLoad | #StoreStore");
	if (val)
		goto wlock_again;
	rw->reader_pc[cpu] = ((unsigned int)caller);
	current->thread.smp_lock_count++;
	current->thread.smp_lock_pc = ((unsigned int)caller);
}

void _do_read_unlock (rwlock_t *rw, char *str)
{
	unsigned long caller, val;
	int stuck = INIT_STUCK;
	int cpu = smp_processor_id();
	int shown = 0;

	GET_CALLER(caller);

	/* Drop our identity _first_. */
	rw->reader_pc[cpu] = 0;
	current->thread.smp_lock_count--;
runlock_again:
	/* Spin trying to decrement the counter using casx.  */
	__asm__ __volatile__(
"	ldx	[%0], %%g5\n"
"	sub	%%g5, 1, %%g7\n"
"	casx	[%0], %%g5, %%g7\n"
"	membar	#StoreLoad | #StoreStore\n"
"	sub	%%g5, %%g7, %0\n"
	: "=r" (val)
	: "0" (&(rw->lock))
	: "g5", "g7", "memory");
	if (val) {
		if (!--stuck) {
			if (shown++ <= 2)
				show_read(str, rw, caller);
			stuck = INIT_STUCK;
		}
		goto runlock_again;
	}
}

void _do_write_lock (rwlock_t *rw, char *str)
{
	unsigned long caller, val;
	int stuck = INIT_STUCK;
	int cpu = smp_processor_id();
	int shown = 0;

	GET_CALLER(caller);
wlock_again:
	/* Spin while there is another writer. */
	while (((long)rw->lock) < 0) {
		if (!--stuck) {
			if (shown++ <= 2)
				show_write(str, rw, caller);
			stuck = INIT_STUCK;
		}
		membar("#LoadLoad");
	}

	/* Try to acuire the write bit.  */
	__asm__ __volatile__(
"	mov	1, %%g3\n"
"	sllx	%%g3, 63, %%g3\n"
"	ldx	[%0], %%g5\n"
"	brlz,pn	%%g5, 1f\n"
"	 or	%%g5, %%g3, %%g7\n"
"	casx	[%0], %%g5, %%g7\n"
"	membar	#StoreLoad | #StoreStore\n"
"	ba,pt	%%xcc, 2f\n"
"	 sub	%%g5, %%g7, %0\n"
"1:	mov	1, %0\n"
"2:"	: "=r" (val)
	: "0" (&(rw->lock))
	: "g3", "g5", "g7", "memory");
	if (val) {
		/* We couldn't get the write bit. */
		if (!--stuck) {
			if (shown++ <= 2)
				show_write(str, rw, caller);
			stuck = INIT_STUCK;
		}
		goto wlock_again;
	}
	if ((rw->lock & ((1UL<<63)-1UL)) != 0UL) {
		/* Readers still around, drop the write
		 * lock, spin, and try again.
		 */
		if (!--stuck) {
			if (shown++ <= 2)
				show_write(str, rw, caller);
			stuck = INIT_STUCK;
		}
		__asm__ __volatile__(
"		mov	1, %%g3\n"
"		sllx	%%g3, 63, %%g3\n"
"1:		ldx	[%0], %%g5\n"
"		andn	%%g5, %%g3, %%g7\n"
"		casx	[%0], %%g5, %%g7\n"
"		cmp	%%g5, %%g7\n"
"		bne,pn	%%xcc, 1b\n"
"		 membar	#StoreLoad | #StoreStore"
		: /* no outputs */
		: "r" (&(rw->lock))
		: "g3", "g5", "g7", "cc", "memory");
		while(rw->lock != 0) {
			if (!--stuck) {
				if (shown++ <= 2)
					show_write(str, rw, caller);
				stuck = INIT_STUCK;
			}
			membar("#LoadLoad");
		}
		goto wlock_again;
	}

	/* We have it, say who we are. */
	rw->writer_pc = ((unsigned int)caller);
	rw->writer_cpu = cpu;
	current->thread.smp_lock_count++;
	current->thread.smp_lock_pc = ((unsigned int)caller);
}

void _do_write_unlock(rwlock_t *rw)
{
	unsigned long caller, val;
	int stuck = INIT_STUCK;
	int shown = 0;

	GET_CALLER(caller);

	/* Drop our identity _first_ */
	rw->writer_pc = 0;
	rw->writer_cpu = NO_PROC_ID;
	current->thread.smp_lock_count--;
wlock_again:
	__asm__ __volatile__(
"	mov	1, %%g3\n"
"	sllx	%%g3, 63, %%g3\n"
"	ldx	[%0], %%g5\n"
"	andn	%%g5, %%g3, %%g7\n"
"	casx	[%0], %%g5, %%g7\n"
"	membar	#StoreLoad | #StoreStore\n"
"	sub	%%g5, %%g7, %0\n"
	: "=r" (val)
	: "0" (&(rw->lock))
	: "g3", "g5", "g7", "memory");
	if (val) {
		if (!--stuck) {
			if (shown++ <= 2)
				show_write("write_unlock", rw, caller);
			stuck = INIT_STUCK;
		}
		goto wlock_again;
	}
}

int atomic_dec_and_lock(atomic_t *atomic, spinlock_t *lock)
{
	spin_lock(lock);
	if (atomic_dec_and_test(atomic))
		return 1;
	spin_unlock(lock);
	return 0;
}

#endif /* CONFIG_SMP && CONFIG_DEBUG_SPINLOCK */
