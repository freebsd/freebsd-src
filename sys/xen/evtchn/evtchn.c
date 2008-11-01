/******************************************************************************
 * evtchn.c
 * 
 * Communication via Xen event channels.
 * 
 * Copyright (c) 2002-2005, K A Fraser
 * Copyright (c) 2005-2006 Kip Macy
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>

#include <machine/cpufunc.h>
#include <machine/intr_machdep.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xen_intr.h>
#include <machine/xen/synch_bitops.h>
#include <machine/xen/evtchn.h>
#include <machine/xen/hypervisor.h>
#include <sys/smp.h>



/* linux helper functions that got sucked in 
 * rename and move XXX
 */


static inline int find_first_bit(const unsigned long *addr, unsigned size)
{
	int d0, d1;
	int res;

	/* This looks at memory. Mark it volatile to tell gcc not to move it around */
	__asm__ __volatile__(
		"xorl %%eax,%%eax\n\t"
		"repe; scasl\n\t"
		"jz 1f\n\t"
		"leal -4(%%edi),%%edi\n\t"
		"bsfl (%%edi),%%eax\n"
		"1:\tsubl %%ebx,%%edi\n\t"
		"shll $3,%%edi\n\t"
		"addl %%edi,%%eax"
		:"=a" (res), "=&c" (d0), "=&D" (d1)
		:"1" ((size + 31) >> 5), "2" (addr), "b" (addr) : "memory");
	return res;
}

#define min_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define first_cpu(src) __first_cpu(&(src), NR_CPUS)
static inline int __first_cpu(const xen_cpumask_t *srcp, int nbits)
{
	return min_t(int, nbits, find_first_bit(srcp->bits, nbits));
}

static inline unsigned long __ffs(unsigned long word)
{
        __asm__("bsfl %1,%0"
                :"=r" (word)
                :"rm" (word));
        return word;
}

static struct mtx irq_mapping_update_lock;
static struct xenpic *xp;
struct xenpic_intsrc {
	struct intsrc     xp_intsrc;
	uint8_t           xp_vector;
	boolean_t	  xp_masked;
};

struct xenpic { 
	struct pic           *xp_dynirq_pic; 
	struct pic           *xp_pirq_pic;   
	uint16_t             xp_numintr; 
	struct xenpic_intsrc xp_pins[0]; 
}; 

#define TODO            printf("%s: not implemented!\n", __func__) 

/* IRQ <-> event-channel mappings. */
static int evtchn_to_irq[NR_EVENT_CHANNELS];

/* Packed IRQ information: binding type, sub-type index, and event channel. */
static uint32_t irq_info[NR_IRQS];
/* Binding types. */
enum {
	IRQT_UNBOUND,
	IRQT_PIRQ,
	IRQT_VIRQ,
	IRQT_IPI,
	IRQT_LOCAL_PORT,
	IRQT_CALLER_PORT,
	_IRQT_COUNT
	
};


#define _IRQT_BITS 4
#define _EVTCHN_BITS 12
#define _INDEX_BITS (32 - _IRQT_BITS - _EVTCHN_BITS)

/* Constructor for packed IRQ information. */
static inline uint32_t
mk_irq_info(uint32_t type, uint32_t index, uint32_t evtchn)
{

	return ((type << (32 - _IRQT_BITS)) | (index << _EVTCHN_BITS) | evtchn);
}

/* Constructor for packed IRQ information. */

/* Convenient shorthand for packed representation of an unbound IRQ. */
#define IRQ_UNBOUND	mk_irq_info(IRQT_UNBOUND, 0, 0)

/*
 * Accessors for packed IRQ information.
 */

static inline unsigned int evtchn_from_irq(int irq)
{
	return irq_info[irq] & ((1U << _EVTCHN_BITS) - 1);
}

static inline unsigned int index_from_irq(int irq)
{
	return (irq_info[irq] >> _EVTCHN_BITS) & ((1U << _INDEX_BITS) - 1);
}

static inline unsigned int type_from_irq(int irq)
{
	return irq_info[irq] >> (32 - _IRQT_BITS);
}


/* IRQ <-> VIRQ mapping. */ 
 
/* IRQ <-> IPI mapping. */ 
#ifndef NR_IPIS
#ifdef SMP
#error "NR_IPIS not defined"
#endif
#define NR_IPIS 1 
#endif 

/* Bitmap indicating which PIRQs require Xen to be notified on unmask. */
static unsigned long pirq_needs_unmask_notify[NR_PIRQS/sizeof(unsigned long)];

/* Reference counts for bindings to IRQs. */
static int irq_bindcount[NR_IRQS];

