# $FreeBSD: src/gnu/usr.bin/binutils/as/i386-freebsd/Makefile,v 1.10 2003/08/14 14:43:29 ru Exp $

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "i386"
NOMAN=	true
BINDIR=	/usr/libexec/cross/i386-freebsd
.endif
SRCS+=	obj-elf.c tc-i386.c

.include <bsd.prog.mk>
