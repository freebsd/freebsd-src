# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.9.8.1.2.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
