# $FreeBSD: src/gnu/usr.bin/binutils/as/i386-freebsd/Makefile,v 1.7.2.3 2002/09/01 23:39:13 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

NOMAN=	true
.if ${TARGET_ARCH} != "i386"
BINDIR=	/usr/libexec/cross/i386-freebsd
.endif
SRCS+=	obj-elf.c tc-i386.c

.include <bsd.prog.mk>
