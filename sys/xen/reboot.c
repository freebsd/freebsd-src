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
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/gnttab.h>
#include <xen/xen_intr.h>
#include <xen/xenbus/xenbusvar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#ifdef XENHVM

#include <dev/xen/xenpci/xenpcivar.h>

#else

static void xen_suspend(void);

#endif

static void 
shutdown_handler(struct xenbus_watch *watch,
		 const char **vec, unsigned int len)
{
	char *str;
	struct xenbus_transaction xbt;
	int error, howto;
	
	howto = 0;

 again:
	error = xenbus_transaction_start(&xbt);
	if (error)
		return;

	error = xenbus_read(xbt, "control", "shutdown", NULL, (void **) &str);

	/* Ignore read errors and empty reads. */
	if (error || strlen(str) == 0) {
		xenbus_transaction_end(xbt, 1);
		return;
	}

	xenbus_write(xbt, "control", "shutdown", "");

	error = xenbus_transaction_end(xbt, 0);
	if (error == EAGAIN) {
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

	if (howto == -1) {
		xen_suspend();
		goto done;
	}

	shutdown_nice(howto);
 done:
	free(str, M_DEVBUF);
}

#ifndef XENHVM

/*
 * In HV mode, we let acpi take care of halts and reboots.
 */

static void
xen_shutdown_final(void *arg, int howto)
{

	if (howto & (RB_HALT | RB_POWEROFF))
		HYPERVISOR_shutdown(SHUTDOWN_poweroff);
	else
		HYPERVISOR_shutdown(SHUTDOWN_reboot);
}

#endif

static struct xenbus_watch shutdown_watch = {
	.node = "control/shutdown",
	.callback = shutdown_handler
};

static void
setup_shutdown_watcher(void *unused)
{

	if (register_xenbus_watch(&shutdown_watch))
		printf("Failed to set shutdown watcher\n");
#ifndef XENHVM
	EVENTHANDLER_REGISTER(shutdown_final, xen_shutdown_final, NULL,
	    SHUTDOWN_PRI_LAST);
#endif
}

SYSINIT(shutdown, SI_SUB_PSEUDO, SI_ORDER_ANY, setup_shutdown_watcher, NULL);

#ifndef XENHVM

extern void xencons_suspend(void);
extern void xencons_resume(void);

static void 
xen_suspend()
{
	int i, j, k, fpp;
	unsigned long max_pfn, start_info_mfn;

#ifdef SMP
	cpumask_t map;
	/*
	 * Bind us to CPU 0 and stop any other VCPUs.
	 */
	mtx_lock_spin(&sched_lock);
	sched_bind(curthread, 0);
	mtx_unlock_spin(&sched_lock);
	KASSERT(PCPU_GET(cpuid) == 0, ("xen_suspend: not running on cpu 0"));

	map = PCPU_GET(other_cpus) & ~stopped_cpus;
	if (map)
		stop_cpus(map);
#endif

	if (DEVICE_SUSPEND(root_bus) != 0) {
		printf("xen_suspend: device_suspend failed\n");
		if (map)
			restart_cpus(map);
		return;
	}

	local_irq_disable();

	xencons_suspend();
	gnttab_suspend();

	max_pfn = HYPERVISOR_shared_info->arch.max_pfn;

	void *shared_info = HYPERVISOR_shared_info;
	HYPERVISOR_shared_info = NULL;
	pmap_kremove((vm_offset_t) shared_info);
	PT_UPDATES_FLUSH();

	xen_start_info->store_mfn = MFNTOPFN(xen_start_info->store_mfn);
	xen_start_info->console.domU.mfn = MFNTOPFN(xen_start_info->console.domU.mfn);

	/*
	 * We'll stop somewhere inside this hypercall. When it returns,
	 * we'll start resuming after the restore.
	 */
	start_info_mfn = VTOMFN(xen_start_info);
	pmap_suspend();
	HYPERVISOR_suspend(start_info_mfn);
	pmap_resume();

	pmap_kenter_ma((vm_offset_t) shared_info, xen_start_info->shared_info);
	HYPERVISOR_shared_info = shared_info;

	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list =
		VTOMFN(xen_pfn_to_mfn_frame_list_list);
  
	fpp = PAGE_SIZE/sizeof(unsigned long);
	for (i = 0, j = 0, k = -1; i < max_pfn; i += fpp, j++) {
		if ((j % fpp) == 0) {
			k++;
			xen_pfn_to_mfn_frame_list_list[k] = 
				VTOMFN(xen_pfn_to_mfn_frame_list[k]);
			j = 0;
		}
		xen_pfn_to_mfn_frame_list[k][j] = 
			VTOMFN(&xen_phys_machine[i]);
	}
	HYPERVISOR_shared_info->arch.max_pfn = max_pfn;

	gnttab_resume();
	irq_resume();
	local_irq_enable();
	xencons_resume();

#ifdef CONFIG_SMP
	for_each_cpu(i)
		vcpu_prepare(i);

#endif
	/* 
	 * Only resume xenbus /after/ we've prepared our VCPUs; otherwise
	 * the VCPU hotplug callback can race with our vcpu_prepare
	 */
	DEVICE_RESUME(root_bus);

#ifdef SMP
	sched_unbind(curthread);
	if (map)
		restart_cpus(map);
#endif
}

#endif
