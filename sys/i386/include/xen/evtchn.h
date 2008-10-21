/******************************************************************************
 * evtchn.h
 * 
 * Communication via Xen event channels.
 * Also definitions for the device that demuxes notifications to userspace.
 * 
 * Copyright (c) 2004, K A Fraser
 *
 * $FreeBSD$
 */

#ifndef __ASM_EVTCHN_H__
#define __ASM_EVTCHN_H__
#include <machine/pcpu.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/synch_bitops.h>
#include <machine/frame.h>

/*
 * LOW-LEVEL DEFINITIONS
 */

/*
 * Unlike notify_remote_via_evtchn(), this is safe to use across
 * save/restore. Notifications on a broken connection are silently dropped.
 */
void notify_remote_via_irq(int irq);


/* Entry point for notifications into Linux subsystems. */
void evtchn_do_upcall(struct trapframe *frame);

/* Entry point for notifications into the userland character device. */
void evtchn_device_upcall(int port);

void mask_evtchn(int port);

void unmask_evtchn(int port);

#ifdef SMP
void rebind_evtchn_to_cpu(int port, unsigned int cpu);
#else
#define rebind_evtchn_to_cpu(port, cpu)	((void)0)
#endif

static inline
int test_and_set_evtchn_mask(int port)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	return synch_test_and_set_bit(port, s->evtchn_mask);
}

static inline void 
clear_evtchn(int port)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	synch_clear_bit(port, &s->evtchn_pending[0]);
}

static inline void 
notify_remote_via_evtchn(int port)
{
        struct evtchn_send send = { .port = port };
        (void)HYPERVISOR_event_channel_op(EVTCHNOP_send, &send);
}

/*
 * Use these to access the event channel underlying the IRQ handle returned
 * by bind_*_to_irqhandler().
 */
int irq_to_evtchn_port(int irq);

void ipi_pcpu(unsigned int cpu, int vector);

/*
 * CHARACTER-DEVICE DEFINITIONS
 */

#define PORT_NORMAL    0x0000
#define PORT_EXCEPTION 0x8000
#define PORTIDX_MASK   0x7fff

/* /dev/xen/evtchn resides at device number major=10, minor=200 */
#define EVTCHN_MINOR 200

/* /dev/xen/evtchn ioctls: */
/* EVTCHN_RESET: Clear and reinit the event buffer. Clear error condition. */
#define EVTCHN_RESET  _IO('E', 1)
/* EVTCHN_BIND: Bind to the specified event-channel port. */
#define EVTCHN_BIND   _IO('E', 2)
/* EVTCHN_UNBIND: Unbind from the specified event-channel port. */
#define EVTCHN_UNBIND _IO('E', 3)

#endif /* __ASM_EVTCHN_H__ */
