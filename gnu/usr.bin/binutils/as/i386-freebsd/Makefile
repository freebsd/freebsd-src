#
# $Id: Makefile,v 1.1 1998/05/04 22:05:44 jb Exp $
#

.include "../Makefile.inc0"

# Get the BINDIR for this host:
.include "../../Makefile.inc"

.if	${MACHINE_ARCH} == "i386"
PROG=		as
.else
PROG=		as_i386
.endif
SRCS+=		obj-elf.c tc-i386.c

.include <bsd.prog.mk>
