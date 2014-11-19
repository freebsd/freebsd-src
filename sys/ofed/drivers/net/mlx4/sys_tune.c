/*
 * Copyright (c) 2010, 2014 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/sched.h>
#include <linux/mutex.h>
#include <asm/atomic.h>

#include "mlx4.h"

#if defined(CONFIG_X86) && defined(CONFIG_APM_MODULE)

/* Each CPU is put into a group.  In most cases, the group number is
 * equal to the CPU number of one of the CPUs in the group.  The
 * exception is group NR_CPUS which is the default group.  This is
 * protected by sys_tune_startup_mutex. */
DEFINE_PER_CPU(int, idle_cpu_group) = NR_CPUS;

/* For each group, a count of the number of CPUs in the group which
 * are known to be busy.  A busy CPU might be running the busy loop
 * below or general kernel code.  The count is decremented on entry to
 * the old pm_idle handler and incremented on exit.  The aim is to
 * avoid the count going to zero or negative.  This situation can
 * occur temporarily during module unload or CPU hot-plug but
 * normality will be restored when the affected CPUs next exit the
 * idle loop. */
static atomic_t busy_cpu_count[NR_CPUS+1];

/* A workqueue item to be executed to cause the CPU to exit from the
 * idle loop. */
DEFINE_PER_CPU(struct work_struct, sys_tune_cpu_work);

#define sys_tune_set_state(CPU,STATE) \
	do { } while(0)


/* A mutex to protect most of the module datastructures. */
static DEFINE_MUTEX(sys_tune_startup_mutex);

/* The old pm_idle handler. */
static void (*old_pm_idle)(void) = NULL;

static void sys_tune_pm_idle(void)
{
	atomic_t *busy_cpus_ptr;
	int busy_cpus;
	int cpu = smp_processor_id();

	busy_cpus_ptr = &(busy_cpu_count[per_cpu(idle_cpu_group, cpu)]);

	sys_tune_set_state(cpu, 2);

	local_irq_enable();
	while (!need_resched()) {
		busy_cpus = atomic_read(busy_cpus_ptr);

		/* If other CPUs in this group are busy then let this
		 * CPU go idle.  We mustn't let the number of busy
		 * CPUs drop below 1. */
		if ( busy_cpus > 1 &&
		     old_pm_idle != NULL &&
		     ( atomic_cmpxchg(busy_cpus_ptr, busy_cpus,
				      busy_cpus-1) == busy_cpus ) ) {
			local_irq_disable();
			sys_tune_set_state(cpu, 3);
			/* This check might not be necessary, but it
			 * seems safest to include it because there
			 * might be a kernel version which requires
			 * it. */
			if (need_resched())
				local_irq_enable();
			else
				old_pm_idle();
			/* This CPU is busy again. */
			sys_tune_set_state(cpu, 1);
			atomic_add(1, busy_cpus_ptr);
			return;
		}

		cpu_relax();
	}
	sys_tune_set_state(cpu, 0);
}


void sys_tune_work_func(struct work_struct *work)
{
	/* Do nothing.  Since this function is running in process
	 * context, the idle thread isn't running on this CPU. */
}


#ifdef CONFIG_SMP
static void sys_tune_smp_call(void *info)
{
	schedule_work(&get_cpu_var(sys_tune_cpu_work));
	put_cpu_var(sys_tune_cpu_work);
}
#endif


#ifdef CONFIG_SMP
static void sys_tune_refresh(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
        on_each_cpu(&sys_tune_smp_call, NULL, 0, 1);
#else
        on_each_cpu(&sys_tune_smp_call, NULL, 1);
#endif
}
#else
static void sys_tune_refresh(void)
{
	/* The current thread is executing on the one and only CPU so
	 * the idle thread isn't running. */
}
#endif



static int sys_tune_cpu_group(int cpu)
{
#ifdef CONFIG_SMP
	const cpumask_t *mask;
	int other_cpu;
	int group;

#if defined(topology_thread_cpumask) && defined(ST_HAVE_EXPORTED_CPU_SIBLING_MAP)
	/* Keep one hyperthread busy per core. */
	mask = topology_thread_cpumask(cpu);
#else
	return cpu;
#endif
	for_each_cpu_mask(cpu, *(mask))	{
		group = per_cpu(idle_cpu_group, other_cpu);
		if (group != NR_CPUS)
			return group;
	}
#endif

	return cpu;
}


