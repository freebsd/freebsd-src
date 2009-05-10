/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 *	from: src/sys/alpha/include/smp.h,v 1.8 2005/01/05 20:05:50 imp
 *	JNPR: smp.h,v 1.3 2006/12/02 09:53:41 katta
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_SMP_H_
#define	_MACHINE_SMP_H_

#ifdef _KERNEL

/*
 * Interprocessor interrupts for SMP.
 */
#define	IPI_INVLTLB		0x0001
#define	IPI_RENDEZVOUS		0x0002
#define	IPI_AST			0x0004
#define	IPI_STOP		0x0008

#ifndef LOCORE

extern u_int32_t		boot_cpu_id;

void	ipi_selected(u_int cpus, u_int32_t ipi);
void	ipi_all_but_self(u_int32_t ipi);
intrmask_t	smp_handle_ipi(struct trapframe *frame);
void	smp_init_secondary(u_int32_t cpuid);
void	mips_ipi_send(int thread_id);

#endif /* !LOCORE */
#endif /* _KERNEL */

#endif /* _MACHINE_SMP_H_ */
