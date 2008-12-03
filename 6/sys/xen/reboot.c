/*
 *
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004-2006,2008 Kip Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian Limpach.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/reboot.h>

#include <xen/xenbus/xenbusvar.h>

static void 
shutdown_handler(struct xenbus_watch *watch,
		 const char **vec, unsigned int len)
{
	char *str;
	struct xenbus_transaction xbt;
	int err, howto;
	
	howto = 0;

 again:
	err = xenbus_transaction_start(&xbt);
	if (err)
		return;
	str = (char *)xenbus_read(xbt, "control", "shutdown", NULL);
	/* Ignore read errors and empty reads. */
	if (XENBUS_IS_ERR_READ(str)) {
		xenbus_transaction_end(xbt, 1);
		return;
	}

	xenbus_write(xbt, "control", "shutdown", "");

	err = xenbus_transaction_end(xbt, 0);
	if (err == EAGAIN) {
		free(str, M_DEVBUF);
		goto again;
	}

	if (strcmp(str, "reboot") == 0)
		howto = 0;
	else if (strcmp(str, "poweroff") == 0)
		howto |= (RB_POWEROFF | RB_HALT);
	else if (strcmp(str, "halt") == 0)
#ifdef XENHVM
		/*
		 * We rely on acpi powerdown to halt the VM.
		 */
		howto |= (RB_POWEROFF | RB_HALT);
#else
		howto |= RB_HALT;
#endif
	else if (strcmp(str, "suspend") == 0)
		howto = -1;
	else {
		printf("Ignoring shutdown request: %s\n", str);
		goto done;
	}
#ifdef notyet
	if (howto == -1) {
		do_suspend(NULL);
		goto done;
	}
#else 
	if (howto == -1) {
		printf("suspend not currently supported\n");
		goto done;
	}
#endif
	shutdown_nice(howto);
 done:
	free(str, M_DEVBUF);
}

static struct xenbus_watch shutdown_watch = {
	.node = "control/shutdown",
	.callback = shutdown_handler
};

static void
setup_shutdown_watcher(void *unused)
{

	if (register_xenbus_watch(&shutdown_watch))
		printf("Failed to set shutdown watcher\n");
}

SYSINIT(shutdown, SI_SUB_PSEUDO, SI_ORDER_ANY, setup_shutdown_watcher, NULL);

#ifdef notyet

static void 
xen_suspend(void *ignore)
{
	int i, j, k, fpp;

	extern void time_resume(void);
	extern unsigned long max_pfn;
	extern unsigned long *pfn_to_mfn_frame_list_list;
	extern unsigned long *pfn_to_mfn_frame_list[];

#ifdef CONFIG_SMP
#error "do_suspend must be run cpu 0 - need to create separate thread"
	cpumask_t prev_online_cpus;
	int vcpu_prepare(int vcpu);
#endif

	int err = 0;

	PANIC_IF(smp_processor_id() != 0);

#if defined(CONFIG_SMP) && !defined(CONFIG_HOTPLUG_CPU)
	if (num_online_cpus() > 1) {
		printk(KERN_WARNING "Can't suspend SMP guests "
		       "without CONFIG_HOTPLUG_CPU\n");
		return -EOPNOTSUPP;
	}
#endif

	xenbus_suspend();

#ifdef CONFIG_SMP
	lock_cpu_hotplug();
	/*
	 * Take all other CPUs offline. We hold the hotplug semaphore to
	 * avoid other processes bringing up CPUs under our feet.
	 */
	cpus_clear(prev_online_cpus);
	while (num_online_cpus() > 1) {
		for_each_online_cpu(i) {
			if (i == 0)
				continue;
			unlock_cpu_hotplug();
			err = cpu_down(i);
			lock_cpu_hotplug();
			if (err != 0) {
				printk(KERN_CRIT "Failed to take all CPUs "
				       "down: %d.\n", err);
				goto out_reenable_cpus;
			}
			cpu_set(i, prev_online_cpus);
		}
	}
#endif /* CONFIG_SMP */

	preempt_disable();


	__cli();
	preempt_enable();
#ifdef SMP
	unlock_cpu_hotplug();
#endif
	gnttab_suspend();

	pmap_kremove(HYPERVISOR_shared_info);

	xen_start_info->store_mfn = mfn_to_pfn(xen_start_info->store_mfn);
	xen_start_info->console.domU.mfn = mfn_to_pfn(xen_start_info->console.domU.mfn);

	/*
	 * We'll stop somewhere inside this hypercall. When it returns,
	 * we'll start resuming after the restore.
	 */
	HYPERVISOR_suspend(VTOMFN(xen_start_info));

	pmap_kenter_ma(HYPERVISOR_shared_info, xen_start_info->shared_info);
	set_fixmap(FIX_SHARED_INFO, xen_start_info->shared_info);

#if 0
	memset(empty_zero_page, 0, PAGE_SIZE);
#endif     
	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list =
		VTOMFN(pfn_to_mfn_frame_list_list);
  
	fpp = PAGE_SIZE/sizeof(unsigned long);
	for (i = 0, j = 0, k = -1; i < max_pfn; i += fpp, j++) {
		if ((j % fpp) == 0) {
			k++;
			pfn_to_mfn_frame_list_list[k] = 
				VTOMFN(pfn_to_mfn_frame_list[k]);
			j = 0;
		}
		pfn_to_mfn_frame_list[k][j] = 
			VTOMFN(&phys_to_machine_mapping[i]);
	}
	HYPERVISOR_shared_info->arch.max_pfn = max_pfn;

	gnttab_resume();

	irq_resume();

	time_resume();

	__sti();

	xencons_resume();

#ifdef CONFIG_SMP
	for_each_cpu(i)
		vcpu_prepare(i);

#endif
	/* 
	 * Only resume xenbus /after/ we've prepared our VCPUs; otherwise
	 * the VCPU hotplug callback can race with our vcpu_prepare
	 */
	xenbus_resume();

#ifdef CONFIG_SMP
 out_reenable_cpus:
	for_each_cpu_mask(i, prev_online_cpus) {
		j = cpu_up(i);
		if ((j != 0) && !cpu_online(i)) {
			printk(KERN_CRIT "Failed to bring cpu "
			       "%d back up (%d).\n",
			       i, j);
			err = j;
		}
	}
#endif
	return err;
}

#endif /* notyet */
