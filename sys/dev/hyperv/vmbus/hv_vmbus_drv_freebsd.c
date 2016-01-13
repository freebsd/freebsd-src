/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
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

#include "hv_vmbus_priv.h"


#define VMBUS_IRQ	0x5

static device_t vmbus_devp;
static int vmbus_inited;
static hv_setup_args setup_args; /* only CPU 0 supported at this time */

/**
 * @brief Software interrupt thread routine to handle channel messages from
 * the hypervisor.
 */
static void
vmbus_msg_swintr(void *arg)
{
	int 			cpu;
	void*			page_addr;
	hv_vmbus_channel_msg_header	 *hdr;
	hv_vmbus_channel_msg_table_entry *entry;
	hv_vmbus_channel_msg_type msg_type;
	hv_vmbus_message*	msg;
	hv_vmbus_message*	copied;
	static bool warned	= false;

	cpu = (int)(long)arg;
	KASSERT(cpu <= mp_maxid, ("VMBUS: vmbus_msg_swintr: "
	    "cpu out of range!"));

	page_addr = hv_vmbus_g_context.syn_ic_msg_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;

	for (;;) {
		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE)
			break; /* no message */

		hdr = (hv_vmbus_channel_msg_header *)msg->u.payload;
		msg_type = hdr->message_type;

		if (msg_type >= HV_CHANNEL_MESSAGE_COUNT && !warned) {
			warned = true;
			printf("VMBUS: unknown message type = %d\n", msg_type);
			goto handled;
		}

		entry = &g_channel_message_table[msg_type];

		if (entry->handler_no_sleep)
			entry->messageHandler(hdr);
		else {

			copied = malloc(sizeof(hv_vmbus_message),
					M_DEVBUF, M_NOWAIT);
			KASSERT(copied != NULL,
				("Error VMBUS: malloc failed to allocate"
					" hv_vmbus_message!"));
			if (copied == NULL)
				continue;

			memcpy(copied, msg, sizeof(hv_vmbus_message));
			hv_queue_work_item(hv_vmbus_g_connection.work_queue,
					   hv_vmbus_on_channel_message,
					   copied);
		}
handled:
	    msg->header.message_type = HV_MESSAGE_TYPE_NONE;

	    /*
	     * Make sure the write to message_type (ie set to
	     * HV_MESSAGE_TYPE_NONE) happens before we read the
	     * message_pending and EOMing. Otherwise, the EOMing will
	     * not deliver any more messages
	     * since there is no empty slot
	     */
	    wmb();

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
hv_vmbus_isr(void *unused) 
{
	int				cpu;
	hv_vmbus_message*		msg;
	hv_vmbus_synic_event_flags*	event;
	void*				page_addr;

	cpu = PCPU_GET(cpuid);

	/*
	 * The Windows team has advised that we check for events
	 * before checking for messages. This is the way they do it
	 * in Windows when running as a guest in Hyper-V
	 */

	page_addr = hv_vmbus_g_context.syn_ic_event_page[cpu];
	event = (hv_vmbus_synic_event_flags*)
		    page_addr + HV_VMBUS_MESSAGE_SINT;

	if ((hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008) ||
	    (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)) {
		/* Since we are a child, we only need to check bit 0 */
		if (synch_test_and_clear_bit(0, &event->flags32[0])) {
			swi_sched(hv_vmbus_g_context.event_swintr[cpu], 0);
		}
	} else {
		/*
		 * On host with Win8 or above, we can directly look at
		 * the event page. If bit n is set, we have an interrupt 
		 * on the channel with id n.
		 * Directly schedule the event software interrupt on
		 * current cpu.
		 */
		swi_sched(hv_vmbus_g_context.event_swintr[cpu], 0);
	}

	/* Check if there are actual msgs to be process */
	page_addr = hv_vmbus_g_context.syn_ic_msg_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;

	if (msg->header.message_type != HV_MESSAGE_TYPE_NONE) {
		swi_sched(hv_vmbus_g_context.msg_swintr[cpu], 0);
	}

	return FILTER_HANDLED;
}

#ifdef HV_DEBUG_INTR 
uint32_t hv_intr_count = 0;
#endif
uint32_t hv_vmbus_swintr_event_cpu[MAXCPU];
uint32_t hv_vmbus_intr_cpu[MAXCPU];

void
hv_vector_handler(struct trapframe *trap_frame)
{
#ifdef HV_DEBUG_INTR
	int cpu;
#endif

	/*
	 * Disable preemption.
	 */
	critical_enter();

#ifdef HV_DEBUG_INTR
	/*
	 * Do a little interrupt counting.
	 */
	cpu = PCPU_GET(cpuid);
	hv_vmbus_intr_cpu[cpu]++;
	hv_intr_count++;
#endif

	hv_vmbus_isr(NULL); 

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
			M_NOWAIT |  M_ZERO);
	KASSERT(child_dev != NULL,
	    ("Error VMBUS: malloc failed to allocate hv_device!"));

	if (child_dev == NULL)
		return (NULL);

	child_dev->channel = channel;
	memcpy(&child_dev->class_id, &type, sizeof(hv_guid));
	memcpy(&child_dev->device_id, &instance, sizeof(hv_guid));

	return (child_dev);
}

