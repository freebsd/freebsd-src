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
#include <dev/hyperv/vmbus/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#include <contrib/dev/acpica/include/acpi.h>
#include "acpi_if.h"

struct vmbus_softc	*vmbus_sc;

extern inthand_t IDTVEC(rsvd), IDTVEC(vmbus_isr);

static void
vmbus_msg_task(void *xsc, int pending __unused)
{
	struct vmbus_softc *sc = xsc;
	volatile struct vmbus_message *msg;

	msg = VMBUS_PCPU_GET(sc, message, curcpu) + VMBUS_SINT_MESSAGE;
	for (;;) {
		if (msg->msg_type == VMBUS_MSGTYPE_NONE) {
			/* No message */
			break;
		} else if (msg->msg_type == VMBUS_MSGTYPE_CHANNEL) {
			/* Channel message */
			vmbus_chan_msgproc(sc,
			    __DEVOLATILE(const struct vmbus_message *, msg));
		}

		msg->msg_type = VMBUS_MSGTYPE_NONE;
		/*
		 * Make sure the write to msg_type (i.e. set to
		 * VMBUS_MSGTYPE_NONE) happens before we read the
		 * msg_flags and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages since there is no
		 * empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->msg_flags & VMBUS_MSGFLAG_PENDING) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(MSR_HV_EOM, 0);
		}
	}
}

static __inline int
vmbus_handle_intr1(struct vmbus_softc *sc, struct trapframe *frame, int cpu)
{
	volatile struct vmbus_message *msg;
	struct vmbus_message *msg_base;

	msg_base = VMBUS_PCPU_GET(sc, message, cpu);

	/*
	 * Check event timer.
	 *
	 * TODO: move this to independent IDT vector.
	 */
	msg = msg_base + VMBUS_SINT_TIMER;
	if (msg->msg_type == VMBUS_MSGTYPE_TIMER_EXPIRED) {
		msg->msg_type = VMBUS_MSGTYPE_NONE;

		vmbus_et_intr(frame);

		/*
		 * Make sure the write to msg_type (i.e. set to
		 * VMBUS_MSGTYPE_NONE) happens before we read the
		 * msg_flags and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages since there is no
		 * empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->msg_flags & VMBUS_MSGFLAG_PENDING) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(MSR_HV_EOM, 0);
		}
	}

	/*
	 * Check events.  Hot path for network and storage I/O data; high rate.
	 *
	 * NOTE:
	 * As recommended by the Windows guest fellows, we check events before
	 * checking messages.
	 */
	sc->vmbus_event_proc(sc, cpu);

	/*
	 * Check messages.  Mainly management stuffs; ultra low rate.
	 */
	msg = msg_base + VMBUS_SINT_MESSAGE;
	if (__predict_false(msg->msg_type != VMBUS_MSGTYPE_NONE)) {
		taskqueue_enqueue(VMBUS_PCPU_GET(sc, message_tq, cpu),
		    VMBUS_PCPU_PTR(sc, message_task, cpu));
	}

	return (FILTER_HANDLED);
}

void
vmbus_handle_intr(struct trapframe *trap_frame)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	int cpu = curcpu;

	/*
	 * Disable preemption.
	 */
	critical_enter();

	/*
	 * Do a little interrupt counting.
	 */
	(*VMBUS_PCPU_GET(sc, intr_cnt, cpu))++;

	vmbus_handle_intr1(sc, trap_frame, cpu);

	/*
	 * Enable preemption.
	 */
	critical_exit();
}

static void
vmbus_synic_setup(void *xsc)
{
	struct vmbus_softc *sc = xsc;
	int cpu = curcpu;
	uint64_t val, orig;
	uint32_t sint;

	if (hyperv_features & CPUID_HV_MSR_VP_INDEX) {
		/*
		 * Save virtual processor id.
		 */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = rdmsr(MSR_HV_VP_INDEX);
	} else {
		/*
		 * XXX
		 * Virtual processoor id is only used by a pretty broken
		 * channel selection code from storvsc.  It's nothing
		 * critical even if CPUID_HV_MSR_VP_INDEX is not set; keep
		 * moving on.
		 */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = cpu;
	}

	/*
	 * Setup the SynIC message.
	 */
	orig = rdmsr(MSR_HV_SIMP);
	val = MSR_HV_SIMP_ENABLE | (orig & MSR_HV_SIMP_RSVD_MASK) |
	    ((VMBUS_PCPU_GET(sc, message_dma.hv_paddr, cpu) >> PAGE_SHIFT) <<
	     MSR_HV_SIMP_PGSHIFT);
	wrmsr(MSR_HV_SIMP, val);

	/*
	 * Setup the SynIC event flags.
	 */
	orig = rdmsr(MSR_HV_SIEFP);
	val = MSR_HV_SIEFP_ENABLE | (orig & MSR_HV_SIEFP_RSVD_MASK) |
	    ((VMBUS_PCPU_GET(sc, event_flags_dma.hv_paddr, cpu)
	      >> PAGE_SHIFT) << MSR_HV_SIEFP_PGSHIFT);
	wrmsr(MSR_HV_SIEFP, val);


	/*
	 * Configure and unmask SINT for message and event flags.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = rdmsr(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	wrmsr(sint, val);

	/*
	 * Configure and unmask SINT for timer.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = rdmsr(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	wrmsr(sint, val);

	/*
	 * All done; enable SynIC.
	 */
	orig = rdmsr(MSR_HV_SCONTROL);
	val = MSR_HV_SCTRL_ENABLE | (orig & MSR_HV_SCTRL_RSVD_MASK);
	wrmsr(MSR_HV_SCONTROL, val);
}

static void
vmbus_synic_teardown(void *arg)
{
	uint64_t orig;
	uint32_t sint;

	/*
	 * Disable SynIC.
	 */
	orig = rdmsr(MSR_HV_SCONTROL);
	wrmsr(MSR_HV_SCONTROL, (orig & MSR_HV_SCTRL_RSVD_MASK));

	/*
	 * Mask message and event flags SINT.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = rdmsr(sint);
	wrmsr(sint, orig | MSR_HV_SINT_MASKED);

	/*
	 * Mask timer SINT.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = rdmsr(sint);
	wrmsr(sint, orig | MSR_HV_SINT_MASKED);

	/*
	 * Teardown SynIC message.
	 */
	orig = rdmsr(MSR_HV_SIMP);
	wrmsr(MSR_HV_SIMP, (orig & MSR_HV_SIMP_RSVD_MASK));

	/*
	 * Teardown SynIC event flags.
	 */
	orig = rdmsr(MSR_HV_SIEFP);
	wrmsr(MSR_HV_SIEFP, (orig & MSR_HV_SIEFP_RSVD_MASK));
}

static int
vmbus_dma_alloc(struct vmbus_softc *sc)
{
	bus_dma_tag_t parent_dtag;
	uint8_t *evtflags;
	int cpu;

	parent_dtag = bus_get_dma_tag(sc->vmbus_dev);
	CPU_FOREACH(cpu) {
		void *ptr;

		/*
		 * Per-cpu messages and event flags.
		 */
		ptr = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
		    PAGE_SIZE, VMBUS_PCPU_PTR(sc, message_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, message, cpu) = ptr;

		ptr = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
		    PAGE_SIZE, VMBUS_PCPU_PTR(sc, event_flags_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, event_flags, cpu) = ptr;
	}

	evtflags = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_evtflags_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (evtflags == NULL)
		return ENOMEM;
	sc->vmbus_rx_evtflags = (u_long *)evtflags;
	sc->vmbus_tx_evtflags = (u_long *)(evtflags + (PAGE_SIZE / 2));
	sc->vmbus_evtflags = evtflags;

	sc->vmbus_mnf1 = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_mnf1_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->vmbus_mnf1 == NULL)
		return ENOMEM;

	sc->vmbus_mnf2 = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_mnf2_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->vmbus_mnf2 == NULL)
		return ENOMEM;

	return 0;
}

