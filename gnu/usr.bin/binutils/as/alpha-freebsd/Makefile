# $FreeBSD: src/gnu/usr.bin/binutils/as/alpha-freebsd/Makefile,v 1.7.2.2 2002/09/01 23:39:13 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "alpha"
NOMAN=	true
BINDIR=	/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=	obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
