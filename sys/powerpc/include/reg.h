/* $NetBSD: reg.h,v 1.4 2000/06/04 09:30:44 tsubai Exp $	*/
/* $FreeBSD$	*/

#ifndef _POWERPC_REG_H_
#define	_POWERPC_REG_H_

struct reg {
	register_t fixreg[32];
	register_t lr;
	int cr;
	int xer;
	register_t ctr;
	register_t pc;
};

struct fpreg {
	double fpreg[32];
	double fpscr;
};

struct dbreg {
	unsigned long	junk;
};

#ifdef _KERNEL
/*
 * XXX these interfaces are MI, so they should be declared in a MI place.
 */
void	setregs __P((struct thread *, u_long, u_long, u_long));
int	fill_regs __P((struct thread *, struct reg *));
int	set_regs __P((struct thread *, struct reg *));
int	fill_fpregs __P((struct thread *, struct fpreg *));
int	set_fpregs __P((struct thread *, struct fpreg *));
int	fill_dbregs __P((struct thread *, struct dbreg *));
int	set_dbregs __P((struct thread *, struct dbreg *));
#endif

#endif /* _POWERPC_REG_H_ */
