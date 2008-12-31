# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
