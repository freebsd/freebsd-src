#
# $FreeBSD: src/gnu/usr.bin/binutils/as/sparc-freebsd/Makefile,v 1.3.2.1 2000/07/06 22:15:48 obrien Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

PROG=		as
.if	${TARGET_ARCH} != "sparc"
NOMAN=		1
BINDIR=		/usr/libexec/cross/sparc-freebsd
# If we aren't doing cross-assembling, some operations can be optimized, since
# byte orders and value sizes don't need to be adjusted.
CFLAGS+=	-DCROSS_COMPILE=1
.endif
SRCS+=		obj-elf.c tc-sparc.c

.include <bsd.prog.mk>
