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

/* 
 * Dynamic binding of event channels and VIRQ sources to guest IRQ space.
 */

/*
 * Bind a caller port event channel to an interrupt handler. If
 * successful, the guest IRQ number is returned in *irqp. Return zero
 * on success or errno otherwise.
 */
extern int bind_caller_port_to_irqhandler(unsigned int caller_port,
	const char *devname, driver_intr_t handler, void *arg,
	unsigned long irqflags, unsigned int *irqp);

/*
 * Bind a listening port to an interrupt handler. If successful, the
 * guest IRQ number is returned in *irqp. Return zero on success or
 * errno otherwise.
 */
extern int bind_listening_port_to_irqhandler(unsigned int remote_domain,
	const char *devname, driver_intr_t handler, void *arg,
	unsigned long irqflags, unsigned int *irqp);

/*
 * Bind a VIRQ to an interrupt handler. If successful, the guest IRQ
 * number is returned in *irqp. Return zero on success or errno
 * otherwise.
 */
extern int bind_virq_to_irqhandler(unsigned int virq, unsigned int cpu,
	const char *devname, driver_filter_t filter, driver_intr_t handler,
	void *arg, unsigned long irqflags,	unsigned int *irqp);

/*
 * Bind an IPI to an interrupt handler. If successful, the guest
 * IRQ number is returned in *irqp. Return zero on success or errno
 * otherwise.
 */
extern int bind_ipi_to_irqhandler(unsigned int ipi, unsigned int cpu,
	const char *devname, driver_filter_t filter,
	unsigned long irqflags, unsigned int *irqp);

/*
 * Bind an interdomain event channel to an interrupt handler. If
 * successful, the guest IRQ number is returned in *irqp. Return zero
 * on success or errno otherwise.
 */
extern int bind_interdomain_evtchn_to_irqhandler(unsigned int remote_domain,
	unsigned int remote_port, const char *devname,
	driver_intr_t handler, void *arg,
	unsigned long irqflags, unsigned int *irqp);

/*
 * Unbind an interrupt handler using the guest IRQ number returned
 * when it was bound.
 */
extern void unbind_from_irqhandler(unsigned int irq);

static __inline__ int irq_cannonicalize(unsigned int irq)
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
