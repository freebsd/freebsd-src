#
# $FreeBSD: src/gnu/usr.bin/binutils/as/alpha-freebsd/Makefile,v 1.7.2.1 2000/07/06 22:15:42 obrien Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "alpha"
NOMAN=		1
BINDIR=		/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
