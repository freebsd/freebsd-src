# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.14.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
