/*
 *
 * linux/lib/brlock.c
 *
 * 'Big Reader' read-write spinlocks.  See linux/brlock.h for details.
 *
 * Copyright 2000, Ingo Molnar <mingo@redhat.com>
 * Copyright 2000, David S. Miller <davem@redhat.com>
 */

#include <linux/config.h>

#ifdef CONFIG_SMP

#include <linux/sched.h>
#include <linux/brlock.h>

#ifdef __BRLOCK_USE_ATOMICS

brlock_read_lock_t __brlock_array[NR_CPUS][__BR_IDX_MAX] =
   { [0 ... NR_CPUS-1] = { [0 ... __BR_IDX_MAX-1] = RW_LOCK_UNLOCKED } };

void __br_write_lock (enum brlock_indices idx)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		write_lock(&__brlock_array[cpu_logical_map(i)][idx]);
}

void __br_write_unlock (enum brlock_indices idx)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		write_unlock(&__brlock_array[cpu_logical_map(i)][idx]);
}

#else /* ! __BRLOCK_USE_ATOMICS */

brlock_read_lock_t __brlock_array[NR_CPUS][__BR_IDX_MAX] =
   { [0 ... NR_CPUS-1] = { [0 ... __BR_IDX_MAX-1] = 0 } };

struct br_wrlock __br_write_locks[__BR_IDX_MAX] =
   { [0 ... __BR_IDX_MAX-1] = { SPIN_LOCK_UNLOCKED } };

void __br_write_lock (enum brlock_indices idx)
{
	int i;

again:
	spin_lock(&__br_write_locks[idx].lock);
	for (i = 0; i < smp_num_cpus; i++)
		if (__brlock_array[cpu_logical_map(i)][idx] != 0) {
			spin_unlock(&__br_write_locks[idx].lock);
			barrier();
			cpu_relax();
			goto again;
		}
}

void __br_write_unlock (enum brlock_indices idx)
{
	spin_unlock(&__br_write_locks[idx].lock);
}

#endif /* __BRLOCK_USE_ATOMICS */

#endif /* CONFIG_SMP */
