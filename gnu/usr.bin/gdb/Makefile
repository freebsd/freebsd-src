# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.14.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
