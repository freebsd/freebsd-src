/*-
 * Copyright (c) 1982, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)systm.h	8.7 (Berkeley) 3/29/95
 * $FreeBSD$
 */

#ifndef _SYS_SYSTM_H_
#define	_SYS_SYSTM_H_

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <sys/callout.h>

extern int securelevel;		/* system security level (see init(8)) */
extern int suser_enabled;	/* suser_xxx() is permitted to return 0 */

extern int cold;		/* nonzero if we are doing a cold boot */
extern const char *panicstr;	/* panic message */
extern int dumping;		/* system is dumping */
extern char version[];		/* system version */
extern char copyright[];	/* system copyright */

extern int nswap;		/* size of swap space */

extern struct cv selwait;	/* select conditional variable */

extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern long dumplo;		/* offset into dumpdev */

extern dev_t rootdev;		/* root device */
extern dev_t rootdevs[2];	/* possible root devices */
extern char *rootdevnames[2];	/* names of possible root devices */
extern struct vnode *rootvp;	/* vnode equivalent to above */

extern int boothowto;		/* reboot flags, from console subsystem */
extern int bootverbose;		/* nonzero to print verbose messages */

extern int maxusers;		/* system tune hint */

#ifdef	INVARIANTS		/* The option is always available */
#define	KASSERT(exp,msg)	do { if (!(exp)) panic msg; } while (0)
#else
#define	KASSERT(exp,msg)
#endif

/*
 * XXX the hints declarations are even more misplaced than most declarations
 * in this file, since they are needed in one file (per arch) and only used
 * in two files.
 * XXX most of these variables should be const.
 */
extern int envmode;
extern int hintmode;		/* 0 = off. 1 = config, 2 = fallback */
extern char *kern_envp;
extern char static_env[];
extern char static_hints[];	/* by config for now */

/*
 * General function declarations.
 */

struct clockframe;
struct malloc_type;
struct mtx;
struct proc;
struct kse;
struct thread;
struct tty;
struct ucred;
struct uio;
struct _jmp_buf;

int	setjmp __P((struct _jmp_buf *));
void	longjmp __P((struct _jmp_buf *, int)) __dead2;
void	Debugger __P((const char *msg));
int	dumpstatus __P((vm_offset_t addr, off_t count));
int	nullop __P((void));
int	eopnotsupp __P((void));
int	seltrue __P((dev_t dev, int which, struct thread *td));
int	ureadc __P((int, struct uio *));
void	*hashinit __P((int count, struct malloc_type *type, u_long *hashmask));
void	*phashinit __P((int count, struct malloc_type *type, u_long *nentries));

void	cpu_boot __P((int));
void	cpu_rootconf __P((void));
void	critical_enter __P((void));
void	critical_exit __P((void));
void	init_param1 __P((void));
void	init_param2 __P((int physpages));
void	tablefull __P((const char *));
int	kvprintf __P((char const *, void (*)(int, void*), void *, int,
		      _BSD_VA_LIST_)) __printflike(1, 0);
void	log __P((int, const char *, ...)) __printflike(2, 3);
void	log_console __P((struct uio *));
int	printf __P((const char *, ...)) __printflike(1, 2);
int	snprintf __P((char *, size_t, const char *, ...)) __printflike(3, 4);
int	sprintf __P((char *buf, const char *, ...)) __printflike(2, 3);
int	uprintf __P((const char *, ...)) __printflike(1, 2);
int	vprintf __P((const char *, _BSD_VA_LIST_)) __printflike(1, 0);
int	vsnprintf __P((char *, size_t, const char *, _BSD_VA_LIST_)) __printflike(3, 0);
int     vsprintf __P((char *buf, const char *, _BSD_VA_LIST_)) __printflike(2, 0);
int	ttyprintf __P((struct tty *, const char *, ...)) __printflike(2, 3);
int	sscanf __P((const char *, char const *, ...));
int	vsscanf __P((const char *, char const *, _BSD_VA_LIST_));
long	strtol __P((const char *, char **, int));
u_long	strtoul __P((const char *, char **, int));
quad_t	strtoq __P((const char *, char **, int));
u_quad_t strtouq __P((const char *, char **, int));
void	tprintf __P((struct proc *p, int pri, const char *, ...)) __printflike(3, 4);

void	bcopy __P((const void *from, void *to, size_t len));
void	ovbcopy __P((const void *from, void *to, size_t len));

#ifdef __i386__
extern void	(*bzero) __P((volatile void *buf, size_t len));
#else
void	bzero __P((void *buf, size_t len));
#endif

void	*memcpy __P((void *to, const void *from, size_t len));

int	copystr __P((const void *kfaddr, void *kdaddr, size_t len,
		size_t *lencopied));
int	copyinstr __P((const void *udaddr, void *kaddr, size_t len,
		size_t *lencopied));
int	copyin __P((const void *udaddr, void *kaddr, size_t len));
int	copyout __P((const void *kaddr, void *udaddr, size_t len));

int	fubyte __P((const void *base));
int	subyte __P((void *base, int byte));
int	suibyte __P((void *base, int byte));
long	fuword __P((const void *base));
int	suword __P((void *base, long word));
int	fusword __P((void *base));
int	susword __P((void *base, int word));