#define VALID_EVTCHN(_chn) ((_chn) != 0)

#ifdef SMP

static uint8_t cpu_evtchn[NR_EVENT_CHANNELS];
static unsigned long cpu_evtchn_mask[NR_CPUS][NR_EVENT_CHANNELS/BITS_PER_LONG];

#define active_evtchns(cpu,sh,idx)		\
	((sh)->evtchn_pending[idx] &		\
	 cpu_evtchn_mask[cpu][idx] &		\
	 ~(sh)->evtchn_mask[idx])

static void bind_evtchn_to_cpu(unsigned int chn, unsigned int cpu)
{
	clear_bit(chn, (unsigned long *)cpu_evtchn_mask[cpu_evtchn[chn]]);
	set_bit(chn, (unsigned long *)cpu_evtchn_mask[cpu]);
	cpu_evtchn[chn] = cpu;
}

static void init_evtchn_cpu_bindings(void)
{
	/* By default all event channels notify CPU#0. */
	memset(cpu_evtchn, 0, sizeof(cpu_evtchn));
	memset(cpu_evtchn_mask[0], ~0, sizeof(cpu_evtchn_mask[0]));
}

#define cpu_from_evtchn(evtchn)		(cpu_evtchn[evtchn])

#else

#define active_evtchns(cpu,sh,idx)		\
	((sh)->evtchn_pending[idx] &		\
	 ~(sh)->evtchn_mask[idx])
#define bind_evtchn_to_cpu(chn,cpu)	((void)0)
#define init_evtchn_cpu_bindings()	((void)0)
#define cpu_from_evtchn(evtchn)		(0)

#endif


/*
 * Force a proper event-channel callback from Xen after clearing the
 * callback mask. We do this in a very simple manner, by making a call
 * down into Xen. The pending flag will be checked by Xen on return.
 */
void force_evtchn_callback(void)
{
	(void)HYPERVISOR_xen_version(0, NULL);
}

void 
evtchn_do_upcall(struct trapframe *frame) 
{
	unsigned long  l1, l2;
	unsigned int   l1i, l2i, port;
	int            irq, cpu;
	shared_info_t *s;
	vcpu_info_t   *vcpu_info;
	
	cpu = smp_processor_id();
	s = HYPERVISOR_shared_info;
	vcpu_info = &s->vcpu_info[cpu];

	vcpu_info->evtchn_upcall_pending = 0;

	/* NB. No need for a barrier here -- XCHG is a barrier on x86. */
	l1 = xen_xchg(&vcpu_info->evtchn_pending_sel, 0);

	while (l1 != 0) {
		l1i = __ffs(l1);
		l1 &= ~(1 << l1i);
		
		while ((l2 = active_evtchns(cpu, s, l1i)) != 0) {
			l2i = __ffs(l2);

			port = (l1i * BITS_PER_LONG) + l2i;
			if ((irq = evtchn_to_irq[port]) != -1) {
				struct intsrc *isrc = intr_lookup_source(irq);
				/* 
				 * ack 
				 */
				mask_evtchn(port);
				clear_evtchn(port); 

				intr_execute_handlers(isrc, frame);
			} else {
				evtchn_device_upcall(port);
			}
		}
	}
}

void
ipi_pcpu(unsigned int cpu, int vector) 
{ 
        int irq;

	irq = per_cpu(ipi_to_irq, cpu)[vector]; 
	
        notify_remote_via_irq(irq); 
} 

static int 
find_unbound_irq(void)
{
	int dynirq, irq;
	
	for (dynirq = 0; dynirq < NR_IRQS; dynirq++) {
		irq = dynirq_to_irq(dynirq);
		if (irq_bindcount[irq] == 0)
			break;
	}
	
	if (irq == NR_IRQS)
		panic("No available IRQ to bind to: increase NR_IRQS!\n");

	return (irq);
}

static int
bind_caller_port_to_irq(unsigned int caller_port)
{
        int irq;

        mtx_lock_spin(&irq_mapping_update_lock);

        if ((irq = evtchn_to_irq[caller_port]) == -1) {
                if ((irq = find_unbound_irq()) < 0)
                        goto out;

                evtchn_to_irq[caller_port] = irq;
                irq_info[irq] = mk_irq_info(IRQT_CALLER_PORT, 0, caller_port);
        }

        irq_bindcount[irq]++;

 out:
        mtx_unlock_spin(&irq_mapping_update_lock);
        return irq;
}

