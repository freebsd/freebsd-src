#
# $FreeBSD: src/gnu/usr.bin/binutils/as/alpha-freebsd/Makefile,v 1.7 1999/12/17 15:50:43 marcel Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "alpha"
NOMAN=		1
BINDIR=		/usr/libexec/cross/alpha-freebsd
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
