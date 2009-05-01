# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