void	realitexpire __P((void *));

void	hardclock __P((struct clockframe *frame));
void	hardclock_process __P((struct thread *td, int user));
void	softclock __P((void *));
void	statclock __P((struct clockframe *frame));
void	statclock_process __P((struct kse *ke, register_t pc, int user));

void	startprofclock __P((struct proc *));
void	stopprofclock __P((struct proc *));
void	setstatclockrate __P((int hzrate));

/* flags for suser_xxx() */
#define PRISON_ROOT	1

int	suser __P((struct proc *));
int	suser_td __P((struct thread *));
int	suser_xxx __P((struct ucred *cred, struct proc *proc, int flag));
int	suser_xxx_td __P((struct ucred *cred, struct thread *thread, int flag));
int	cr_cansee __P((struct ucred *u1, struct ucred *u2));

char	*getenv __P((const char *name));
int	getenv_int __P((const char *name, int *data));
int	getenv_string __P((const char *name, char *data, int size));
int	getenv_quad __P((const char *name, quad_t *data));

#ifdef APM_FIXUP_CALLTODO 
struct timeval;
void	adjust_timeout_calltodo __P((struct timeval *time_change)); 
#endif /* APM_FIXUP_CALLTODO */ 

#include <sys/libkern.h>

/* Initialize the world */
void	consinit __P((void));
void	cpu_initclocks __P((void));
void	usrinfoinit __P((void));

/* Finalize the world. */
void	shutdown_nice __P((int));

/*
 * Kernel to clock driver interface.
 */
void	inittodr __P((time_t base));
void	resettodr __P((void));
void	startrtclock __P((void));

/* Timeouts */
typedef void timeout_t __P((void *));	/* timeout function type */
#define CALLOUT_HANDLE_INITIALIZER(handle)	\
	{ NULL }

void	callout_handle_init __P((struct callout_handle *));
struct	callout_handle timeout __P((timeout_t *, void *, int));
void	untimeout __P((timeout_t *, void *, struct callout_handle));
caddr_t	kern_timeout_callwheel_alloc __P((caddr_t v));
void	kern_timeout_callwheel_init __P((void));

/* Stubs for obsolete functions that used to be for interrupt  management */
static __inline void		spl0(void)		{ return; }
static __inline intrmask_t	splbio(void)		{ return 0; }
static __inline intrmask_t	splcam(void)		{ return 0; }
static __inline intrmask_t	splclock(void)		{ return 0; }
static __inline intrmask_t	splhigh(void)		{ return 0; }
static __inline intrmask_t	splimp(void)		{ return 0; }
static __inline intrmask_t	splnet(void)		{ return 0; }
static __inline intrmask_t	splsoftcam(void)	{ return 0; }
static __inline intrmask_t	splsoftclock(void)	{ return 0; }
static __inline intrmask_t	splsofttty(void)	{ return 0; }
static __inline intrmask_t	splsoftvm(void)		{ return 0; }
static __inline intrmask_t	splsofttq(void)		{ return 0; }
static __inline intrmask_t	splstatclock(void)	{ return 0; }
static __inline intrmask_t	spltty(void)		{ return 0; }
static __inline intrmask_t	splvm(void)		{ return 0; }
static __inline void		splx(intrmask_t ipl)	{ return; }

/*
 * Various callout lists.
 */

/* Exit callout list declarations. */
typedef void (*exitlist_fn) __P((struct proc *procp));

int	at_exit __P((exitlist_fn function));
int	rm_at_exit __P((exitlist_fn function));

/* Fork callout list declarations. */
typedef void (*forklist_fn) __P((struct proc *parent, struct proc *child,
				 int flags));

int	at_fork __P((forklist_fn function));
int	rm_at_fork __P((forklist_fn function));

/* Exec callout list declarations. */
typedef void (*execlist_fn) __P((struct proc *procp));

int	at_exec __P((execlist_fn function));
int	rm_at_exec __P((execlist_fn function));

/*
 * Not exactly a callout LIST, but a callout entry.
 * Allow an external module to define a hardware watchdog tickler.
 * Normally a process would do this, but there are times when the
 * kernel needs to be able to hold off the watchdog, when the process
 * is not active, e.g., when dumping core.
 */
typedef void (*watchdog_tickle_fn) __P((void));

extern watchdog_tickle_fn	wdog_tickler;

/* 
 * Common `proc' functions are declared here so that proc.h can be included
 * less often.
 */
int	msleep __P((void *chan, struct mtx *mtx, int pri, const char *wmesg,
		    int timo));
#define	tsleep(chan, pri, wmesg, timo)	msleep(chan, NULL, pri, wmesg, timo)
void	wakeup __P((void *chan));
void	wakeup_one __P((void *chan));

/*
 * Common `dev_t' stuff are declared here to avoid #include poisoning
 */

int major(dev_t x);
int minor(dev_t x);
dev_t makedev(int x, int y);
udev_t dev2udev(dev_t x);
dev_t udev2dev(udev_t x, int b);
int uminor(udev_t dev);
int umajor(udev_t dev);
udev_t makeudev(int x, int y);

/* XXX: Should be void nanodelay(u_int nsec); */
void	DELAY __P((int usec));

#endif /* !_SYS_SYSTM_H_ */
