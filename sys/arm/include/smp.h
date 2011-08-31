/* $FreeBSD$ */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#include <sys/_cpuset.h>

#define IPI_AST		0
#define IPI_PREEMPT	2
#define IPI_RENDEZVOUS	3
#define IPI_STOP	4
#define IPI_STOP_HARD	5
#define IPI_HARDCLOCK	6

void	init_secondary(int cpu);

void	ipi_all_but_self(u_int ipi);
void	ipi_cpu(int cpu, u_int ipi);
void	ipi_selected(cpuset_t cpus, u_int ipi);

/* Platform interface */
void	platform_mp_setmaxid(void);
int	platform_mp_probe(void);
int	platform_mp_start_ap(int cpuid);


#endif /* !_MACHINE_SMP_H_ */
