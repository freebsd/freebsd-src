#
# $FreeBSD: src/gnu/usr.bin/binutils/as/sparc-freebsd/Makefile,v 1.3 1999/12/17 15:50:44 marcel Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "sparc"
NOMAN=		1
BINDIR=		/usr/libexec/cross/sparc-freebsd
.endif
SRCS+=		obj-elf.c tc-sparc.c

.include <bsd.prog.mk>
