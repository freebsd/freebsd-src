/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

/*
 * Interprocessor interrupts for SMP.
 */
#define	IPI_INVLTLB		0x0001
#define	IPI_RENDEZVOUS		0x0002
#define	IPI_AST			0x0004
#define	IPI_CHECKSTATE		0x0008
#define	IPI_STOP		0x0010

#ifndef LOCORE

extern u_int64_t		boot_cpu_id;

void	ipi_selected(u_int cpus, u_int64_t ipi);
void	ipi_all(u_int64_t ipi);
void	ipi_all_but_self(u_int64_t ipi);
void	ipi_self(u_int64_t ipi);
void	smp_handle_ipi(struct trapframe *frame);
void	smp_init_secondary(void);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