static int
bind_local_port_to_irq(unsigned int local_port)
{
        int irq;

        mtx_lock_spin(&irq_mapping_update_lock);

        PANIC_IF(evtchn_to_irq[local_port] != -1);

        if ((irq = find_unbound_irq()) < 0) {
                struct evtchn_close close = { .port = local_port };
                PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_close, &close));
		
                goto out;
        }

        evtchn_to_irq[local_port] = irq;
        irq_info[irq] = mk_irq_info(IRQT_LOCAL_PORT, 0, local_port);
        irq_bindcount[irq]++;

 out:
        mtx_unlock_spin(&irq_mapping_update_lock);
        return irq;
}

static int
bind_listening_port_to_irq(unsigned int remote_domain)
{
        struct evtchn_alloc_unbound alloc_unbound;
        int err;

        alloc_unbound.dom        = DOMID_SELF;
        alloc_unbound.remote_dom = remote_domain;

        err = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
                                          &alloc_unbound);

        return err ? : bind_local_port_to_irq(alloc_unbound.port);
}

static int
bind_interdomain_evtchn_to_irq(unsigned int remote_domain,
    unsigned int remote_port)
{
        struct evtchn_bind_interdomain bind_interdomain;
        int err;

        bind_interdomain.remote_dom  = remote_domain;
        bind_interdomain.remote_port = remote_port;

        err = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
                                          &bind_interdomain);

        return err ? : bind_local_port_to_irq(bind_interdomain.local_port);
}

static int 
bind_virq_to_irq(unsigned int virq, unsigned int cpu)
{
	struct evtchn_bind_virq bind_virq;
	int evtchn, irq;

	mtx_lock_spin(&irq_mapping_update_lock);

	if ((irq = per_cpu(virq_to_irq, cpu)[virq]) == -1) {
		if ((irq = find_unbound_irq()) < 0)
			goto out;

		bind_virq.virq = virq;
		bind_virq.vcpu = cpu;
		PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq,
			&bind_virq) != 0);

		evtchn = bind_virq.port;

		evtchn_to_irq[evtchn] = irq;
		irq_info[irq] = mk_irq_info(IRQT_VIRQ, virq, evtchn);

		per_cpu(virq_to_irq, cpu)[virq] = irq;

		bind_evtchn_to_cpu(evtchn, cpu);
	}

	irq_bindcount[irq]++;
out:
	mtx_unlock_spin(&irq_mapping_update_lock);

	return irq;
}


extern int bind_ipi_to_irq(unsigned int ipi, unsigned int cpu);

int 
bind_ipi_to_irq(unsigned int ipi, unsigned int cpu)
{
	struct evtchn_bind_ipi bind_ipi;
	int evtchn, irq;
	
	mtx_lock_spin(&irq_mapping_update_lock);
	
	if ((irq = per_cpu(ipi_to_irq, cpu)[ipi]) == -1) {
		if ((irq = find_unbound_irq()) < 0)
			goto out;

		bind_ipi.vcpu = cpu;
		PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi, &bind_ipi) != 0);
		evtchn = bind_ipi.port;

		evtchn_to_irq[evtchn] = irq;
		irq_info[irq] = mk_irq_info(IRQT_IPI, ipi, evtchn);

		per_cpu(ipi_to_irq, cpu)[ipi] = irq;

		bind_evtchn_to_cpu(evtchn, cpu);
	}
	irq_bindcount[irq]++;
out:
	
	mtx_unlock_spin(&irq_mapping_update_lock);

	return irq;
}


void 
unbind_from_irq(int irq)
{
	struct evtchn_close close;
	int evtchn = evtchn_from_irq(irq);

	mtx_lock_spin(&irq_mapping_update_lock);

	if ((--irq_bindcount[irq] == 0) && VALID_EVTCHN(evtchn)) {
		close.port = evtchn;
		PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_close, &close) != 0);

		switch (type_from_irq(irq)) {
		case IRQT_VIRQ:
			per_cpu(virq_to_irq, cpu_from_evtchn(evtchn))[index_from_irq(irq)] = -1;
			break;
		case IRQT_IPI:
			per_cpu(ipi_to_irq, cpu_from_evtchn(evtchn))[index_from_irq(irq)] = -1;
			break;
		default:
			break;
		}

		/* Closed ports are implicitly re-bound to VCPU0. */
		bind_evtchn_to_cpu(evtchn, 0);

		evtchn_to_irq[evtchn] = -1;
		irq_info[irq] = IRQ_UNBOUND;
	}

	mtx_unlock_spin(&irq_mapping_update_lock);
}

