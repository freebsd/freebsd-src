#
# $FreeBSD: src/gnu/usr.bin/binutils/as/i386-freebsd/Makefile,v 1.7.2.1 2000/07/06 22:15:42 obrien Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "i386"
NOMAN=		1
BINDIR=		/usr/libexec/cross/i386-freebsd
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
