/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * VM Bus Driver Implementation
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/rtprio.h>
#include <sys/interrupt.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <sys/pcpu.h>
#include <machine/apicvar.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#include <contrib/dev/acpica/include/acpi.h>
#include "acpi_if.h"

struct vmbus_softc	*vmbus_sc;

static device_t vmbus_devp;
static int vmbus_inited;
static hv_setup_args setup_args; /* only CPU 0 supported at this time */

static char *vmbus_ids[] = { "VMBUS", NULL };

static void
vmbus_msg_task(void *arg __unused, int pending __unused)
{
	hv_vmbus_message *msg;

	msg = ((hv_vmbus_message *)hv_vmbus_g_context.syn_ic_msg_page[curcpu]) +
	    HV_VMBUS_MESSAGE_SINT;
	for (;;) {
		const hv_vmbus_channel_msg_table_entry *entry;
		hv_vmbus_channel_msg_header *hdr;
		hv_vmbus_channel_msg_type msg_type;

		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE)
			break; /* no message */

		hdr = (hv_vmbus_channel_msg_header *)msg->u.payload;
		msg_type = hdr->message_type;

		if (msg_type >= HV_CHANNEL_MESSAGE_COUNT) {
			printf("VMBUS: unknown message type = %d\n", msg_type);
			goto handled;
		}

		entry = &g_channel_message_table[msg_type];
		if (entry->messageHandler)
			entry->messageHandler(hdr);
handled:
		msg->header.message_type = HV_MESSAGE_TYPE_NONE;
		/*
		 * Make sure the write to message_type (ie set to
		 * HV_MESSAGE_TYPE_NONE) happens before we read the
		 * message_pending and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages
		 * since there is no empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->header.message_flags.u.message_pending) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(HV_X64_MSR_EOM, 0);
		}
	}
}

/**
 * @brief Interrupt filter routine for VMBUS.
 *
 * The purpose of this routine is to determine the type of VMBUS protocol
 * message to process - an event or a channel message.
 */
static inline int
hv_vmbus_isr(struct trapframe *frame)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	int cpu = curcpu;
	hv_vmbus_message *msg;
	void *page_addr;

	/*
	 * The Windows team has advised that we check for events
	 * before checking for messages. This is the way they do it
	 * in Windows when running as a guest in Hyper-V
	 */
	sc->vmbus_event_proc(sc, cpu);

	/* Check if there are actual msgs to be process */
	page_addr = hv_vmbus_g_context.syn_ic_msg_page[cpu];
	msg = ((hv_vmbus_message *)page_addr) + HV_VMBUS_TIMER_SINT;

	/* we call eventtimer process the message */
	if (msg->header.message_type == HV_MESSAGE_TIMER_EXPIRED) {
		msg->header.message_type = HV_MESSAGE_TYPE_NONE;

		/* call intrrupt handler of event timer */
		hv_et_intr(frame);

		/*
		 * Make sure the write to message_type (ie set to
		 * HV_MESSAGE_TYPE_NONE) happens before we read the
		 * message_pending and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages
		 * since there is no empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();

		if (msg->header.message_flags.u.message_pending) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(HV_X64_MSR_EOM, 0);
		}
	}

	msg = ((hv_vmbus_message *)page_addr) + HV_VMBUS_MESSAGE_SINT;
	if (msg->header.message_type != HV_MESSAGE_TYPE_NONE) {
		taskqueue_enqueue(hv_vmbus_g_context.hv_msg_tq[cpu],
		    &hv_vmbus_g_context.hv_msg_task[cpu]);
	}

	return (FILTER_HANDLED);
}

u_long *hv_vmbus_intr_cpu[MAXCPU];

void
hv_vector_handler(struct trapframe *trap_frame)
{
	int cpu;

	/*
	 * Disable preemption.
	 */
	critical_enter();

	/*
	 * Do a little interrupt counting.
	 */
	cpu = PCPU_GET(cpuid);
	(*hv_vmbus_intr_cpu[cpu])++;

	hv_vmbus_isr(trap_frame);

	/*
	 * Enable preemption.
	 */
	critical_exit();
}

static int
vmbus_read_ivar(
	device_t	dev,
	device_t	child,
	int		index,
	uintptr_t*	result)
{
	struct hv_device *child_dev_ctx = device_get_ivars(child);

	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
		*result = (uintptr_t) &child_dev_ctx->class_id;
		return (0);
	case HV_VMBUS_IVAR_INSTANCE:
		*result = (uintptr_t) &child_dev_ctx->device_id;
		return (0);
	case HV_VMBUS_IVAR_DEVCTX:
		*result = (uintptr_t) child_dev_ctx;
		return (0);
	case HV_VMBUS_IVAR_NODE:
		*result = (uintptr_t) child_dev_ctx->device;
		return (0);
	}
	return (ENOENT);
}

