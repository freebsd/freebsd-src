#	$Id: Makefile.inc,v 1.9 1998/04/30 09:04:08 jb Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S