static void
print_dev_guid(struct hv_device *dev)
{
	int i;
	unsigned char guid_name[100];
	for (i = 0; i < 32; i += 2)
		sprintf(&guid_name[i], "%02x", dev->class_id.data[i / 2]);
	if(bootverbose)
		printf("VMBUS: Class ID: %s\n", guid_name);
}

int
hv_vmbus_child_device_register(struct hv_device *child_dev)
{
	device_t child;
	int ret = 0;

	print_dev_guid(child_dev);


	child = device_add_child(vmbus_devp, NULL, -1);
	child_dev->device = child;
	device_set_ivars(child, child_dev);

	mtx_lock(&Giant);
	ret = device_probe_and_attach(child);
	mtx_unlock(&Giant);

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

static void
vmbus_identify(driver_t *driver, device_t parent)
{
	if (!hv_vmbus_query_hypervisor_presence())
		return;

	vm_guest = VM_GUEST_HV;

	BUS_ADD_CHILD(parent, 0, "vmbus", 0);
}

static int
vmbus_probe(device_t dev) {
	if(bootverbose)
		device_printf(dev, "VMBUS: probe\n");

	device_set_desc(dev, "Vmbus Devices");

	return (BUS_PROBE_NOWILDCARD);
}

#ifdef HYPERV
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

#else /* HYPERV */

static int
vmbus_vector_alloc(void)
{
	return(0);
}

static void
vmbus_vector_free(int vector)
{
}

#endif /* HYPERV */

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
	int i, j, n, ret;

	if (vmbus_inited)
		return (0);

	vmbus_inited = 1;

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
		hv_vmbus_intr_cpu[j] = 0;
		hv_vmbus_swintr_event_cpu[j] = 0;
		hv_vmbus_g_context.hv_event_intr_event[j] = NULL;
		hv_vmbus_g_context.hv_msg_intr_event[j] = NULL;
		hv_vmbus_g_context.event_swintr[j] = NULL;
		hv_vmbus_g_context.msg_swintr[j] = NULL;

		for (i = 0; i < 2; i++)
			setup_args.page_buffers[2 * j + i] = NULL;
	}

	/*
	 * Per cpu setup.
	 */
	CPU_FOREACH(j) {
		/*
		 * Setup software interrupt thread and handler for msg handling.
		 */
		ret = swi_add(&hv_vmbus_g_context.hv_msg_intr_event[j],
		    "hv_msg", vmbus_msg_swintr, (void *)(long)j, SWI_CLOCK, 0,
		    &hv_vmbus_g_context.msg_swintr[j]);
		if (ret) {
			if(bootverbose)
				printf("VMBUS: failed to setup msg swi for "
				    "cpu %d\n", j);
			goto cleanup1;
		}

		/*
		 * Bind the swi thread to the cpu.
		 */
		ret = intr_event_bind(hv_vmbus_g_context.hv_msg_intr_event[j],
		    j);
	 	if (ret) {
			if(bootverbose)
				printf("VMBUS: failed to bind msg swi thread "
				    "to cpu %d\n", j);
			goto cleanup1;
		}

		/*
		 * Setup software interrupt thread and handler for
		 * event handling.
		 */
		ret = swi_add(&hv_vmbus_g_context.hv_event_intr_event[j],
		    "hv_event", hv_vmbus_on_events, (void *)(long)j,
		    SWI_CLOCK, 0, &hv_vmbus_g_context.event_swintr[j]);
		if (ret) {
			if(bootverbose)
				printf("VMBUS: failed to setup event swi for "
				    "cpu %d\n", j);
			goto cleanup1;
		}

		/*
		 * Prepare the per cpu msg and event pages to be called on each cpu.
		 */
		for(i = 0; i < 2; i++) {
			setup_args.page_buffers[2 * j + i] =
				malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT | M_ZERO);
			if (setup_args.page_buffers[2 * j + i] == NULL) {
				KASSERT(setup_args.page_buffers[2 * j + i] != NULL,
					("Error VMBUS: malloc failed!"));
				goto cleanup1;
			}
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

	hv_vmbus_request_channel_offers();
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
		if (hv_vmbus_g_context.msg_swintr[j] != NULL)
			swi_remove(hv_vmbus_g_context.msg_swintr[j]);
		if (hv_vmbus_g_context.event_swintr[j] != NULL)
			swi_remove(hv_vmbus_g_context.event_swintr[j]);
		hv_vmbus_g_context.hv_msg_intr_event[j] = NULL;	
		hv_vmbus_g_context.hv_event_intr_event[j] = NULL;	
	}

	vmbus_vector_free(hv_vmbus_g_context.hv_cb_vector);

	cleanup:
	hv_vmbus_cleanup();

	return (ret);
}