static int
vmbus_write_ivar(
	device_t	dev,
	device_t	child,
	int		index,
	uintptr_t	value)
{
	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
	case HV_VMBUS_IVAR_INSTANCE:
	case HV_VMBUS_IVAR_DEVCTX:
	case HV_VMBUS_IVAR_NODE:
		/* read-only */
		return (EINVAL);
	}
	return (ENOENT);
}

static int
vmbus_child_pnpinfo_str(device_t dev, device_t child, char *buf, size_t buflen)
{
	char guidbuf[40];
	struct hv_device *dev_ctx = device_get_ivars(child);

	if (dev_ctx == NULL)
		return (0);

	strlcat(buf, "classid=", buflen);
	snprintf_hv_guid(guidbuf, sizeof(guidbuf), &dev_ctx->class_id);
	strlcat(buf, guidbuf, buflen);

	strlcat(buf, " deviceid=", buflen);
	snprintf_hv_guid(guidbuf, sizeof(guidbuf), &dev_ctx->device_id);
	strlcat(buf, guidbuf, buflen);

	return (0);
}

struct hv_device*
hv_vmbus_child_device_create(
	hv_guid		type,
	hv_guid		instance,
	hv_vmbus_channel*	channel)
{
	hv_device* child_dev;

	/*
	 * Allocate the new child device
	 */
	child_dev = malloc(sizeof(hv_device), M_DEVBUF,
			M_WAITOK |  M_ZERO);

	child_dev->channel = channel;
	memcpy(&child_dev->class_id, &type, sizeof(hv_guid));
	memcpy(&child_dev->device_id, &instance, sizeof(hv_guid));

	return (child_dev);
}

int
snprintf_hv_guid(char *buf, size_t sz, const hv_guid *guid)
{
	int cnt;
	const unsigned char *d = guid->data;

	cnt = snprintf(buf, sz,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6],
		d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	return (cnt);
}

int
hv_vmbus_child_device_register(struct hv_device *child_dev)
{
	device_t child;

	if (bootverbose) {
		char name[40];
		snprintf_hv_guid(name, sizeof(name), &child_dev->class_id);
		printf("VMBUS: Class ID: %s\n", name);
	}

	child = device_add_child(vmbus_devp, NULL, -1);
	child_dev->device = child;
	device_set_ivars(child, child_dev);

	return (0);
}

int
hv_vmbus_child_device_unregister(struct hv_device *child_dev)
{
	int ret = 0;
	/*
	 * XXXKYS: Ensure that this is the opposite of
	 * device_add_child()
	 */
	mtx_lock(&Giant);
	ret = device_delete_child(vmbus_devp, child_dev->device);
	mtx_unlock(&Giant);
	return(ret);
}

static int
vmbus_probe(device_t dev)
{
	if (ACPI_ID_PROBE(device_get_parent(dev), dev, vmbus_ids) == NULL ||
	    device_get_unit(dev) != 0)
		return (ENXIO);

	device_set_desc(dev, "Hyper-V Vmbus");

	return (BUS_PROBE_DEFAULT);
}

extern inthand_t IDTVEC(rsvd), IDTVEC(hv_vmbus_callback);

/**
 * @brief Find a free IDT slot and setup the interrupt handler.
 */
