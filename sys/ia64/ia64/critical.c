/*-
 * Copyright (c) 2001 Matthew Dillon.  This code is distributed under
 * the BSD copyright, /usr/src/COPYRIGHT.
 *
 * $FreeBSD: src/sys/ia64/ia64/critical.c,v 1.3 2002/04/01 23:51:22 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pcpu.h>
#include <sys/eventhandler.h>	/* XX */
#include <sys/ktr.h>		/* XX */
#include <sys/signalvar.h>
#include <sys/sysproto.h>	/* XX */
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/ucontext.h>
#include <machine/critical.h>

/*
 * cpu_critical_fork_exit() - cleanup after fork
 */
void
cpu_critical_fork_exit(void)
{
	struct thread *td;

	td = curthread;
	td->td_critnest = 1;
	td->td_md.md_savecrit = (ia64_get_psr() | IA64_PSR_I);
}

/*
 * cpu_thread_link() - thread linkup, initialize machine-dependant fields
 */
void
cpu_thread_link(struct thread *td)
{

	td->td_md.md_savecrit = 0;
}
