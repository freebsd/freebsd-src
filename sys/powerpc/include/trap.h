/* $FreeBSD: src/sys/powerpc/include/trap.h,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */

#if defined(AIM)
#include <machine/trap_aim.h>
#elif defined(E500)
#include <machine/trap_booke.h>
#endif

#ifndef LOCORE
struct trapframe;
void    trap(struct trapframe *);
#endif
