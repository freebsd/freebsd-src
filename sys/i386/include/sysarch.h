/*
 * Architecture specific syscalls (i386)
 *
 *	$Id: sysarch.h,v 1.2 1993/10/16 14:39:35 rgrimes Exp $
 */
#define I386_GET_LDT	0
#define I386_SET_LDT	1

#ifndef KERNEL
int i386_get_ldt __P((int, union descriptor *, int));
int i386_set_ldt __P((int, union descriptor *, int));
#endif