static void
vmbus_dma_free(struct vmbus_softc *sc)
{
	int cpu;

	if (sc->vmbus_evtflags != NULL) {
		hyperv_dmamem_free(&sc->vmbus_evtflags_dma, sc->vmbus_evtflags);
		sc->vmbus_evtflags = NULL;
		sc->vmbus_rx_evtflags = NULL;
		sc->vmbus_tx_evtflags = NULL;
	}
	if (sc->vmbus_mnf1 != NULL) {
		hyperv_dmamem_free(&sc->vmbus_mnf1_dma, sc->vmbus_mnf1);
		sc->vmbus_mnf1 = NULL;
	}
	if (sc->vmbus_mnf2 != NULL) {
		hyperv_dmamem_free(&sc->vmbus_mnf2_dma, sc->vmbus_mnf2);
		sc->vmbus_mnf2 = NULL;
	}

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, message, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, message_dma, cpu),
			    VMBUS_PCPU_GET(sc, message, cpu));
			VMBUS_PCPU_GET(sc, message, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, event_flags, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, event_flags_dma, cpu),
			    VMBUS_PCPU_GET(sc, event_flags, cpu));
			VMBUS_PCPU_GET(sc, event_flags, cpu) = NULL;
		}
	}
}

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
	 * as vmbus channel callback vector. We install 'vmbus_isr'
	 * handler at that vector and use it to interrupt vcpus.
	 */
	vector = APIC_SPURIOUS_INT;
	while (--vector >= APIC_IPI_INTS) {
		ip = &idt[vector];
		func = ((long)ip->gd_hioffset << 16 | ip->gd_looffset);
		if (func == (uintptr_t)&IDTVEC(rsvd)) {
#ifdef __i386__
			setidt(vector , IDTVEC(vmbus_isr), SDT_SYS386IGT,
			    SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else
			setidt(vector , IDTVEC(vmbus_isr), SDT_SYSIGT,
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
	KASSERT(func == (uintptr_t)&IDTVEC(vmbus_isr),
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

static int
vmbus_intr_setup(struct vmbus_softc *sc)
{
	int cpu;

	CPU_FOREACH(cpu) {
		struct task cpuset_task;
		char buf[MAXCOMLEN + 1];
		cpuset_t cpu_mask;

		/* Allocate an interrupt counter for Hyper-V interrupt */
		snprintf(buf, sizeof(buf), "cpu%d:hyperv", cpu);
		intrcnt_add(buf, VMBUS_PCPU_PTR(sc, intr_cnt, cpu));

		/*
		 * Setup taskqueue to handle events.  Task will be per-
		 * channel.
		 */
		VMBUS_PCPU_GET(sc, event_tq, cpu) = taskqueue_create_fast(
		    "hyperv event", M_WAITOK, taskqueue_thread_enqueue,
		    VMBUS_PCPU_PTR(sc, event_tq, cpu));
		taskqueue_start_threads(VMBUS_PCPU_PTR(sc, event_tq, cpu),
		    1, PI_NET, "hvevent%d", cpu);

		CPU_SETOF(cpu, &cpu_mask);
		TASK_INIT(&cpuset_task, 0, vmbus_cpuset_setthread_task,
		    &cpu_mask);
		taskqueue_enqueue(VMBUS_PCPU_GET(sc, event_tq, cpu),
		    &cpuset_task);
		taskqueue_drain(VMBUS_PCPU_GET(sc, event_tq, cpu),
		    &cpuset_task);

		/*
		 * Setup tasks and taskqueues to handle messages.
		 */
		VMBUS_PCPU_GET(sc, message_tq, cpu) = taskqueue_create_fast(
		    "hyperv msg", M_WAITOK, taskqueue_thread_enqueue,
		    VMBUS_PCPU_PTR(sc, message_tq, cpu));
		taskqueue_start_threads(VMBUS_PCPU_PTR(sc, message_tq, cpu), 1,
		    PI_NET, "hvmsg%d", cpu);
		TASK_INIT(VMBUS_PCPU_PTR(sc, message_task, cpu), 0,
		    vmbus_msg_task, sc);

		CPU_SETOF(cpu, &cpu_mask);
		TASK_INIT(&cpuset_task, 0, vmbus_cpuset_setthread_task,
		    &cpu_mask);
		taskqueue_enqueue(VMBUS_PCPU_GET(sc, message_tq, cpu),
		    &cpuset_task);
		taskqueue_drain(VMBUS_PCPU_GET(sc, message_tq, cpu),
		    &cpuset_task);
	}

	/*
	 * All Hyper-V ISR required resources are setup, now let's find a
	 * free IDT vector for Hyper-V ISR and set it up.
	 */
	sc->vmbus_idtvec = vmbus_vector_alloc();
	if (sc->vmbus_idtvec == 0) {
		device_printf(sc->vmbus_dev, "cannot find free IDT vector\n");
		return ENXIO;
	}
	if(bootverbose) {
		device_printf(sc->vmbus_dev, "vmbus IDT vector %d\n",
		    sc->vmbus_idtvec);
	}
	return 0;
}

static void
vmbus_intr_teardown(struct vmbus_softc *sc)
{
	int cpu;

	vmbus_vector_free(sc->vmbus_idtvec);

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, event_tq, cpu) != NULL) {
			taskqueue_free(VMBUS_PCPU_GET(sc, event_tq, cpu));
			VMBUS_PCPU_GET(sc, event_tq, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, message_tq, cpu) != NULL) {
			taskqueue_drain(VMBUS_PCPU_GET(sc, message_tq, cpu),
			    VMBUS_PCPU_PTR(sc, message_task, cpu));
			taskqueue_free(VMBUS_PCPU_GET(sc, message_tq, cpu));
			VMBUS_PCPU_GET(sc, message_tq, cpu) = NULL;
		}
	}
}

static int
vmbus_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct hv_device *child_dev_ctx = device_get_ivars(child);

	switch (index) {
	case HV_VMBUS_IVAR_TYPE:
		*result = (uintptr_t)&child_dev_ctx->class_id;
		return (0);

	case HV_VMBUS_IVAR_INSTANCE:
		*result = (uintptr_t)&child_dev_ctx->device_id;
		return (0);

	case HV_VMBUS_IVAR_DEVCTX:
		*result = (uintptr_t)child_dev_ctx;
		return (0);

	case HV_VMBUS_IVAR_NODE:
		*result = (uintptr_t)child_dev_ctx->device;
		return (0);
	}
	return (ENOENT);
}

static int
vmbus_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
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
	struct hv_device *dev_ctx = device_get_ivars(child);
	char guidbuf[HYPERV_GUID_STRLEN];

	if (dev_ctx == NULL)
		return (0);

	strlcat(buf, "classid=", buflen);
	hyperv_guid2str(&dev_ctx->class_id, guidbuf, sizeof(guidbuf));
	strlcat(buf, guidbuf, buflen);

	strlcat(buf, " deviceid=", buflen);
	hyperv_guid2str(&dev_ctx->device_id, guidbuf, sizeof(guidbuf));
	strlcat(buf, guidbuf, buflen);

	return (0);
}