int 
bind_caller_port_to_irqhandler(unsigned int caller_port,
			  const char *devname,
			  driver_intr_t handler,
			  void *arg,
			  unsigned long irqflags,
                          void **cookiep)
{
	unsigned int irq;
	int retval;

	irq = bind_caller_port_to_irq(caller_port);
	intr_register_source(&xp->xp_pins[irq].xp_intsrc);
	retval = intr_add_handler(devname, irq, NULL, handler, arg, irqflags, cookiep);
	if (retval != 0) {
		unbind_from_irq(irq);
		return -retval;
	}

	return irq;
}

int 
bind_listening_port_to_irqhandler(
	                  unsigned int remote_domain,
			  const char *devname,
			  driver_intr_t handler,
			  void *arg,
			  unsigned long irqflags,
                          void **cookiep)
{
	unsigned int irq;
	int retval;

	irq = bind_listening_port_to_irq(remote_domain);
	intr_register_source(&xp->xp_pins[irq].xp_intsrc);
	retval = intr_add_handler(devname, irq, NULL, handler, arg, irqflags, cookiep);
	if (retval != 0) {
		unbind_from_irq(irq);
		return -retval;
	}

	return irq;
}

int 
bind_interdomain_evtchn_to_irqhandler(
	                unsigned int remote_domain,
	                unsigned int remote_port,
			const char *devname,
			driver_filter_t filter,
			driver_intr_t handler,
			unsigned long irqflags)
{
	unsigned int irq;
	int retval;

	irq = bind_interdomain_evtchn_to_irq(remote_domain, remote_port);
	intr_register_source(&xp->xp_pins[irq].xp_intsrc);
	retval = intr_add_handler(devname, irq, filter, handler, NULL, irqflags, NULL);
	if (retval != 0) {
		unbind_from_irq(irq);
		return -retval;
	}

	return irq;
}

int 
bind_virq_to_irqhandler(unsigned int virq,
			unsigned int cpu,
			const char *devname,
			driver_filter_t filter,
			driver_intr_t handler,
			unsigned long irqflags)
{
	unsigned int irq;
	int retval;

	irq = bind_virq_to_irq(virq, cpu);
	intr_register_source(&xp->xp_pins[irq].xp_intsrc);
	retval = intr_add_handler(devname, irq, filter, handler, NULL, irqflags, NULL);
	if (retval != 0) {
		unbind_from_irq(irq);
		return -retval;
	}

	return irq;
}

int 
bind_ipi_to_irqhandler(unsigned int ipi,
		       unsigned int cpu,
		       const char *devname,
		       driver_filter_t filter,
		       unsigned long irqflags)
{
	int irq, retval;
	
	irq = bind_ipi_to_irq(ipi, cpu);
	intr_register_source(&xp->xp_pins[irq].xp_intsrc);
	retval = intr_add_handler(devname, irq, filter, NULL,
	    NULL, irqflags, NULL);
	if (retval != 0) {
		unbind_from_irq(irq);
		return -retval;
	}

	return irq;
}

void
unbind_from_irqhandler(unsigned int irq, void *dev_id)
{
	if (dev_id)
		intr_remove_handler(dev_id); /* XXX */
	unbind_from_irq(irq);
}

#if 0
/* Rebind an evtchn so that it gets delivered to a specific cpu */
static void
rebind_irq_to_cpu(unsigned irq, unsigned tcpu)
{
	evtchn_op_t op = { .cmd = EVTCHNOP_bind_vcpu };
	int evtchn;

	mtx_lock_spin(&irq_mapping_update_lock);

	evtchn = evtchn_from_irq(irq);
	if (!VALID_EVTCHN(evtchn)) {
		mtx_unlock_spin(&irq_mapping_update_lock);
		return;
	}

	/* Send future instances of this interrupt to other vcpu. */
	bind_vcpu.port = evtchn;
	bind_vcpu.vcpu = tcpu;

	/*
	 * If this fails, it usually just indicates that we're dealing with a 
	 * virq or IPI channel, which don't actually need to be rebound. Ignore
	 * it, but don't do the xenlinux-level rebind in that case.
	 */
	if (HYPERVISOR_event_channel_op(&op) >= 0)
		bind_evtchn_to_cpu(evtchn, tcpu);

	mtx_unlock_spin(&irq_mapping_update_lock);

}

static void set_affinity_irq(unsigned irq, xen_cpumask_t dest)
{
	unsigned tcpu = first_cpu(dest);
	rebind_irq_to_cpu(irq, tcpu);
}
#endif

/*
 * Interface to generic handling in intr_machdep.c
 */


/*------------ interrupt handling --------------------------------------*/
#define TODO            printf("%s: not implemented!\n", __func__) 


