/*
 * $FreeBSD$
 */
#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

/*
 * Interprocessor interrupts for SMP. The following values are indices
 * into the IPI vector table. The SAL gives us the vector used for AP
 * wake-up. We base the other vectors on that. Keep IPI_AP_WAKEUP at
 * index 0 and IPI_MCA_RENDEZ at index 1. See sal.c for details.
 */
/* Architecture specific IPIs. */
#define	IPI_AP_WAKEUP		0
#define	IPI_HIGH_FP		1
#define	IPI_MCA_CMCV		2
#define	IPI_MCA_RENDEZ		3
#define	IPI_TEST		4
/* Machine independent IPIs. */
#define	IPI_AST			5
#define	IPI_RENDEZVOUS		6
#define	IPI_STOP		7

#define	IPI_COUNT		8

#ifndef LOCORE

extern int ipi_vector[];

void	ipi_all(int ipi);
void	ipi_all_but_self(int ipi);
void	ipi_selected(u_int64_t cpus, int ipi);
void	ipi_self(int ipi);
void	ipi_send(u_int64_t lid, int ipi);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* !_MACHINE_SMP_H */
