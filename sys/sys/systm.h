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
 *	$Id: systm.h,v 1.7 1993/10/08 20:59:39 rgrimes Exp $
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

extern struct sysent {		/* system call table */
	int	sy_narg;	/* number of arguments */
	int	(*sy_call)();	/* implementing function */
} sysent[];

/* Prototypes I needed to fix that kern_exit warning
    ---- this really the first step in the work that's 
         been done on sun-lamp to add kernel function
         prototypes.                                 */
void	kexit __P((struct proc *, int));
void	cpu_exit __P((struct proc *));
void    swtch __P((void));


extern const char *panicstr;	/* panic message */
extern char version[];		/* system version */
extern char copyright[];	/* system copyright */

extern int nblkdev;		/* number of entries in bdevsw */
extern int nchrdev;		/* number of entries in cdevsw */
extern int nswdev;		/* number of swap devices */
extern int nswap;		/* size of swap space */

extern int selwait;		/* select timeout address */

extern u_char curpri;		/* priority of current process */

extern int maxmem;		/* max memory per process */
extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern long dumplo;		/* offset into dumpdev */

extern dev_t rootdev;		/* root device */
extern struct vnode *rootvp;	/* vnode equivalent to above */

extern dev_t swapdev;		/* swapping device */
extern struct vnode *swapdev_vp;/* vnode equivalent to above */

extern int boothowto;		/* reboot flags, from console subsystem */
#ifdef	KADB
extern char *bootesym;		/* end of symbol info from boot */
#endif

/* casts to keep lint happy */
#define	insque(q,p)	_insque((caddr_t)q,(caddr_t)p)
#define	remque(q)	_remque((caddr_t)q)

/*
 * General function declarations.
 */


int	nullop __P((void));
int	enodev __P((void));
int	enoioctl __P((void));
int	enxio __P((void));
int	eopnotsupp __P((void));
int	selscan __P((struct proc *p, fd_set *ibits, fd_set *obits,
		int nfd, int *retval));
int	seltrue __P((dev_t dev, int which, struct proc *p));
void	selwakeup  __P((pid_t pid, int coll));

__dead void	panic __P((const char *));
void	tablefull __P((char *));
int	addlog __P((const char *, ...));
void	log __P((int, const char *, ...));
int	printf __P((const char *, ...));
int	sprintf __P((char *buf, const char *, ...));
void	ttyprintf __P((struct tty *, const char *, ...));

void	bcopy __P((void *from, void *to, u_int len));
void	ovbcopy __P((void *from, void *to, u_int len));
void	bzero __P((void *buf, u_int len));
static int	bcmp __P((void *str1, void *str2, u_int len));
static size_t	strlen __P((const char *string));

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

int	scanc __P((unsigned size, u_char *cp, u_char *table, int mask));
int	skpc __P((int mask, int size, char *cp));
int	locc __P((int mask, char *cp, unsigned size));
static int	ffs __P((long value));

/*
 * XXX - a lot were missing.  A lot are still missing.  Some of the above
 * are inconsistent with ANSI C.  I fixed strlen.  Others are inconsistent
 * with with non-ANSI C due to having unpromoted args.
 */
#define	nonint	int		/* really void */
struct	proc;
struct	ringb;
struct	speedtab;

typedef	nonint	(*timeout_func_t)	__P((caddr_t arg, int ticks));

nonint	DELAY		__P((int count));
int	getc		__P((struct ringb *rbp));
void	psignal		__P((struct proc *p, int sig));
size_t	rb_write	__P((struct ringb *to, char *buf, size_t nfrom));
void	spinwait	__P((int millisecs));
int	splhigh		__P((void));
int	spltty		__P((void));
int	splx		__P((int new_pri));
#ifdef notyet
nonint	timeout		__P((timeout_func_t func, caddr_t arg, int t));
#endif
void	trapsignal	__P((struct proc *p, int sig, unsigned code));
int	ttioctl		__P((struct tty *tp, int com, caddr_t data, int flag));
nonint	ttsetwater	__P((struct tty *tp));
nonint	ttstart		__P((struct tty *tp));
nonint	ttychars	__P((struct tty *tp));
int	ttyclose	__P((struct tty *tp));
nonint	ttyinput	__P((int c, struct tty *tp));
int	ttysleep	__P((struct tty *tp, caddr_t chan, int pri,
			     char *wmesg, int timo));
int	ttspeedtab	__P((int speed, struct speedtab *table));
nonint	ttwakeup	__P((struct tty *tp));
#ifdef notyet
nonint	wakeup		__P((caddr_t chan));
#endif

#undef	nonint

/*
 * Machine-dependent function declarations.
 */
#include <machine/cpufunc.h>
#endif /* _SYS_SYSTM_H_ */
