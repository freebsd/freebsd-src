#
# $FreeBSD: src/gnu/usr.bin/binutils/as/i386-freebsd/Makefile,v 1.7.2.2 2001/08/01 22:54:08 obrien Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
NOMAN=		1
.if	${TARGET_ARCH} != "i386"
BINDIR=		/usr/libexec/cross/i386-freebsd
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
