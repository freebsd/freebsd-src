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

void	setregs(struct proc *, u_long, u_long, u_long);

#endif /* _POWERPC_REG_H_ */
