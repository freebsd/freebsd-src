/*
 * debugging spinlocks for parisc
 * 
 * Adapted from the ppc version
 */


#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/io.h>

#ifdef CONFIG_DEBUG_SPINLOCK

#undef INIT_STUCK
#define INIT_STUCK 200000000 /*0xffffffff*/

#define __spin_trylock(x) (__ldcw(&(x)->lock) != 0)

void spin_lock(spinlock_t *lock)
{
	int cpu = smp_processor_id();
	unsigned int stuck = INIT_STUCK;
	while (!__spin_trylock(lock)) {
		while ((unsigned volatile long)lock->lock == 0) {
			if (!--stuck) {
				printk("spin_lock(%p) CPU#%d NIP %p"
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
	if (!__spin_trylock(lock))
		return 0;
	lock->owner_cpu = smp_processor_id(); 
	lock->owner_pc = (unsigned long)__builtin_return_address(0);
	return 1;
}

void spin_unlock(spinlock_t *lp)
{
  	if ( lp->lock )
		printk("spin_unlock(%p): no lock cpu %d curr PC %p %s/%d\n",
		       lp, smp_processor_id(), __builtin_return_address(0),
		       current->comm, current->pid);
	if ( lp->owner_cpu != smp_processor_id() )
		printk("spin_unlock(%p): cpu %d trying clear of cpu %d pc %lx val %x\n",
		      lp, smp_processor_id(), (int)lp->owner_cpu,
		      lp->owner_pc,lp->lock);
	lp->owner_pc = lp->owner_cpu = 0;
	wmb();
	lp->lock = 1;
}

#endif
