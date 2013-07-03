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
#include <sys/pcpu.h>

#include "hv_vmbus_priv.h"


#define VMBUS_IRQ	0x5

static struct intr_event *hv_msg_intr_event;
static struct intr_event *hv_event_intr_event;
static void *msg_swintr;
static void *event_swintr;
static device_t vmbus_devp;
static void *vmbus_cookiep;
static int vmbus_rid;
struct resource *intr_res;
static int vmbus_irq = VMBUS_IRQ;
static int vmbus_inited;

/**
 * @brief Software interrupt thread routine to handle channel messages from
 * the hypervisor.
 */
static void
vmbus_msg_swintr(void *dummy)
{
	int 			cpu;
	void*			page_addr;
	hv_vmbus_message*	msg;
	hv_vmbus_message*	copied;

	cpu = PCPU_GET(cpuid);
	page_addr = hv_vmbus_g_context.syn_ic_msg_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;

	for (;;) {
		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE) {
			break; /* no message */
		} else {
			copied = malloc(sizeof(hv_vmbus_message),
					M_DEVBUF, M_NOWAIT);
			KASSERT(copied != NULL,
				("Error VMBUS: malloc failed to allocate"
					" hv_vmbus_message!"));
			if (copied == NULL)
				continue;
			memcpy(copied, msg, sizeof(hv_vmbus_message));
			hv_queue_work_item(hv_vmbus_g_connection.work_queue,
			hv_vmbus_on_channel_message, copied);
	    }

	    msg->header.message_type = HV_MESSAGE_TYPE_NONE;

	    /*
	     * Make sure the write to message_type (ie set to
	     * HV_MESSAGE_TYPE_NONE) happens before we read the
	     * message_pending and EOMing. Otherwise, the EOMing will
	     * not deliver any more messages
	     * since there is no empty slot
	     */
	    wmb();

	    if (msg->header.message_flags.message_pending) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			hv_vmbus_write_msr(HV_X64_MSR_EOM, 0);
	    }
	}
}

/**
 * @brief Interrupt filter routine for VMBUS.
 *
 * The purpose of this routine is to determine the type of VMBUS protocol
 * message to process - an event or a channel message.
 * As this is an interrupt filter routine, the function runs in a very
 * restricted envinronment.  From the manpage for bus_setup_intr(9)
 *
 *   In this restricted environment, care must be taken to account for all
 *   races.  A careful analysis of races should be done as well.  It is gener-
 *   ally cheaper to take an extra interrupt, for example, than to protect
 *   variables with spinlocks.	Read, modify, write cycles of hardware regis-
 *   ters need to be carefully analyzed if other threads are accessing the
 *   same registers.
 */
