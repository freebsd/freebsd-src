# $FreeBSD: src/gnu/usr.bin/binutils/as/alpha-freebsd/Makefile,v 1.8 2001/10/15 01:43:21 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "alpha"
NOMAN=	true
BINDIR=	/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=	obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
