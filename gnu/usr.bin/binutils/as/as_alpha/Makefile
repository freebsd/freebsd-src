#
# $Id: Makefile,v 1.3 1998/05/05 10:32:47 jb Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "alpha"
PROG=		as
.else
PROG=		as_alpha
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