static int
hv_vmbus_isr(void *unused) 
{
	int				cpu;
	hv_vmbus_message*		msg;
	hv_vmbus_synic_event_flags*	event;
	void*				page_addr;

	cpu = PCPU_GET(cpuid);
	/* (Temporary limit) */
	KASSERT(cpu == 0, ("hv_vmbus_isr: Interrupt on CPU other than zero"));

	/*
	 * The Windows team has advised that we check for events
	 * before checking for messages. This is the way they do it
	 * in Windows when running as a guest in Hyper-V
	 */

	page_addr = hv_vmbus_g_context.syn_ic_event_page[cpu];
	event = (hv_vmbus_synic_event_flags*)
		    page_addr + HV_VMBUS_MESSAGE_SINT;

	/* Since we are a child, we only need to check bit 0 */
	if (synch_test_and_clear_bit(0, &event->flags32[0])) {
		swi_sched(event_swintr, 0);
	}

	/* Check if there are actual msgs to be process */
	page_addr = hv_vmbus_g_context.syn_ic_msg_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;

	if (msg->header.message_type != HV_MESSAGE_TYPE_NONE) {
		swi_sched(msg_swintr, 0);
	}

	return FILTER_HANDLED;
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

static void vmbus_identify(driver_t *driver, device_t parent) {
	BUS_ADD_CHILD(parent, 0, "vmbus", 0);
	if (device_find_child(parent, "vmbus", 0) == NULL) {
		BUS_ADD_CHILD(parent, 0, "vmbus", 0);
	}
}

static int
vmbus_probe(device_t dev) {
	if(bootverbose)
		device_printf(dev, "VMBUS: probe\n");

	if (!hv_vmbus_query_hypervisor_presence())
		return (ENXIO);

	device_set_desc(dev, "Vmbus Devices");

	return (0);
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
	struct ioapic_intsrc {
		struct intsrc io_intsrc;
		u_int io_irq;
		u_int io_intpin:8;
		u_int io_vector:8;
		u_int io_cpu:8;
		u_int io_activehi:1;
		u_int io_edgetrigger:1;
		u_int io_masked:1;
		int io_bus:4;
		uint32_t io_lowreg;
	};

	int ret;
	unsigned int vector = 0;
	struct intsrc *isrc;
	struct ioapic_intsrc *intpin;

	if (vmbus_inited)
		return (0);

	vmbus_inited = 1;

	ret = hv_vmbus_init();

	if (ret) {
		if(bootverbose)
			printf("Error VMBUS: Hypervisor Initialization Failed!\n");
		return (ret);
	}

	ret = swi_add(&hv_msg_intr_event, "hv_msg", vmbus_msg_swintr,
	    NULL, SWI_CLOCK, 0, &msg_swintr);

	if (ret)
	    goto cleanup;

	/*
	 * Message SW interrupt handler checks a per-CPU page and
	 * thus the thread needs to be bound to CPU-0 - which is where
	 * all interrupts are processed.
	 */
	ret = intr_event_bind(hv_msg_intr_event, 0);

	if (ret)
		goto cleanup1;

	ret = swi_add(&hv_event_intr_event, "hv_event", hv_vmbus_on_events,
	    NULL, SWI_CLOCK, 0, &event_swintr);

	if (ret)
		goto cleanup1;

	intr_res = bus_alloc_resource(vmbus_devp,
	    SYS_RES_IRQ, &vmbus_rid, vmbus_irq, vmbus_irq, 1, RF_ACTIVE);

	if (intr_res == NULL) {
		ret = ENOMEM; /* XXXKYS: Need a better errno */
		goto cleanup2;
	}

	/*
	 * Setup interrupt filter handler
	 */
	ret = bus_setup_intr(vmbus_devp, intr_res,
	    INTR_TYPE_NET | INTR_MPSAFE, hv_vmbus_isr, NULL,
	    NULL, &vmbus_cookiep);

	if (ret != 0)
		goto cleanup3;

	ret = bus_bind_intr(vmbus_devp, intr_res, 0);
	if (ret != 0)
		goto cleanup4;

	isrc = intr_lookup_source(vmbus_irq);
	if ((isrc == NULL) || (isrc->is_event == NULL)) {
		ret = EINVAL;
		goto cleanup4;
	}

	/* vector = isrc->is_event->ie_vector; */
	intpin = (struct ioapic_intsrc *)isrc;
	vector = intpin->io_vector;

	if(bootverbose)
		printf("VMBUS: irq 0x%x vector 0x%x\n", vmbus_irq, vector);

	/**
	 * Notify the hypervisor of our irq.
	 */

	smp_rendezvous(NULL, hv_vmbus_synic_init, NULL, &vector);

	/**
	 * Connect to VMBus in the root partition
	 */
	ret = hv_vmbus_connect();

	if (ret)
	    goto cleanup4;

	hv_vmbus_request_channel_offers();
	return (ret);

	cleanup4:

	/*
	 * remove swi, bus and intr resource
	 */
	bus_teardown_intr(vmbus_devp, intr_res, vmbus_cookiep);

	cleanup3:

	bus_release_resource(vmbus_devp, SYS_RES_IRQ, vmbus_rid, intr_res);

	cleanup2:
	swi_remove(event_swintr);

	cleanup1:
	swi_remove(msg_swintr);

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
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) 
		vmbus_bus_init();
}

static void
vmbus_bus_exit(void)
{
	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	smp_rendezvous(NULL, hv_vmbus_synic_cleanup, NULL, NULL);

	hv_vmbus_cleanup();

	/* remove swi, bus and intr resource */
	bus_teardown_intr(vmbus_devp, intr_res, vmbus_cookiep);

	bus_release_resource(vmbus_devp, SYS_RES_IRQ, vmbus_rid, intr_res);

	swi_remove(msg_swintr);
	swi_remove(event_swintr);

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

/* TODO: We want to be earlier than SI_SUB_VFS */
SYSINIT(vmb_init, SI_SUB_VFS, SI_ORDER_MIDDLE, vmbus_init, NULL);

