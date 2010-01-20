/* $FreeBSD$ */

#if defined(AIM)
#include <machine/trap_aim.h>
#elif defined(E500)
#include <machine/trap_booke.h>
#endif

#ifndef LOCORE
struct trapframe;
void    trap(struct trapframe *);
#endif
