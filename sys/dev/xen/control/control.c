/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Justin T. Gibbs, Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

/*-
 * HVM suspend/resume support:
 *
 * Copyright (c) 2008 Citrix Systems, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
/**
 * \file control.c
 *
 * \brief Device driver to repond to control domain events that impact
 *        this VM.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/rman.h>
#include <sys/sched.h>
#include <sys/taskqueue.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/eventhandler.h>
#include <sys/timetc.h>

#include <geom/geom.h>

#include <machine/_inttypes.h>
#if defined(__amd64__) || defined(__i386__)
#include <machine/intr_machdep.h>

#include <x86/apicvar.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <xen/xen-os.h>
#include <xen/blkif.h>
#include <xen/evtchn.h>
#include <xen/gnttab.h>
#include <xen/xen_intr.h>

#include <xen/hvm.h>

#include <contrib/xen/event_channel.h>
#include <contrib/xen/grant_table.h>

#include <xen/xenbus/xenbusvar.h>

bool xen_suspend_cancelled;
/*--------------------------- Forward Declarations --------------------------*/
/** Function signature for shutdown event handlers. */
typedef	void (xctrl_shutdown_handler_t)(void);

static xctrl_shutdown_handler_t xctrl_poweroff;
static xctrl_shutdown_handler_t xctrl_reboot;
static xctrl_shutdown_handler_t xctrl_suspend;
static xctrl_shutdown_handler_t xctrl_crash;

/*-------------------------- Private Data Structures -------------------------*/
/** Element type for lookup table of event name to handler. */
struct xctrl_shutdown_reason {
	const char		 *name;
	xctrl_shutdown_handler_t *handler;
};

/** Lookup table for shutdown event name to handler. */
static const struct xctrl_shutdown_reason xctrl_shutdown_reasons[] = {
	{ "poweroff", xctrl_poweroff },
	{ "reboot",   xctrl_reboot   },
	{ "suspend",  xctrl_suspend  },
	{ "crash",    xctrl_crash    },
	{ "halt",     xctrl_poweroff },
};

struct xctrl_softc {
	struct xs_watch    xctrl_watch;	
};

/*------------------------------ Event Handlers ------------------------------*/
static void
xctrl_poweroff(void)
{
	shutdown_nice(RB_POWEROFF|RB_HALT);
}

static void
xctrl_reboot(void)
{
	shutdown_nice(0);
}

#if !defined(__amd64__) && !defined(__i386__)
static void
xctrl_suspend(void)
{
	printf("WARNING: xen/control: Suspend not supported!\n");
}
#else /* __amd64__ || __i386__ */
static void
xctrl_suspend(void)
{
#ifdef SMP
	cpuset_t cpu_suspend_map;
#endif

	EVENTHANDLER_INVOKE(power_suspend_early);
	xs_lock();
	stop_all_proc();
	xs_unlock();
	suspend_all_fs();
	EVENTHANDLER_INVOKE(power_suspend);

#ifdef EARLY_AP_STARTUP
	MPASS(mp_ncpus == 1 || smp_started);
	thread_lock(curthread);
	sched_bind(curthread, 0);
	thread_unlock(curthread);
#else
	if (smp_started) {
		thread_lock(curthread);
		sched_bind(curthread, 0);
		thread_unlock(curthread);
	}
#endif
	KASSERT((PCPU_GET(cpuid) == 0), ("Not running on CPU#0"));

	/*
	 * Be sure to hold Giant across DEVICE_SUSPEND/RESUME.
	 */
	bus_topo_lock();
	if (DEVICE_SUSPEND(root_bus) != 0) {
		bus_topo_unlock();
		printf("%s: device_suspend failed\n", __func__);
		return;
	}

#ifdef SMP
#ifdef EARLY_AP_STARTUP
	/*
	 * Suspend other CPUs. This prevents IPIs while we
	 * are resuming, and will allow us to reset per-cpu
	 * vcpu_info on resume.
	 */
	cpu_suspend_map = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &cpu_suspend_map);
	if (!CPU_EMPTY(&cpu_suspend_map))
		suspend_cpus(cpu_suspend_map);
