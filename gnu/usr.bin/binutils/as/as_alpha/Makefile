#
# $FreeBSD: src/gnu/usr.bin/binutils/as/as_alpha/Makefile,v 1.3.2.1 1999/08/29 14:31:57 peter Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "alpha"
PROG=		as
.else
PROG=		as_alpha
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
