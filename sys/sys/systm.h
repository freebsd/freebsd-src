/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*-
 * Copyright (c) 1982, 1988, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)systm.h	7.17 (Berkeley) 5/25/91
 *	$Id: systm.h,v 1.11 1994/05/04 08:31:08 rgrimes Exp $
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

#include "sys/param.h"		/* XXX */
#include "sys/sysent.h"		/* XXX */

/*
 * Machine-dependent function declarations.
 * These must be first in case a machine-dependent function is static
 * [inline].  ANSI C's linkage scope rules require the static version
 * to be visible first.  However, if the machine-dependent functions
 * were actually macros, they would have to be defined last.
 */
#include <machine/cpufunc.h>

/* Initialize the world */
void	startrtclock __P((void));
void	consinit __P((void));
void	vm_mem_init __P((void));
void	kmeminit __P((void));
void	cpu_startup __P((void));
void	rqinit __P((void));
void	vm_init_limits __P((struct proc *));
void	vfsinit __P((void));
void	mbinit __P((void));
void	shminit __P((void));
void	ifinit __P((void));
void	domaininit __P((void));
void	swapinit __P((void));
void	enablertclock __P((void));


/* Default network interfaces... */
void	slattach(void);
void	pppattach(void);
void	loattach(void);


/* select() support functions */
int	selscan __P((struct proc *, fd_set *, fd_set *,	int, int *));
int	seltrue __P((int /*dev_t*/, int, struct proc *));
void	selwakeup  __P((int /*pid_t*/, int));

extern int selwait;		/* select timeout address */


/* Interrupt masking. */
void	spl0 __P((void));
int	splbio __P((void));
int	splclock __P((void));
int	splhigh __P((void));
int	splimp __P((void));
int	splnet __P((void));
#define	splnone spl0		/* XXX traditional; the reverse is better */
int	splsoftclock __P((void));
int	splsofttty __P((void));
int	spltty __P((void));
void	splx __P((int));


/* Scheduling */
void	roundrobin __P((caddr_t, int));
void	schedcpu __P((caddr_t, int));
void	softclock();
void	setsoftclock __P((void));
void	setpri __P((struct proc *));
void	swtch __P((void));
void	vmmeter __P((void));


/* Timeouts and sleeps */
typedef	void (*timeout_func_t)(caddr_t, int);
extern void timeout(timeout_func_t, caddr_t, int);
extern void wakeup(caddr_t);
extern void untimeout(timeout_func_t, caddr_t);
extern int tsleep(caddr_t, int, const char *, int);
extern void wakeup(caddr_t);


/* User data reference */
int	useracc __P((caddr_t, int, int));
int	kernacc __P((caddr_t, int, int));
int	copystr __P((void *kfaddr, void *kdaddr, u_int len, u_int *done));
int	copyinstr __P((void *udaddr, void *kaddr, u_int len, u_int *done));
int	copyoutstr __P((void *kaddr, void *udaddr, u_int len, u_int *done));
int	copyin __P((void *udaddr, void *kaddr, u_int len));
int	copyout __P((void *kaddr, void *udaddr, u_int len));
int	fubyte __P((void *base));
#ifdef notdef
int	fuibyte __P((void *base));
#endif
int	subyte __P((void *base, int byte));
int	suibyte __P((void *base, int byte));
int	fuword __P((void *base));
int	fuiword __P((void *base));
int	suword __P((void *base, int word));
int	suiword __P((void *base, int word));


/* printf() family */
int	printf __P((const char *, ...));
int	sprintf __P((char *, const char *, ...));
void	uprintf __P((const char *, ...));


/* Miscellaneous */
void	logwakeup __P((void));
void	addlog __P((const char *, ...));
void	log __P((int, const char *, ...));
void	tablefull __P((const char *));
void	DELAY		__P((int));
void	spinwait	__P((int));
struct ucred;
int	suser __P((struct ucred *, u_short *));

/* Functions to return errors */
int	nullop __P((void));	/* WARNING WILL ROBINSON */
int	enodev __P((void));	/* All these routines are potentially */
int	enoioctl __P((void));	/* called with differing arguments. */
int	enxio __P((void));
int	eopnotsupp __P((void));


/* routines which never return */
#ifdef __GNUC__
typedef void _sched_t(void);	/* sched() */
__dead _sched_t sched;

typedef void _kexit_t(struct proc *, int); /* kexit() */
__dead _kexit_t kexit;

typedef void _cpu_exit_t(struct proc *); /* cpu_exit() */
__dead _cpu_exit_t cpu_exit;

typedef void _panic_t(const char *); /* panic() */
__dead _panic_t panic;

typedef void _boot_t(int);	/* boot() */
__dead _boot_t boot;

#else
void	panic __P((const char *));
void	sched __P((void));
void	exit __P((struct proc *, int));
void	cpu_exit __P((struct proc *));
void	boot __P((int));
#endif


/* string functions */
int	strcmp __P((const char *, const char *));
char   *strncpy __P((char *, const char *, int));
char   *strcat __P((char *, const char *));
char   *strcpy __P((char *, const char *));
void	bcopy __P((const void *from, void *to, u_int len));
void	ovbcopy __P((void *from, void *to, u_int len));
void	bzero __P((void *, u_int));
#ifndef __GNUC__
int	bcmp __P((const void *str1, const void *str2, u_int len));
#endif
int	scanc __P((unsigned size, u_char *cp, u_char *table, int mask));
int	skpc __P((int, u_int, u_char *));
int	locc __P((int, unsigned, u_char *));

/* Debugger entry points */
void	Debugger __P((const char *));

#endif /* _SYS_SYSTM_H_ */