static void     xenpic_dynirq_enable_source(struct intsrc *isrc); 
static void     xenpic_dynirq_disable_source(struct intsrc *isrc, int); 
static void     xenpic_dynirq_eoi_source(struct intsrc *isrc); 
static void     xenpic_dynirq_enable_intr(struct intsrc *isrc); 
static void     xenpic_dynirq_disable_intr(struct intsrc *isrc); 

static void     xenpic_pirq_enable_source(struct intsrc *isrc); 
static void     xenpic_pirq_disable_source(struct intsrc *isrc, int); 
static void     xenpic_pirq_eoi_source(struct intsrc *isrc); 
static void     xenpic_pirq_enable_intr(struct intsrc *isrc); 
static void     xenpic_pirq_disable_intr(struct intsrc *isrc); 


static int      xenpic_vector(struct intsrc *isrc); 
static int      xenpic_source_pending(struct intsrc *isrc); 
static void     xenpic_suspend(struct pic* pic); 
static void     xenpic_resume(struct pic* pic); 
static void     xenpic_assign_cpu(struct intsrc *, u_int apic_id);


struct pic xenpic_dynirq_template  =  { 
	.pic_enable_source	=	xenpic_dynirq_enable_source, 
	.pic_disable_source	=	xenpic_dynirq_disable_source,
	.pic_eoi_source		=	xenpic_dynirq_eoi_source, 
	.pic_enable_intr	=	xenpic_dynirq_enable_intr, 
	.pic_disable_intr	=	xenpic_dynirq_disable_intr, 
	.pic_vector		=	xenpic_vector, 
	.pic_source_pending	=	xenpic_source_pending,
	.pic_suspend		=	xenpic_suspend, 
	.pic_resume		=	xenpic_resume 
};

struct pic xenpic_pirq_template  =  { 
	.pic_enable_source	=	xenpic_pirq_enable_source, 
	.pic_disable_source	=	xenpic_pirq_disable_source,
	.pic_eoi_source		=	xenpic_pirq_eoi_source, 
	.pic_enable_intr	=	xenpic_pirq_enable_intr, 
	.pic_disable_intr	=	xenpic_pirq_disable_intr, 
	.pic_vector		=	xenpic_vector, 
	.pic_source_pending	=	xenpic_source_pending,
	.pic_suspend		=	xenpic_suspend, 
	.pic_resume		=	xenpic_resume,
	.pic_assign_cpu		=	xenpic_assign_cpu
};



