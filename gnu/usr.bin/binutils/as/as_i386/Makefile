#
# $FreeBSD: src/gnu/usr.bin/binutils/as/as_i386/Makefile,v 1.3.2.1 1999/08/29 14:31:58 peter Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "i386"
PROG=		as
.else
PROG=		as_i386
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
