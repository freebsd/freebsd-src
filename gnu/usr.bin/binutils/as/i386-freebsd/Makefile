# $FreeBSD: src/gnu/usr.bin/binutils/as/i386-freebsd/Makefile,v 1.9 2001/10/15 01:43:22 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

NOMAN=	true
.if ${TARGET_ARCH} != "i386"
BINDIR=	/usr/libexec/cross/i386-freebsd
.endif
SRCS+=	obj-elf.c tc-i386.c

.include <bsd.prog.mk>
