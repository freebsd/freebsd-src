#
# $Id: Makefile,v 1.2 1998/05/05 08:57:11 jb Exp $
#

.include "${.CURDIR}/../Makefile.inc0"

.if	${MACHINE_ARCH} == "i386"
PROG=		as
.else
PROG=		as_i386
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
