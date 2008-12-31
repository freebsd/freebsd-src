/* $NetBSD: reg.h,v 1.4 2000/06/04 09:30:44 tsubai Exp $	*/
/* $FreeBSD: src/sys/powerpc/include/reg.h,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $	*/

#ifndef _POWERPC_REG_H_
#define	_POWERPC_REG_H_

/* Must match struct trapframe */
struct reg {
	register_t fixreg[32];
	register_t lr;
	int cr;
	int xer;
	register_t ctr;
	register_t pc;
};

/* Must match pcb.pcb_fpu */
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
int	fill_regs(struct thread *, struct reg *);
int	set_regs(struct thread *, struct reg *);
int	fill_fpregs(struct thread *, struct fpreg *);
int	set_fpregs(struct thread *, struct fpreg *);
int	fill_dbregs(struct thread *, struct dbreg *);
int	set_dbregs(struct thread *, struct dbreg *);
#endif

#endif /* _POWERPC_REG_H_ */