static int
vmbus_vector_alloc(void)
{
	int vector;
	uintptr_t func;
	struct gate_descriptor *ip;

	/*
	 * Search backwards form the highest IDT vector available for use
	 * as vmbus channel callback vector. We install 'hv_vmbus_callback'
	 * handler at that vector and use it to interrupt vcpus.
	 */
	vector = APIC_SPURIOUS_INT;
	while (--vector >= APIC_IPI_INTS) {
		ip = &idt[vector];
		func = ((long)ip->gd_hioffset << 16 | ip->gd_looffset);
		if (func == (uintptr_t)&IDTVEC(rsvd)) {
#ifdef __i386__
			setidt(vector , IDTVEC(hv_vmbus_callback), SDT_SYS386IGT,
			    SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else
			setidt(vector , IDTVEC(hv_vmbus_callback), SDT_SYSIGT,
			    SEL_KPL, 0);
#endif

			return (vector);
		}
	}
	return (0);
}

/**
 * @brief Restore the IDT slot to rsvd.
 */
static void
vmbus_vector_free(int vector)
{
        uintptr_t func;
        struct gate_descriptor *ip;

	if (vector == 0)
		return;

        KASSERT(vector >= APIC_IPI_INTS && vector < APIC_SPURIOUS_INT,
            ("invalid vector %d", vector));

        ip = &idt[vector];
        func = ((long)ip->gd_hioffset << 16 | ip->gd_looffset);
        KASSERT(func == (uintptr_t)&IDTVEC(hv_vmbus_callback),
            ("invalid vector %d", vector));

        setidt(vector, IDTVEC(rsvd), SDT_SYSIGT, SEL_KPL, 0);
}

static void
vmbus_cpuset_setthread_task(void *xmask, int pending __unused)
{
	cpuset_t *mask = xmask;
	int error;

	error = cpuset_setthread(curthread->td_tid, mask);
	if (error) {
		panic("curthread=%ju: can't pin; error=%d",
		    (uintmax_t)curthread->td_tid, error);
	}
}

/**
 * @brief Main vmbus driver initialization routine.
 *
 * Here, we
 * - initialize the vmbus driver context
 * - setup various driver entry points
 * - invoke the vmbus hv main init routine
 * - get the irq resource
 * - invoke the vmbus to add the vmbus root device
 * - setup the vmbus root device
 * - retrieve the channel offers
 */
static int
vmbus_bus_init(void)
{
	struct vmbus_softc *sc;
	int i, j, n, ret;
	char buf[MAXCOMLEN + 1];
	cpuset_t cpu_mask;

	if (vmbus_inited)
		return (0);

	vmbus_inited = 1;
	sc = vmbus_get_softc();

	ret = hv_vmbus_init();

	if (ret) {
		if(bootverbose)
			printf("Error VMBUS: Hypervisor Initialization Failed!\n");
		return (ret);
	}

	/*
	 * Find a free IDT slot for vmbus callback.
	 */
	hv_vmbus_g_context.hv_cb_vector = vmbus_vector_alloc();

	if (hv_vmbus_g_context.hv_cb_vector == 0) {
		if(bootverbose)
			printf("Error VMBUS: Cannot find free IDT slot for "
			    "vmbus callback!\n");
		goto cleanup;
	}

	if(bootverbose)
		printf("VMBUS: vmbus callback vector %d\n",
		    hv_vmbus_g_context.hv_cb_vector);

	/*
	 * Notify the hypervisor of our vector.
	 */
	setup_args.vector = hv_vmbus_g_context.hv_cb_vector;

	CPU_FOREACH(j) {
		snprintf(buf, sizeof(buf), "cpu%d:hyperv", j);
		intrcnt_add(buf, &hv_vmbus_intr_cpu[j]);

		for (i = 0; i < 2; i++)
			setup_args.page_buffers[2 * j + i] = NULL;
	}

	/*
	 * Per cpu setup.
	 */
	CPU_FOREACH(j) {
		struct task cpuset_task;

		/*
		 * Setup taskqueue to handle events
		 */
		hv_vmbus_g_context.hv_event_queue[j] = taskqueue_create_fast("hyperv event", M_WAITOK,
			taskqueue_thread_enqueue, &hv_vmbus_g_context.hv_event_queue[j]);
		taskqueue_start_threads(&hv_vmbus_g_context.hv_event_queue[j], 1, PI_NET,
			"hvevent%d", j);

		CPU_SETOF(j, &cpu_mask);
		TASK_INIT(&cpuset_task, 0, vmbus_cpuset_setthread_task, &cpu_mask);
		taskqueue_enqueue(hv_vmbus_g_context.hv_event_queue[j], &cpuset_task);
		taskqueue_drain(hv_vmbus_g_context.hv_event_queue[j], &cpuset_task);

		/*
		 * Setup per-cpu tasks and taskqueues to handle msg.
		 */
		hv_vmbus_g_context.hv_msg_tq[j] = taskqueue_create_fast(
		    "hyperv msg", M_WAITOK, taskqueue_thread_enqueue,
		    &hv_vmbus_g_context.hv_msg_tq[j]);
		taskqueue_start_threads(&hv_vmbus_g_context.hv_msg_tq[j], 1, PI_NET,
		    "hvmsg%d", j);
		TASK_INIT(&hv_vmbus_g_context.hv_msg_task[j], 0,
		    vmbus_msg_task, NULL);

		CPU_SETOF(j, &cpu_mask);
		TASK_INIT(&cpuset_task, 0, vmbus_cpuset_setthread_task, &cpu_mask);
		taskqueue_enqueue(hv_vmbus_g_context.hv_msg_tq[j], &cpuset_task);
		taskqueue_drain(hv_vmbus_g_context.hv_msg_tq[j], &cpuset_task);

		/*
		 * Prepare the per cpu msg and event pages to be called on each cpu.
		 */
		for(i = 0; i < 2; i++) {
			setup_args.page_buffers[2 * j + i] =
				malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO);
		}
	}

	if (bootverbose)
		printf("VMBUS: Calling smp_rendezvous, smp_started = %d\n",
		    smp_started);

	smp_rendezvous(NULL, hv_vmbus_synic_init, NULL, &setup_args);

	/*
	 * Connect to VMBus in the root partition
	 */
	ret = hv_vmbus_connect();

	if (ret != 0)
		goto cleanup1;

	if (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008 ||
	    hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)
		sc->vmbus_event_proc = vmbus_event_proc_compat;
	else
		sc->vmbus_event_proc = vmbus_event_proc;

	hv_vmbus_request_channel_offers();

	vmbus_scan();
	bus_generic_attach(vmbus_devp);
	device_printf(vmbus_devp, "device scan, probe and attach done\n");

	return (ret);

	cleanup1:
	/*
	 * Free pages alloc'ed
	 */
	for (n = 0; n < 2 * MAXCPU; n++)
		if (setup_args.page_buffers[n] != NULL)
			free(setup_args.page_buffers[n], M_DEVBUF);

	/*
	 * remove swi and vmbus callback vector;
	 */
	CPU_FOREACH(j) {
		if (hv_vmbus_g_context.hv_event_queue[j] != NULL) {
			taskqueue_free(hv_vmbus_g_context.hv_event_queue[j]);
			hv_vmbus_g_context.hv_event_queue[j] = NULL;
		}
	}

	vmbus_vector_free(hv_vmbus_g_context.hv_cb_vector);

	cleanup:
	hv_vmbus_cleanup();

	return (ret);
}

