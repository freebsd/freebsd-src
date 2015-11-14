/* $FreeBSD$ */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#include <sys/_cpuset.h>
#include <machine/pcb.h>

#ifdef ARM_INTRNG
enum {
	IPI_AST,
	IPI_PREEMPT,
	IPI_RENDEZVOUS,
	IPI_STOP,
	IPI_STOP_HARD = IPI_STOP, /* These are synonyms on arm. */
	IPI_HARDCLOCK,
	IPI_TLB,
	IPI_CACHE,
	ARM_IPI_COUNT
};
#else
#define IPI_AST		0
#define IPI_PREEMPT	2
#define IPI_RENDEZVOUS	3
#define IPI_STOP	4
#define IPI_STOP_HARD	4
#define IPI_HARDCLOCK	6
#define IPI_TLB		7
#define IPI_CACHE	8
#endif /* INTRNG */

void	init_secondary(int cpu);
void	mpentry(void);

void	ipi_all_but_self(u_int ipi);
void	ipi_cpu(int cpu, u_int ipi);
void	ipi_selected(cpuset_t cpus, u_int ipi);

/* PIC interface */
void	pic_ipi_send(cpuset_t cpus, u_int ipi);
#ifndef ARM_INTRNG
void	pic_ipi_clear(int ipi);
int	pic_ipi_read(int arg);
#endif

/* Platform interface */
void	platform_mp_setmaxid(void);
int	platform_mp_probe(void);
void	platform_mp_start_ap(void);
void	platform_mp_init_secondary(void);

void	platform_ipi_send(cpuset_t cpus, u_int ipi);

/* global data in mp_machdep.c */
extern struct pcb               stoppcbs[];

#endif /* !_MACHINE_SMP_H_ */
