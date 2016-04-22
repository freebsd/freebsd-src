/* $FreeBSD$ */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#include <sys/_cpuset.h>
#include <machine/pcb.h>

#ifdef INTRNG
enum {
	IPI_AST,
	IPI_PREEMPT,
	IPI_RENDEZVOUS,
	IPI_STOP,
	IPI_STOP_HARD = IPI_STOP, /* These are synonyms on arm. */
	IPI_HARDCLOCK,
	IPI_TLB,		/* Not used now, but keep it reserved. */
	IPI_CACHE,		/* Not used now, but keep it reserved. */
	INTR_IPI_COUNT
};
#else
#define IPI_AST		0
#define IPI_PREEMPT	2
#define IPI_RENDEZVOUS	3
#define IPI_STOP	4
#define IPI_STOP_HARD	4
#define IPI_HARDCLOCK	6
#define IPI_TLB		7	/* Not used now, but keep it reserved. */
#define IPI_CACHE	8	/* Not used now, but keep it reserved. */
#endif /* INTRNG */

void	init_secondary(int cpu);
void	mpentry(void);

void	ipi_all_but_self(u_int ipi);
void	ipi_cpu(int cpu, u_int ipi);
void	ipi_selected(cpuset_t cpus, u_int ipi);

/* PIC interface */
#ifndef INTRNG
void	pic_ipi_send(cpuset_t cpus, u_int ipi);
void	pic_ipi_clear(int ipi);
int	pic_ipi_read(int arg);
#endif

/* Platform interface */
void	platform_mp_setmaxid(void);
void	platform_mp_start_ap(void);

/* global data in mp_machdep.c */
extern struct pcb               stoppcbs[];

#endif /* !_MACHINE_SMP_H_ */