void 
xenpic_dynirq_enable_source(struct intsrc *isrc)
{
	unsigned int irq;
	struct xenpic_intsrc *xp;

	xp = (struct xenpic_intsrc *)isrc;
	
	mtx_lock_spin(&irq_mapping_update_lock);
	if (xp->xp_masked) {
		irq = xenpic_vector(isrc);
		unmask_evtchn(evtchn_from_irq(irq));
		xp->xp_masked = FALSE;
	}
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_dynirq_disable_source(struct intsrc *isrc, int foo)
{
	unsigned int irq;
	struct xenpic_intsrc *xp;
	
	xp = (struct xenpic_intsrc *)isrc;
	
	mtx_lock_spin(&irq_mapping_update_lock);
	if (!xp->xp_masked) {
		irq = xenpic_vector(isrc);
		mask_evtchn(evtchn_from_irq(irq));
		xp->xp_masked = TRUE;
	}	
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_dynirq_enable_intr(struct intsrc *isrc)
{
	unsigned int irq;
	struct xenpic_intsrc *xp;
	
	xp = (struct xenpic_intsrc *)isrc;	
	mtx_lock_spin(&irq_mapping_update_lock);
	xp->xp_masked = 0;
	irq = xenpic_vector(isrc);
	unmask_evtchn(evtchn_from_irq(irq));
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_dynirq_disable_intr(struct intsrc *isrc)
{
	unsigned int irq;
	struct xenpic_intsrc *xp;
	
	xp = (struct xenpic_intsrc *)isrc;	
	mtx_lock_spin(&irq_mapping_update_lock);
	xp->xp_masked = 1;
	irq = xenpic_vector(isrc);
	mask_evtchn(evtchn_from_irq(irq));
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_dynirq_eoi_source(struct intsrc *isrc)
{
	unsigned int irq;
	struct xenpic_intsrc *xp;
	
	xp = (struct xenpic_intsrc *)isrc;	
	mtx_lock_spin(&irq_mapping_update_lock);
	xp->xp_masked = 0;
	irq = xenpic_vector(isrc);
	unmask_evtchn(evtchn_from_irq(irq));
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static int
xenpic_vector(struct intsrc *isrc)
{
    struct xenpic_intsrc *pin;

    pin = (struct xenpic_intsrc *)isrc;
   //printf("xenpic_vector(): isrc=%p,vector=%u\n", pin, pin->xp_vector);

    return (pin->xp_vector);
}

static int
xenpic_source_pending(struct intsrc *isrc)
{
    struct xenpic_intsrc *pin = (struct xenpic_intsrc *)isrc;

	/* XXXEN: TODO */
	printf("xenpic_source_pending(): vector=%x,masked=%x\n",
	    pin->xp_vector, pin->xp_masked);

/*	notify_remote_via_evtchn(pin->xp_vector); // XXX RS: Is this correct? */
	return 0;
}

static void 
xenpic_suspend(struct pic* pic)
{ 
	TODO; 
} 
 
static void 
xenpic_resume(struct pic* pic)
{ 
	TODO; 
}

static void 
xenpic_assign_cpu(struct intsrc *isrc, u_int apic_id)
{ 
	TODO; 
}

void
notify_remote_via_irq(int irq)
{
	int evtchn = evtchn_from_irq(irq);

	if (VALID_EVTCHN(evtchn))
		notify_remote_via_evtchn(evtchn);
	else
		panic("invalid evtchn");
}

/* required for support of physical devices */
static inline void 
pirq_unmask_notify(int pirq)
{
	struct physdev_eoi eoi = { .irq = pirq };

	if (unlikely(test_bit(pirq, &pirq_needs_unmask_notify[0]))) {
		(void)HYPERVISOR_physdev_op(PHYSDEVOP_eoi, &eoi);
	}
}

static inline void 
pirq_query_unmask(int pirq)
{
	struct physdev_irq_status_query irq_status_query;

	irq_status_query.irq = pirq;
	(void)HYPERVISOR_physdev_op(PHYSDEVOP_IRQ_STATUS_QUERY, &irq_status_query);
	clear_bit(pirq, &pirq_needs_unmask_notify[0]);
	if ( irq_status_query.flags & PHYSDEVOP_IRQ_NEEDS_UNMASK_NOTIFY )
		set_bit(pirq, &pirq_needs_unmask_notify[0]);
}

/*
 * On startup, if there is no action associated with the IRQ then we are
 * probing. In this case we should not share with others as it will confuse us.
 */
#define probing_irq(_irq) (intr_lookup_source(irq) == NULL)

static void 
xenpic_pirq_enable_intr(struct intsrc *isrc)
{
	struct evtchn_bind_pirq bind_pirq;
	int evtchn;
	unsigned int irq;
	
	mtx_lock_spin(&irq_mapping_update_lock);
	irq = xenpic_vector(isrc);
	evtchn = evtchn_from_irq(irq);

	if (VALID_EVTCHN(evtchn))
		goto out;

	bind_pirq.pirq  = irq;
	/* NB. We are happy to share unless we are probing. */
	bind_pirq.flags = probing_irq(irq) ? 0 : BIND_PIRQ__WILL_SHARE;
	
	if (HYPERVISOR_event_channel_op(EVTCHNOP_bind_pirq, &bind_pirq) != 0) {
#ifndef XEN_PRIVILEGED_GUEST
		panic("unexpected pirq call");
#endif
		if (!probing_irq(irq)) /* Some failures are expected when probing. */
			printf("Failed to obtain physical IRQ %d\n", irq);
		mtx_unlock_spin(&irq_mapping_update_lock);
		return;
	}
	evtchn = bind_pirq.port;

	pirq_query_unmask(irq_to_pirq(irq));

	bind_evtchn_to_cpu(evtchn, 0);
	evtchn_to_irq[evtchn] = irq;
	irq_info[irq] = mk_irq_info(IRQT_PIRQ, irq, evtchn);

 out:
	unmask_evtchn(evtchn);
	pirq_unmask_notify(irq_to_pirq(irq));
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_pirq_disable_intr(struct intsrc *isrc)
{
	unsigned int irq;
	int evtchn;
	struct evtchn_close close;
			
	mtx_lock_spin(&irq_mapping_update_lock);
	irq = xenpic_vector(isrc);
	evtchn = evtchn_from_irq(irq);

	if (!VALID_EVTCHN(evtchn)) 
		goto done;
	
	mask_evtchn(evtchn);

	close.port = evtchn;
	PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_close, &close) != 0);

	bind_evtchn_to_cpu(evtchn, 0);
	evtchn_to_irq[evtchn] = -1;
	irq_info[irq] = IRQ_UNBOUND;
 done:
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_pirq_enable_source(struct intsrc *isrc)
{
	int evtchn;
	unsigned int irq;

	mtx_lock_spin(&irq_mapping_update_lock);
	irq = xenpic_vector(isrc);
	evtchn = evtchn_from_irq(irq);

	if (!VALID_EVTCHN(evtchn))
		goto done;

	unmask_evtchn(evtchn);
	pirq_unmask_notify(irq_to_pirq(irq));
 done:
	mtx_unlock_spin(&irq_mapping_update_lock);
}

static void 
xenpic_pirq_disable_source(struct intsrc *isrc, int eoi)
{
	int evtchn;
	unsigned int irq;

	mtx_lock_spin(&irq_mapping_update_lock);
	irq = xenpic_vector(isrc);
	evtchn = evtchn_from_irq(irq);

	if (!VALID_EVTCHN(evtchn))
		goto done;

	mask_evtchn(evtchn);
 done:
	mtx_unlock_spin(&irq_mapping_update_lock);
}


static void 
xenpic_pirq_eoi_source(struct intsrc *isrc)
{
	int evtchn;
	unsigned int irq;

	mtx_lock_spin(&irq_mapping_update_lock);
	irq = xenpic_vector(isrc);
	evtchn = evtchn_from_irq(irq);

	if (!VALID_EVTCHN(evtchn))
		goto done;

	unmask_evtchn(evtchn);
	pirq_unmask_notify(irq_to_pirq(irq));
 done:
	mtx_unlock_spin(&irq_mapping_update_lock);
}

int
irq_to_evtchn_port(int irq)
{
	return evtchn_from_irq(irq);
}

void 
mask_evtchn(int port)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	synch_set_bit(port, &s->evtchn_mask[0]);
}

void 
unmask_evtchn(int port)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	unsigned int cpu = smp_processor_id();
	vcpu_info_t *vcpu_info = &s->vcpu_info[cpu];

	/* Slow path (hypercall) if this is a non-local port. */
	if (unlikely(cpu != cpu_from_evtchn(port))) {
		struct evtchn_unmask unmask = { .port = port };
		(void)HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &unmask);
		return;
	}

	synch_clear_bit(port, &s->evtchn_mask);

	/*
	 * The following is basically the equivalent of 'hw_resend_irq'. Just
	 * like a real IO-APIC we 'lose the interrupt edge' if the channel is
	 * masked.
	 */
	if (synch_test_bit(port, &s->evtchn_pending) && 
	    !synch_test_and_set_bit(port / BITS_PER_LONG,
				    &vcpu_info->evtchn_pending_sel)) {
		vcpu_info->evtchn_upcall_pending = 1;
		if (!vcpu_info->evtchn_upcall_mask)
			force_evtchn_callback();
	}
}

void irq_resume(void)
{
	evtchn_op_t op;
	int         cpu, pirq, virq, ipi, irq, evtchn;

	struct evtchn_bind_virq bind_virq;
	struct evtchn_bind_ipi bind_ipi;	

	init_evtchn_cpu_bindings();

	/* New event-channel space is not 'live' yet. */
	for (evtchn = 0; evtchn < NR_EVENT_CHANNELS; evtchn++)
		mask_evtchn(evtchn);

	/* Check that no PIRQs are still bound. */
	for (pirq = 0; pirq < NR_PIRQS; pirq++)
		PANIC_IF(irq_info[pirq_to_irq(pirq)] != IRQ_UNBOUND);

	/* Secondary CPUs must have no VIRQ or IPI bindings. */
	for (cpu = 1; cpu < NR_CPUS; cpu++) {
		for (virq = 0; virq < NR_VIRQS; virq++)
			PANIC_IF(per_cpu(virq_to_irq, cpu)[virq] != -1);
		for (ipi = 0; ipi < NR_IPIS; ipi++)
			PANIC_IF(per_cpu(ipi_to_irq, cpu)[ipi] != -1);
	}

	/* No IRQ <-> event-channel mappings. */
	for (irq = 0; irq < NR_IRQS; irq++)
		irq_info[irq] &= ~0xFFFF; /* zap event-channel binding */
	for (evtchn = 0; evtchn < NR_EVENT_CHANNELS; evtchn++)
		evtchn_to_irq[evtchn] = -1;

	/* Primary CPU: rebind VIRQs automatically. */
	for (virq = 0; virq < NR_VIRQS; virq++) {
		if ((irq = per_cpu(virq_to_irq, 0)[virq]) == -1)
			continue;

		PANIC_IF(irq_info[irq] != mk_irq_info(IRQT_VIRQ, virq, 0));

		/* Get a new binding from Xen. */
		bind_virq.virq = virq;
		bind_virq.vcpu = 0;
		PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq, &bind_virq) != 0);
		evtchn = bind_virq.port;
        
		/* Record the new mapping. */
		evtchn_to_irq[evtchn] = irq;
		irq_info[irq] = mk_irq_info(IRQT_VIRQ, virq, evtchn);

		/* Ready for use. */
		unmask_evtchn(evtchn);
	}

	/* Primary CPU: rebind IPIs automatically. */
	for (ipi = 0; ipi < NR_IPIS; ipi++) {
		if ((irq = per_cpu(ipi_to_irq, 0)[ipi]) == -1)
			continue;

		PANIC_IF(irq_info[irq] != mk_irq_info(IRQT_IPI, ipi, 0));

		/* Get a new binding from Xen. */
		memset(&op, 0, sizeof(op));
		bind_ipi.vcpu = 0;
		PANIC_IF(HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi, &bind_ipi) != 0);
		evtchn = bind_ipi.port;
        
		/* Record the new mapping. */
		evtchn_to_irq[evtchn] = irq;
		irq_info[irq] = mk_irq_info(IRQT_IPI, ipi, evtchn);

		/* Ready for use. */
		unmask_evtchn(evtchn);
	}
}

