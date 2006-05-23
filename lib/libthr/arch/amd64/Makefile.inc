#$FreeBSD: src/lib/libthr/arch/amd64/Makefile.inc,v 1.1.2.1 2004/10/09 15:07:19 mtm Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_setcurthread.c