static int
vmbus_attach(device_t dev)
{
	if(bootverbose)
		device_printf(dev, "VMBUS: attach dev: %p\n", dev);
	vmbus_devp = dev;

	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold)
		vmbus_bus_init();

	return (0);
}

static void
vmbus_init(void)
{
	if (vm_guest != VM_GUEST_HV)
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

static void
vmbus_bus_exit(void)
{
	int i;

	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	smp_rendezvous(NULL, hv_vmbus_synic_cleanup, NULL, NULL);

	for(i = 0; i < 2 * MAXCPU; i++) {
		if (setup_args.page_buffers[i] != 0)
			free(setup_args.page_buffers[i], M_DEVBUF);
	}

	hv_vmbus_cleanup();

	/* remove swi */
	CPU_FOREACH(i) {
		if (hv_vmbus_g_context.msg_swintr[i] != NULL)
			swi_remove(hv_vmbus_g_context.msg_swintr[i]);
		if (hv_vmbus_g_context.event_swintr[i] != NULL)
			swi_remove(hv_vmbus_g_context.event_swintr[i]);
		hv_vmbus_g_context.hv_msg_intr_event[i] = NULL;	
		hv_vmbus_g_context.hv_event_intr_event[i] = NULL;	
	}

	vmbus_vector_free(hv_vmbus_g_context.hv_cb_vector);

	return;
}

static void
vmbus_exit(void)
{
	vmbus_bus_exit();
}

static int
vmbus_detach(device_t dev)
{
	vmbus_exit();
	return (0);
}

static void
vmbus_mod_load(void)
{
	if(bootverbose)
		printf("VMBUS: load\n");
}

static void
vmbus_mod_unload(void)
{
	if(bootverbose)
		printf("VMBUS: unload\n");
}

static int
vmbus_modevent(module_t mod, int what, void *arg)
{
	switch (what) {

	case MOD_LOAD:
		vmbus_mod_load();
		break;
	case MOD_UNLOAD:
		vmbus_mod_unload();
		break;
	}

	return (0);
}

static device_method_t vmbus_methods[] = {
	/** Device interface */
	DEVMETHOD(device_identify, vmbus_identify),
	DEVMETHOD(device_probe, vmbus_probe),
	DEVMETHOD(device_attach, vmbus_attach),
	DEVMETHOD(device_detach, vmbus_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),

	/** Bus interface */
	DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_read_ivar, vmbus_read_ivar),
	DEVMETHOD(bus_write_ivar, vmbus_write_ivar),

	{ 0, 0 } };

static char driver_name[] = "vmbus";
static driver_t vmbus_driver = { driver_name, vmbus_methods,0, };


devclass_t vmbus_devclass;

DRIVER_MODULE(vmbus, nexus, vmbus_driver, vmbus_devclass, vmbus_modevent, 0);
MODULE_VERSION(vmbus,1);

/* We want to be started after SMP is initialized */
SYSINIT(vmb_init, SI_SUB_SMP + 1, SI_ORDER_FIRST, vmbus_init, NULL);