#else
	CPU_ZERO(&cpu_suspend_map);	/* silence gcc */
	if (smp_started) {
		/*
		 * Suspend other CPUs. This prevents IPIs while we
		 * are resuming, and will allow us to reset per-cpu
		 * vcpu_info on resume.
		 */
		cpu_suspend_map = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &cpu_suspend_map);
		if (!CPU_EMPTY(&cpu_suspend_map))
			suspend_cpus(cpu_suspend_map);
	}
#endif
#endif

	/*
	 * Prevent any races with evtchn_interrupt() handler.
	 */
	disable_intr();
	intr_suspend();
	xen_hvm_suspend();

	xen_suspend_cancelled = !!HYPERVISOR_suspend(0);

	if (!xen_suspend_cancelled) {
		xen_hvm_resume(false);
	}
	intr_resume(xen_suspend_cancelled != 0);
	enable_intr();

	/*
	 * Reset grant table info.
	 */
	if (!xen_suspend_cancelled) {
		gnttab_resume(NULL);
	}

#ifdef SMP
	if (!CPU_EMPTY(&cpu_suspend_map)) {
		/*
		 * Now that event channels have been initialized,
		 * resume CPUs.
		 */
		resume_cpus(cpu_suspend_map);
#if defined(__amd64__) || defined(__i386__)
		/* Send an IPI_BITMAP in case there are pending bitmap IPIs. */
		lapic_ipi_vectored(IPI_BITMAP_VECTOR, APIC_IPI_DEST_ALL);
#endif
	}
#endif

	/*
	 * FreeBSD really needs to add DEVICE_SUSPEND_CANCEL or
	 * similar.
	 */
	DEVICE_RESUME(root_bus);
	bus_topo_unlock();

	/*
	 * Warm up timecounter again and reset system clock.
	 */
	timecounter->tc_get_timecount(timecounter);
	inittodr(time_second);

#ifdef EARLY_AP_STARTUP
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);
#else
	if (smp_started) {
		thread_lock(curthread);
		sched_unbind(curthread);
		thread_unlock(curthread);
	}
#endif

	resume_all_fs();
	resume_all_proc();

	EVENTHANDLER_INVOKE(power_resume);

	if (bootverbose)
		printf("System resumed after suspension\n");

}
#endif /* __amd64__ || __i386__ */

static void
xctrl_crash(void)
{
	panic("Xen directed crash");
}

static void
xctrl_shutdown_final(void *arg, int howto)
{
	/*
	 * Inform the hypervisor that shutdown is complete, and specify the
	 * nature of the shutdown. RB_HALT is not handled by this function.
	 */
	if (KERNEL_PANICKED())
		HYPERVISOR_shutdown(SHUTDOWN_crash);
	else if ((howto & RB_POWEROFF) != 0)
		HYPERVISOR_shutdown(SHUTDOWN_poweroff);
	else if ((howto & RB_HALT) == 0)
		/* RB_POWERCYCLE or regular reset. */
		HYPERVISOR_shutdown(SHUTDOWN_reboot);
}

/*------------------------------ Event Reception -----------------------------*/
static void
xctrl_on_watch_event(struct xs_watch *watch, const char **vec, unsigned int len)
{
	const struct xctrl_shutdown_reason *reason;
	const struct xctrl_shutdown_reason *last_reason;
	char *result;
	int   error;
	int   result_len;

	error = xs_read(XST_NIL, "control", "shutdown",
			&result_len, (void **)&result);
	if (error != 0 || result_len == 0)
		return;

	/* Acknowledge the request by writing back an empty string. */
	error = xs_write(XST_NIL, "control", "shutdown", "");
	if (error != 0)
		printf("unable to ack shutdown request, proceeding anyway\n");

	reason = xctrl_shutdown_reasons;
	last_reason = reason + nitems(xctrl_shutdown_reasons);
	while (reason < last_reason) {
		if (!strcmp(result, reason->name)) {
			reason->handler();
			break;
		}
		reason++;
	}

	free(result, M_XENSTORE);
}

