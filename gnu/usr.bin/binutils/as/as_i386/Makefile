#
# $Id$
#

.include "../Makefile.inc0"

.if	${MACHINE_ARCH} == "i386"
PROG=		as
.else
PROG=		as_i386
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
