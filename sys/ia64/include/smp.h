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
 * index 0.
 */
#define	IPI_AP_WAKEUP		0		/* ia64 specific */
#define	IPI_AST			1
#define	IPI_RENDEZVOUS		2
#define	IPI_STOP		3
#define	IPI_TEST		4		/* ia64 specific */

#define	IPI_COUNT		5

#ifndef LOCORE

extern int mp_hardware;
extern int mp_ipi_vector[];

void	ipi_all(int ipi);
void	ipi_all_but_self(int ipi);
void	ipi_selected(u_int64_t cpus, int ipi);
void	ipi_self(int ipi);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* !_MACHINE_SMP_H */
