#
# $Id$
#

.include "../Makefile.inc0"

.if	${MACHINE_ARCH} == "alpha"
PROG=		as
.else
PROG=		as_alpha
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
