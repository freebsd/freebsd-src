# $FreeBSD: src/gnu/usr.bin/binutils/as/sparc64-freebsd/Makefile,v 1.4 2001/10/15 01:43:23 obrien Exp $

.include "${.CURDIR}/../Makefile.inc0"

.if ${TARGET_ARCH} != "sparc64"
NOMAN=	true
BINDIR=	/usr/libexec/cross/sparc64-freebsd
# If we aren't doing cross-assembling, some operations can be optimized, since
# byte orders and value sizes don't need to be adjusted.
CFLAGS+= -DCROSS_COMPILE=1
.endif
SRCS+=	obj-elf.c tc-sparc.c

.include <bsd.prog.mk>
