#
# $Id: Makefile,v 1.1 1998/05/04 22:04:50 jb Exp $
#

.include "../Makefile.inc0"

# Get the BINDIR for this host:
.include "../../Makefile.inc"

.if	${MACHINE_ARCH} == "alpha"
PROG=		as
.else
PROG=		as_alpha
.endif
SRCS+=		obj-elf.c tc-alpha.c

.include <bsd.prog.mk>
