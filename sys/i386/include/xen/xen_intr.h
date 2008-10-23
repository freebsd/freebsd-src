/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*- */
#ifndef _XEN_INTR_H_
#define _XEN_INTR_H_

/*
* The flat IRQ space is divided into two regions:
*  1. A one-to-one mapping of real physical IRQs. This space is only used
*     if we have physical device-access privilege. This region is at the 
*     start of the IRQ space so that existing device drivers do not need
*     to be modified to translate physical IRQ numbers into our IRQ space.
*  3. A dynamic mapping of inter-domain and Xen-sourced virtual IRQs. These
*     are bound using the provided bind/unbind functions.
*
*
* $FreeBSD$
*/

#define PIRQ_BASE   0
#define NR_PIRQS  128

#define DYNIRQ_BASE (PIRQ_BASE + NR_PIRQS)
#define NR_DYNIRQS  128

#define NR_IRQS   (NR_PIRQS + NR_DYNIRQS)

#define pirq_to_irq(_x)   ((_x) + PIRQ_BASE)
#define irq_to_pirq(_x)   ((_x) - PIRQ_BASE)

#define dynirq_to_irq(_x) ((_x) + DYNIRQ_BASE)
#define irq_to_dynirq(_x) ((_x) - DYNIRQ_BASE)

/* Dynamic binding of event channels and VIRQ sources to Linux IRQ space. */
extern void unbind_from_irq(int irq);

extern int bind_caller_port_to_irqhandler(unsigned int caller_port,
	const char *devname, driver_intr_t handler, void *arg,
	unsigned long irqflags, void **cookiep);
extern int bind_listening_port_to_irqhandler(unsigned int remote_domain,
	const char *devname, driver_intr_t handler, void *arg, unsigned long irqflags,
	void **cookiep);
extern int bind_virq_to_irqhandler(unsigned int virq, unsigned int cpu, const char *devname,
									 driver_filter_t filter, driver_intr_t handler, unsigned long irqflags);
extern int bind_ipi_to_irqhandler(unsigned int ipi,
	unsigned int cpu,
	const char *devname,
	driver_filter_t handler,
	unsigned long irqflags);

extern int bind_interdomain_evtchn_to_irqhandler(unsigned int remote_domain,
	                                             unsigned int remote_port,
	                                             const char *devname,
	                                             driver_filter_t filter,
	                                             driver_intr_t handler,
	                                             unsigned long irqflags);



extern void unbind_from_irqhandler(unsigned int evtchn, void *dev_id);
static __inline__ int irq_cannonicalize(int irq)
{
    return (irq == 2) ? 9 : irq;
}

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

extern void irq_suspend(void);
extern void irq_resume(void);

extern void	idle_block(void);
extern int	ap_cpu_initclocks(int cpu);

#endif /* _XEN_INTR_H_ */
