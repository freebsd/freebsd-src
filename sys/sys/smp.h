/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 */

#ifndef _SYS_SMP_H_
#define _SYS_SMP_H_

#ifdef _KERNEL
#include <machine/smp.h>

#ifndef LOCORE

#ifdef SMP
extern void (*cpustop_restartfunc)(void);
extern int mp_ncpus;
extern int smp_active;
extern volatile int smp_started;
extern int smp_cpus;
extern u_int all_cpus;
extern volatile u_int started_cpus;
extern volatile u_int stopped_cpus;
extern u_int mp_maxid;

/*
 * Macro allowing us to determine whether a CPU is absent at any given
 * time, thus permitting us to configure sparse maps of cpuid-dependent
 * (per-CPU) structures.
 */
#define	CPU_ABSENT(x_cpu)	((all_cpus & (1 << (x_cpu))) == 0)

/*
 * Machine dependent functions used to initialize MP support.
 *
 * The cpu_mp_probe() should check to see if MP support is present and return
 * zero if it is not or non-zero if it is.  If MP support is present, then
 * cpu_mp_start() will be called so that MP can be enabled.  This function
 * should do things such as startup secondary processors.  It should also
 * setup mp_ncpus, all_cpus, and smp_cpus.  It should also ensure that
 * smp_active and smp_started are initialized at the appropriate time.
 * Once cpu_mp_start() returns, machine independent MP startup code will be
 * executed and a simple message will be output to the console.  Finally,
 * cpu_mp_announce() will be called so that machine dependent messages about
 * the MP support may be output to the console if desired.
 */
void	cpu_mp_announce(void);
int	cpu_mp_probe(void);
void	cpu_mp_start(void);

void	forward_signal(struct thread *);
void	forward_roundrobin(void);
int	restart_cpus(u_int);
int	stop_cpus(u_int);
void	smp_rendezvous_action(void);
void	smp_rendezvous(void (*)(void *), 
		       void (*)(void *),
		       void (*)(void *),
		       void *arg);
#else /* SMP */
#define	CPU_ABSENT(x_cpu)	(0)
#endif /* SMP */
#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* _SYS_SMP_H_ */
