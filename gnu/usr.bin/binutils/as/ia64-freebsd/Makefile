# $FreeBSD: src/gnu/usr.bin/binutils/as/ia64-freebsd/Makefile,v 1.2 2002/12/05 18:24:34 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

WARNS=	0

.if ${TARGET_ARCH} != "ia64"
NOMAN=	 true
BINDIR=	/usr/libexec/cross/ia64-freebsd
.endif
SRCS+=	obj-elf.c tc-ia64.c

.include <bsd.prog.mk>
