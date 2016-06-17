#ifndef __CRIS_SMPLOCK_H
#define __CRIS_SMPLOCK_H

#include <linux/config.h>

#ifdef CONFIG_SMP

#error "SMP is not supported for CRIS"

/*
 *	Locking the kernel 
 */
 
extern __inline void lock_kernel(void)
{
	unsigned long flags;
	int proc = smp_processor_id();

	save_flags(flags);
	cli();
	/* set_bit works atomic in SMP machines */
	while(set_bit(0, (void *)&kernel_flag)) 
	{
		/*
		 *	We just start another level if we have the lock 
		 */
		if (proc == active_kernel_processor)
			break;
		do 
		{
#ifdef __SMP_PROF__		
			smp_spins[smp_processor_id()]++;
#endif			
			/*
			 *	Doing test_bit here doesn't lock the bus 
			 */
			if (test_bit(proc, (void *)&smp_invalidate_needed))
				if (clear_bit(proc, (void *)&smp_invalidate_needed))
					local_flush_tlb();
		}
		while(test_bit(0, (void *)&kernel_flag));
	}
	/* 
	 *	We got the lock, so tell the world we are here and increment
	 *	the level counter 
	 */
	active_kernel_processor = proc;
	kernel_counter++;
	restore_flags(flags);
}

extern __inline void unlock_kernel(void)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	/*
	 *	If it's the last level we have in the kernel, then
	 *	free the lock 
	 */
	if (kernel_counter == 0)
		panic("Kernel counter wrong.\n"); /* FIXME: Why is kernel_counter sometimes 0 here? */
	
	if(! --kernel_counter) 
	{
		active_kernel_processor = NO_PROC_ID;
		clear_bit(0, (void *)&kernel_flag);
	}
	restore_flags(flags);
}

#endif
#endif