static void
vmbus_event_proc_dummy(struct vmbus_softc *sc __unused, int cpu __unused)
{
}

static int
vmbus_attach(device_t dev)
{
	if(bootverbose)
		device_printf(dev, "VMBUS: attach dev: %p\n", dev);

	vmbus_devp = dev;
	vmbus_sc = device_get_softc(dev);

	/*
	 * Event processing logic will be configured:
	 * - After the vmbus protocol version negotiation.
	 * - Before we request channel offers.
	 */
	vmbus_sc->vmbus_event_proc = vmbus_event_proc_dummy;

	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold)
		vmbus_bus_init();

	bus_generic_probe(dev);
	return (0);
}

static void
vmbus_sysinit(void *arg __unused)
{
	if (vm_guest != VM_GUEST_HV || vmbus_get_softc() == NULL)
		return;

	/* 
	 * If the system has already booted and thread
	 * scheduling is possible, as indicated by the
	 * global cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) 
		vmbus_bus_init();
}

static int
vmbus_detach(device_t dev)
{
	int i;

	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	smp_rendezvous(NULL, hv_vmbus_synic_cleanup, NULL, NULL);

	for(i = 0; i < 2 * MAXCPU; i++) {
		if (setup_args.page_buffers[i] != NULL)
			free(setup_args.page_buffers[i], M_DEVBUF);
	}

	hv_vmbus_cleanup();

	/* remove swi */
	CPU_FOREACH(i) {
		if (hv_vmbus_g_context.hv_event_queue[i] != NULL) {
			taskqueue_free(hv_vmbus_g_context.hv_event_queue[i]);
			hv_vmbus_g_context.hv_event_queue[i] = NULL;
		}
	}

	vmbus_vector_free(hv_vmbus_g_context.hv_cb_vector);

	return (0);
}

static device_method_t vmbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			vmbus_probe),
	DEVMETHOD(device_attach,		vmbus_attach),
	DEVMETHOD(device_detach,		vmbus_detach),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		bus_generic_add_child),
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		vmbus_read_ivar),
	DEVMETHOD(bus_write_ivar,		vmbus_write_ivar),
	DEVMETHOD(bus_child_pnpinfo_str,	vmbus_child_pnpinfo_str),

	DEVMETHOD_END
};

static driver_t vmbus_driver = {
	"vmbus",
	vmbus_methods,
	sizeof(struct vmbus_softc)
};

static devclass_t vmbus_devclass;

DRIVER_MODULE(vmbus, acpi, vmbus_driver, vmbus_devclass, NULL, NULL);
MODULE_DEPEND(vmbus, acpi, 1, 1, 1);
MODULE_VERSION(vmbus, 1);

/*
 * NOTE:
 * We have to start as the last step of SI_SUB_SMP, i.e. after SMP is
 * initialized.
 */
SYSINIT(vmbus_initialize, SI_SUB_SMP, SI_ORDER_ANY, vmbus_sysinit, NULL);