struct hv_device *
hv_vmbus_child_device_create(hv_guid type, hv_guid instance,
    hv_vmbus_channel *channel)
{
	hv_device *child_dev;

	/*
	 * Allocate the new child device
	 */
	child_dev = malloc(sizeof(hv_device), M_DEVBUF, M_WAITOK | M_ZERO);

	child_dev->channel = channel;
	memcpy(&child_dev->class_id, &type, sizeof(hv_guid));
	memcpy(&child_dev->device_id, &instance, sizeof(hv_guid));

	return (child_dev);
}

int
hv_vmbus_child_device_register(struct hv_device *child_dev)
{
	device_t child, parent;

	parent = vmbus_get_device();
	if (bootverbose) {
		char name[HYPERV_GUID_STRLEN];

		hyperv_guid2str(&child_dev->class_id, name, sizeof(name));
		device_printf(parent, "add device, classid: %s\n", name);
	}

	child = device_add_child(parent, NULL, -1);
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
	ret = device_delete_child(vmbus_get_device(), child_dev->device);
	mtx_unlock(&Giant);
	return(ret);
}

static int
vmbus_probe(device_t dev)
{
	char *id[] = { "VMBUS", NULL };

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, id) == NULL ||
	    device_get_unit(dev) != 0 || vm_guest != VM_GUEST_HV ||
	    (hyperv_features & CPUID_HV_MSR_SYNIC) == 0)
		return (ENXIO);

	device_set_desc(dev, "Hyper-V Vmbus");

	return (BUS_PROBE_DEFAULT);
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
	struct vmbus_softc *sc = vmbus_get_softc();
	int ret;

	if (sc->vmbus_flags & VMBUS_FLAG_ATTACHED)
		return (0);
	sc->vmbus_flags |= VMBUS_FLAG_ATTACHED;

	/*
	 * Allocate DMA stuffs.
	 */
	ret = vmbus_dma_alloc(sc);
	if (ret != 0)
		goto cleanup;

	/*
	 * Setup interrupt.
	 */
	ret = vmbus_intr_setup(sc);
	if (ret != 0)
		goto cleanup;

	/*
	 * Setup SynIC.
	 */
	if (bootverbose)
		device_printf(sc->vmbus_dev, "smp_started = %d\n", smp_started);
	smp_rendezvous(NULL, vmbus_synic_setup, NULL, sc);
	sc->vmbus_flags |= VMBUS_FLAG_SYNIC;

	/*
	 * Connect to VMBus in the root partition
	 */
	ret = hv_vmbus_connect(sc);
	if (ret != 0)
		goto cleanup;

	if (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008 ||
	    hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)
		sc->vmbus_event_proc = vmbus_event_proc_compat;
	else
		sc->vmbus_event_proc = vmbus_event_proc;

	hv_vmbus_request_channel_offers();

	vmbus_scan();
	bus_generic_attach(sc->vmbus_dev);
	device_printf(sc->vmbus_dev, "device scan, probe and attach done\n");

	return (ret);

cleanup:
	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);

	return (ret);
}

static void
vmbus_event_proc_dummy(struct vmbus_softc *sc __unused, int cpu __unused)
{
}

static int
vmbus_attach(device_t dev)
{
	vmbus_sc = device_get_softc(dev);
	vmbus_sc->vmbus_dev = dev;

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
	struct vmbus_softc *sc = device_get_softc(dev);

	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	if (sc->vmbus_flags & VMBUS_FLAG_SYNIC) {
		sc->vmbus_flags &= ~VMBUS_FLAG_SYNIC;
		smp_rendezvous(NULL, vmbus_synic_teardown, NULL, NULL);
	}

	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);

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

