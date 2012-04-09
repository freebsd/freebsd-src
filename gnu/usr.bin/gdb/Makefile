# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.14.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
