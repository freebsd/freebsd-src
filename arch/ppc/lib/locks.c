/*
 * Locks for smp ppc
 *
 * Written by Cort Dougan (cort@cs.nmt.edu)
 */


#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/io.h>

#if SPINLOCK_DEBUG

#undef INIT_STUCK
#define INIT_STUCK 200000000 /*0xffffffff*/

/*
 * Try to acquire a spinlock.
 * Only does the stwcx. if the load returned 0 - the Programming
 * Environments Manual suggests not doing unnecessary stcwx.'s
 * since they may inhibit forward progress by other CPUs in getting
 * a lock.
 */
static unsigned long __spin_trylock(volatile unsigned long *lock)
{
	unsigned long ret;

	__asm__ __volatile__ ("\n\
1:	lwarx	%0,0,%1\n\
	cmpwi	0,%0,0\n\
	bne	2f\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%2,0,%1\n\
	bne-	1b\n\
	isync\n\
2:"
	: "=&r"(ret)
	: "r"(lock), "r"(1)
	: "cr0", "memory");

	return ret;
}

void _spin_lock(spinlock_t *lock)
{
	int cpu = smp_processor_id();
	unsigned int stuck = INIT_STUCK;
	while (__spin_trylock(&lock->lock)) {
		while ((unsigned volatile long)lock->lock != 0) {
			if (!--stuck) {
				printk("_spin_lock(%p) CPU#%d NIP %p"
				       " holder: cpu %ld pc %08lX\n",
				       lock, cpu, __builtin_return_address(0),
				       lock->owner_cpu,lock->owner_pc);
				stuck = INIT_STUCK;
				/* steal the lock */
				/*xchg_u32((void *)&lock->lock,0);*/
			}
		}
	}
	lock->owner_pc = (unsigned long)__builtin_return_address(0);
	lock->owner_cpu = cpu;
}

int spin_trylock(spinlock_t *lock)
{
	if (__spin_trylock(&lock->lock))
		return 0;
	lock->owner_cpu = smp_processor_id();
	lock->owner_pc = (unsigned long)__builtin_return_address(0);
	return 1;
}

void _spin_unlock(spinlock_t *lp)
{
  	if ( !lp->lock )
		printk("_spin_unlock(%p): no lock cpu %d curr PC %p %s/%d\n",
		       lp, smp_processor_id(), __builtin_return_address(0),
		       current->comm, current->pid);
	if ( lp->owner_cpu != smp_processor_id() )
		printk("_spin_unlock(%p): cpu %d trying clear of cpu %d pc %lx val %lx\n",
		      lp, smp_processor_id(), (int)lp->owner_cpu,
		      lp->owner_pc,lp->lock);
	lp->owner_pc = lp->owner_cpu = 0;
	wmb();
	lp->lock = 0;
}


/*
 * Just like x86, implement read-write locks as a 32-bit counter
 * with the high bit (sign) being the "write" bit.
 * -- Cort
 */
void _read_lock(rwlock_t *rw)
{
	unsigned long stuck = INIT_STUCK;
	int cpu = smp_processor_id();

again:
	/* get our read lock in there */
	atomic_inc((atomic_t *) &(rw)->lock);
	if ( (signed long)((rw)->lock) < 0) /* someone has a write lock */
	{
		/* turn off our read lock */
		atomic_dec((atomic_t *) &(rw)->lock);
		/* wait for the write lock to go away */
		while ((signed long)((rw)->lock) < 0)
		{
			if(!--stuck)
			{
				printk("_read_lock(%p) CPU#%d\n", rw, cpu);
				stuck = INIT_STUCK;
			}
		}
		/* try to get the read lock again */
		goto again;
	}
	wmb();
}

void _read_unlock(rwlock_t *rw)
{
	if ( rw->lock == 0 )
		printk("_read_unlock(): %s/%d (nip %08lX) lock %lx\n",
		       current->comm,current->pid,current->thread.regs->nip,
		      rw->lock);
	wmb();
	atomic_dec((atomic_t *) &(rw)->lock);
}

void _write_lock(rwlock_t *rw)
{
	unsigned long stuck = INIT_STUCK;
	int cpu = smp_processor_id();

again:
	if ( test_and_set_bit(31,&(rw)->lock) ) /* someone has a write lock */
	{
		while ( (rw)->lock & (1<<31) ) /* wait for write lock */
		{
			if(!--stuck)
			{
				printk("write_lock(%p) CPU#%d lock %lx)\n",
				       rw, cpu,rw->lock);
				stuck = INIT_STUCK;
			}
			barrier();
		}
		goto again;
	}

	if ( (rw)->lock & ~(1<<31)) /* someone has a read lock */
	{
		/* clear our write lock and wait for reads to go away */
		clear_bit(31,&(rw)->lock);
		while ( (rw)->lock & ~(1<<31) )
		{
			if(!--stuck)
			{
				printk("write_lock(%p) 2 CPU#%d lock %lx)\n",
				       rw, cpu,rw->lock);
				stuck = INIT_STUCK;
			}
			barrier();
		}
		goto again;
	}
	wmb();
}

void _write_unlock(rwlock_t *rw)
{
	if ( !(rw->lock & (1<<31)) )
		printk("_write_lock(): %s/%d (nip %08lX) lock %lx\n",
		      current->comm,current->pid,current->thread.regs->nip,
		      rw->lock);
	wmb();
	clear_bit(31,&(rw)->lock);
}

#endif