static void sys_tune_add_cpu(int cpu)
{
	int group;

	/* Do nothing if this CPU has already been added. */
	if (per_cpu(idle_cpu_group, cpu) != NR_CPUS)
		return;

	group = sys_tune_cpu_group(cpu);
	per_cpu(idle_cpu_group, cpu) = group;
	atomic_inc(&(busy_cpu_count[group]));

}

static void sys_tune_del_cpu(int cpu)
{

	int group;

	if (per_cpu(idle_cpu_group, cpu) == NR_CPUS)
		return;

	group = per_cpu(idle_cpu_group, cpu);
	/* If the CPU was busy, this can cause the count to drop to
	 * zero.  To rectify this, we need to cause one of the other
	 * CPUs in the group to exit the idle loop.  If the CPU was
	 * not busy then this causes the contribution for this CPU to
	 * go to -1 which can cause the overall count to drop to zero
	 * or go negative.  To rectify this situation we need to cause
	 * this CPU to exit the idle loop. */
	atomic_dec(&(busy_cpu_count[group]));
	per_cpu(idle_cpu_group, cpu) = NR_CPUS;

}


static int sys_tune_cpu_notify(struct notifier_block *self,
			       unsigned long action, void *hcpu)
{
	int cpu = (long)hcpu;
	
	switch(action) {
#ifdef CPU_ONLINE_FROZEN
	case CPU_ONLINE_FROZEN:
#endif
	case CPU_ONLINE:
		mutex_lock(&sys_tune_startup_mutex);
		sys_tune_add_cpu(cpu);
		mutex_unlock(&sys_tune_startup_mutex);
		/* The CPU might have already entered the idle loop in
		 * the wrong group.  Make sure it exits the idle loop
		 * so that it picks up the correct group. */
		sys_tune_refresh();
		break;

#ifdef CPU_DEAD_FROZEN
	case CPU_DEAD_FROZEN:
#endif
	case CPU_DEAD:
		mutex_lock(&sys_tune_startup_mutex);
		sys_tune_del_cpu(cpu);
		mutex_unlock(&sys_tune_startup_mutex);
		/* The deleted CPU may have been the only busy CPU in
		 * the group.  Make sure one of the other CPUs in the
		 * group exits the idle loop. */
		sys_tune_refresh();
		break;
	}
	return NOTIFY_OK;
}


static struct notifier_block sys_tune_cpu_nb = {
	.notifier_call = sys_tune_cpu_notify,
};


static void sys_tune_ensure_init(void)
{
	BUG_ON (old_pm_idle != NULL);

	/* Atomically update pm_idle to &sys_tune_pm_idle.  The old value
	 * is stored in old_pm_idle before installing the new
	 * handler. */
	do {
		old_pm_idle = pm_idle;
	} while (cmpxchg(&pm_idle, old_pm_idle, &sys_tune_pm_idle) !=
		 old_pm_idle);
}
#endif

void sys_tune_fini(void)
{
#if defined(CONFIG_X86) && defined(CONFIG_APM_MODULE)
	void (*old)(void);
	int cpu;

	unregister_cpu_notifier(&sys_tune_cpu_nb);

	mutex_lock(&sys_tune_startup_mutex);


	old = cmpxchg(&pm_idle, &sys_tune_pm_idle, old_pm_idle);

	for_each_online_cpu(cpu)
		sys_tune_del_cpu(cpu);

	mutex_unlock(&sys_tune_startup_mutex);
	
	/* Our handler may still be executing on other CPUs.
	 * Schedule this thread on all CPUs to make sure all
	 * idle threads get interrupted. */
	sys_tune_refresh();

	/* Make sure the work item has finished executing on all CPUs.
	 * This in turn ensures that all idle threads have been
	 * interrupted. */
	flush_scheduled_work();
#endif /* CONFIG_X86 */
}

void sys_tune_init(void)
{
#if defined(CONFIG_X86) && defined(CONFIG_APM_MODULE)
	int cpu;

	for_each_possible_cpu(cpu) {
		INIT_WORK(&per_cpu(sys_tune_cpu_work, cpu),
			  sys_tune_work_func);
	}

	/* Start by registering the handler to ensure we don't miss
	 * any updates. */
	register_cpu_notifier(&sys_tune_cpu_nb);

	mutex_lock(&sys_tune_startup_mutex);

	for_each_online_cpu(cpu)
		sys_tune_add_cpu(cpu);

	sys_tune_ensure_init();


	mutex_unlock(&sys_tune_startup_mutex);

	/* Ensure our idle handler starts to run. */
	sys_tune_refresh();
#endif
}

