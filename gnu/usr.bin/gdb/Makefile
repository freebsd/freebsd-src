# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12.10.1 2010/02/10 00:26:20 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
