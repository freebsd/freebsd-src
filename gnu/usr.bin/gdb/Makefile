# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12 2006/10/21 17:27:36 jmg Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
