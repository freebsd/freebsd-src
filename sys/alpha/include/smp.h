/*
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

#include <machine/mutex.h>
#include <machine/ipl.h>
#include <sys/ktr.h>

#ifndef LOCORE

#define BETTER_CLOCK		/* unconditional on alpha */

/* global data in mp_machdep.c */
extern volatile u_int		checkstate_probed_cpus;
extern volatile u_int		checkstate_need_ast;
extern volatile u_int		resched_cpus;
extern void (*cpustop_restartfunc) __P((void));

extern int			smp_active;
extern int			mp_ncpus;
extern u_int			all_cpus;
extern u_int			started_cpus;
extern u_int			stopped_cpus;

/* functions in mp_machdep.c */
void	mp_start(void);
void	mp_announce(void);
void	smp_invltlb(void);
void	forward_statclock(int pscnt);
void	forward_hardclock(int pscnt);
void	forward_signal(struct proc *);
void	forward_roundrobin(void);
int	stop_cpus(u_int);
int	restart_cpus(u_int);
void	smp_rendezvous_action(void);
void	smp_rendezvous(void (*)(void *), 
		       void (*)(void *),
		       void (*)(void *),
		       void *arg);
void	smp_init_secondary(void);

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