static void 
evtchn_init(void *dummy __unused)
{
	int i, cpu;
	struct xenpic_intsrc *pin, *tpin;


	init_evtchn_cpu_bindings();
	
         /* No VIRQ or IPI bindings. */
	for (cpu = 0; cpu < mp_ncpus; cpu++) {
		for (i = 0; i < NR_VIRQS; i++)
			per_cpu(virq_to_irq, cpu)[i] = -1;
		for (i = 0; i < NR_IPIS; i++)
			per_cpu(ipi_to_irq, cpu)[i] = -1;
	}

	/* No event-channel -> IRQ mappings. */
	for (i = 0; i < NR_EVENT_CHANNELS; i++) {
		evtchn_to_irq[i] = -1;
		mask_evtchn(i); /* No event channels are 'live' right now. */
	}

	/* No IRQ -> event-channel mappings. */
	for (i = 0; i < NR_IRQS; i++)
		irq_info[i] = IRQ_UNBOUND;
	
	xp = malloc(sizeof(struct xenpic) + NR_IRQS*sizeof(struct xenpic_intsrc), 
		    M_DEVBUF, M_WAITOK);

	xp->xp_dynirq_pic = &xenpic_dynirq_template;
	xp->xp_pirq_pic = &xenpic_pirq_template;
	xp->xp_numintr = NR_IRQS;
	bzero(xp->xp_pins, sizeof(struct xenpic_intsrc) * NR_IRQS);


	/* We need to register our PIC's beforehand */
	if (intr_register_pic(&xenpic_pirq_template))
		panic("XEN: intr_register_pic() failure");
	if (intr_register_pic(&xenpic_dynirq_template))
		panic("XEN: intr_register_pic() failure");

	/*
	 * Initialize the dynamic IRQ's - we initialize the structures, but
	 * we do not bind them (bind_evtchn_to_irqhandle() does this)
	 */
	pin = xp->xp_pins;
	for (i = 0; i < NR_DYNIRQS; i++) {
		/* Dynamic IRQ space is currently unbound. Zero the refcnts. */
		irq_bindcount[dynirq_to_irq(i)] = 0;

		tpin = &pin[dynirq_to_irq(i)];
		tpin->xp_intsrc.is_pic = xp->xp_dynirq_pic;
		tpin->xp_vector = dynirq_to_irq(i);
		
	}
	/*
	 * Now, we go ahead and claim every PIRQ there is.
	 */
	pin = xp->xp_pins;
	for (i = 0; i < NR_PIRQS; i++) {
		/* Dynamic IRQ space is currently unbound. Zero the refcnts. */
		irq_bindcount[pirq_to_irq(i)] = 0;

#ifdef RTC_IRQ
		/* If not domain 0, force our RTC driver to fail its probe. */
		if ((i == RTC_IRQ) &&
		    !(xen_start_info->flags & SIF_INITDOMAIN))
			continue;
#endif
		tpin = &pin[pirq_to_irq(i)];		
		tpin->xp_intsrc.is_pic = xp->xp_pirq_pic;
		tpin->xp_vector = pirq_to_irq(i);

	}
}

SYSINIT(evtchn_init, SI_SUB_INTR, SI_ORDER_MIDDLE, evtchn_init, NULL);
    /*
     * irq_mapping_update_lock: in order to allow an interrupt to occur in a critical
     * 	        section, to set pcpu->ipending (etc...) properly, we
     *	        must be able to get the icu lock, so it can't be
     *	        under witness.
     */

MTX_SYSINIT(irq_mapping_update_lock, &irq_mapping_update_lock, "xp", MTX_SPIN);
