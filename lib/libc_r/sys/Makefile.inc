#	$Id: Makefile.inc,v 1.8 1998/04/29 11:03:34 jb Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S

