/*	$NetBSD: trap.h,v 1.1 2001/02/23 03:48:19 ichiro Exp $	*/
/* $FreeBSD: src/sys/arm/include/trap.h,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $ */

#ifndef _MACHINE_TRAP_H_
#define _MACHINE_TRAP_H_
#define GDB_BREAKPOINT		0xe6000011
#define GDB5_BREAKPOINT		0xe7ffdefe
#define PTRACE_BREAKPOINT	0xe7fffff0
#define KERNEL_BREAKPOINT	0xe7ffffff
#endif /* _MACHINE_TRAP_H_ */
