/* $FreeBSD: src/sys/powerpc/include/trap.h,v 1.6.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

#if defined(AIM)
#include <machine/trap_aim.h>
#elif defined(E500)
#include <machine/trap_booke.h>
#endif

#ifndef LOCORE
struct trapframe;
void    trap(struct trapframe *);
#endif
