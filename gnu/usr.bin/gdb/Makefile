# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.9.18.1 2007/12/03 21:04:54 obrien Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
