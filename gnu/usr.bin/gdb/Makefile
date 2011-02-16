# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12.2.1.2.1 2010/12/21 17:10:29 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "arm"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