/*------------------ Private Device Attachment Functions  --------------------*/

static void
notify_support(void)
{
	/*
	 * Notify kernel is ready to handle "control/shutdown" events.  Ignore
	 * errors if the nodes haven't been created by the toolstack, as the
	 * parent "control" directory should be read-only for the guest.
	 */
	xs_write(XST_NIL, "control", "feature-poweroff", "1");
	xs_write(XST_NIL, "control", "feature-reboot", "1");
	xs_write(XST_NIL, "control", "feature-suspend", "1");
}

/**
 * \brief Identify instances of this device type in the system.
 *
 * \param driver  The driver performing this identify action.
 * \param parent  The NewBus parent device for any devices this method adds.
 */
static void
xctrl_identify(driver_t *driver, device_t parent)
{
	/*
	 * A single device instance for our driver is always present
	 * in a system operating under Xen.
	 */
	BUS_ADD_CHILD(parent, 0, driver->name, 0);
}

/**
 * \brief Probe for the existence of the Xen Control device
 *
 * \param dev  NewBus device_t for this Xen control instance.
 *
 * \return  Always returns 0 indicating success.
 */
static int 
xctrl_probe(device_t dev)
{
	device_set_desc(dev, "Xen Control Device");

	return (BUS_PROBE_NOWILDCARD);
}

/**
 * \brief Attach the Xen control device.
 *
 * \param dev  NewBus device_t for this Xen control instance.
 *
 * \return  On success, 0. Otherwise an errno value indicating the
 *          type of failure.
 */
static int
xctrl_attach(device_t dev)
{
	struct xctrl_softc *xctrl;

	xctrl = device_get_softc(dev);

	/* Activate watch */
	xctrl->xctrl_watch.node = "control/shutdown";
	xctrl->xctrl_watch.callback = xctrl_on_watch_event;
	xctrl->xctrl_watch.callback_data = (uintptr_t)xctrl;
	/*
	 * We don't care about the path updated, just about the value changes
	 * on that single node, hence there's no need to queue more that one
	 * event.
	 */
	xctrl->xctrl_watch.max_pending = 1;
	xs_register_watch(&xctrl->xctrl_watch);

	EVENTHANDLER_REGISTER(shutdown_final, xctrl_shutdown_final, NULL,
	    SHUTDOWN_PRI_LAST);

	notify_support();

	return (0);
}

/**
 * \brief Detach the Xen control device.
 *
 * \param dev  NewBus device_t for this Xen control device instance.
 *
 * \return  On success, 0. Otherwise an errno value indicating the
 *          type of failure.
 */
static int
xctrl_detach(device_t dev)
{
	struct xctrl_softc *xctrl;

	xctrl = device_get_softc(dev);

	/* Release watch */
	xs_unregister_watch(&xctrl->xctrl_watch);

	return (0);
}

static int
xctrl_resume(device_t dev)
{
	notify_support();

	return (0);
}

/*-------------------- Private Device Attachment Data  -----------------------*/
static device_method_t xctrl_methods[] = { 
	/* Device interface */ 
	DEVMETHOD(device_identify,	xctrl_identify),
	DEVMETHOD(device_probe,         xctrl_probe), 
	DEVMETHOD(device_attach,        xctrl_attach), 
	DEVMETHOD(device_detach,        xctrl_detach), 
	DEVMETHOD(device_resume,        xctrl_resume),

	DEVMETHOD_END
}; 

DEFINE_CLASS_0(xctrl, xctrl_driver, xctrl_methods, sizeof(struct xctrl_softc));

DRIVER_MODULE(xctrl, xenstore, xctrl_driver, NULL, NULL);
