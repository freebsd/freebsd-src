/*
 * $FreeBSD$
 */
#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

#define	IPI_AST			ia64_ipi_ast
#define	IPI_PREEMPT		ia64_ipi_preempt
#define	IPI_RENDEZVOUS		ia64_ipi_rndzvs
#define	IPI_STOP		ia64_ipi_stop
#define	IPI_STOP_HARD		ia64_ipi_nmi

#ifndef LOCORE

#include <sys/_cpuset.h>

struct pcpu;

struct ia64_ap_state {
	uint64_t	as_trace;
	uint64_t	as_pgtbl_pte;
	uint64_t	as_pgtbl_itir;
	uint64_t	as_text_va;
	uint64_t	as_text_pte;
	uint64_t	as_text_itir;
	uint64_t	as_data_va;
	uint64_t	as_data_pte;
	uint64_t	as_data_itir;
	void		*as_kstack;
	void		*as_kstack_top;
	struct pcpu	*as_pcpu;
	volatile int	as_delay;
	volatile u_int	as_awake;
	volatile u_int	as_spin;
};

extern int ia64_ipi_ast;
extern int ia64_ipi_highfp;
extern int ia64_ipi_nmi;
extern int ia64_ipi_preempt;
extern int ia64_ipi_rndzvs;
extern int ia64_ipi_stop;
extern int ia64_ipi_wakeup;

void	ipi_all_but_self(int ipi);
void	ipi_cpu(int cpu, u_int ipi);
void	ipi_selected(cpuset_t cpus, int ipi);
void	ipi_send(struct pcpu *, int ipi);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* !_MACHINE_SMP_H */
