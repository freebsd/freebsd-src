/*
 * $FreeBSD$
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

/* global data in mp_machdep.c */
extern volatile u_int		checkstate_probed_cpus;
extern volatile u_int		checkstate_need_ast;
extern volatile u_int		resched_cpus;

void	ipi_selected(u_int cpus, u_int ipi);
void	ipi_all(u_int ipi);
void	ipi_all_but_self(u_int ipi);
void	ipi_self(u_int ipi);
void	smp_init_secondary(void);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* !_MACHINE_SMP_H */
