#	$Id: Makefile.inc,v 1.7 1997/05/03 03:57:21 jb Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	__error.c _atomic_lock.S

